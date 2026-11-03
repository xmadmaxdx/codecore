# Architecture

CodeCore v0.3 is the engine foundation. It is deliberately layered so a Flutter
UI, a Kotlin Android UI or a native host can share document behavior without
sharing C++ implementation details.

```text
Host application (Flutter / Kotlin / C / C++)
        │
        ├── Dart FFI  ──┐
        ├── JNI ────────┤
        └── C ABI ──────┤
                        ▼
              native/include/codecore/codecore.h
                        │
                        ▼
                 core::Engine (language registry)
                        │
                        ▼
                 core::Document
                 ├── TextBuffer (piece table)
                 ├── incremental LineIndex
                 ├── undo / redo records + edit origin
                 ├── selection + cursor
                 ├── proposed text + line diff
                 ├── ghost-text state
                 ├── Analysis cache (per version)
                 ├── Syntax tree (Tree-sitter, per version)
                 └── Snapshot (immutable text + analysis)
```

## Text buffer

`TextBuffer` is a piece table:

- The original document text is immutable.
- Inserted text is appended to a growable add buffer.
- The document is a sequence of pieces referencing the original or add buffer.
- Edits split pieces at boundaries and merge adjacent contiguous pieces.
- The add buffer is compacted once it exceeds both 4 MB and the original size.

This avoids copying the whole document on every keystroke. Materializing the
whole document is still O(n).

## Line index and positions

`LineIndex` stores line-start offsets only. It is rebuilt from the text on a full
set and updated on each edit by removing starts inside the replaced range,
shifting later starts and inserting starts for new newlines.

- `lineOfOffset` is binary search.
- `line(index)` derives `contentEnd` from the current text, excluding `\r\n` or
  `\n`.
- Offset-to-position uses code-point counting from the line start.
- Position-to-offset clamps at `contentEnd`, so columns address line content,
  not line terminators.

The update is O(number of lines) per edit, not O(1); incremental tokenization and
a tree-based line index remain future work.

## Analysis cache and snapshots

`Analysis` owns a text copy, a line index and lazily computed lexer, diagnostics
and folding results. It is immutable after construction.

- `Document` caches one analysis per text version.
- Repeated tokenize/diagnose/folding/find calls on an unchanged version reuse it.
- Any edit resets the cache.
- `Snapshot` carries text, language and version and has its own analysis cache.
  It remains valid after the document changes or is destroyed, so hosts can run
  queries on a worker thread without holding the document lock.

Snapshots copy text. They are not structural sharing.

## Document model

`Document` owns the buffer, language pointer, selection, history, proposal,
ghost text and analysis cache.

- Offsets are UTF-8 byte offsets. Edits validate code-point boundaries and UTF-8.
- Lines are `{start, contentEnd, end}`.
- Every text mutation increments a version and emits a change event.
- Read-only mode blocks all text mutations; selection, navigation, querying and
  proposal preview remain available.

## Undo, redo and edit origins

Each edit records its offset, removed text, inserted text and `EditOrigin`.

- Adjacent single-character typing coalesces for up to one second when the origin
  is unchanged, capped at 256 bytes.
- Erases are discrete.
- New edits clear redo.
- History defaults to 200 entries; 0 disables it.
- Undo/redo events carry the origin of the record being applied.
- Proposal acceptance and snapshot restoration compute a minimal replacement
  range and record one history entry, so each is one undo step.

## Multi-cursor

`Document` stores a vector of selections; index 0 is the primary selection and
`setSelection` resets the list to one entry. `addCursor` appends a validated
selection, and `moveCursors` computes each target independently.

- `multiInsert` replaces every selection with the same text. Overlapping
  selections merge first; duplicate collapsed carets insert once.
- `multiErase` erases each selection, or one code point per collapsed caret,
  with `backward` selecting backspace versus delete.
- Edits apply from the highest offset down, so lower offsets remain valid. The
  history record stores every primitive operation in one group, which makes
  undo and redo a single transaction.
- Carets are recomputed in ascending order after the edit. Change listeners see
  one event per affected range at the same version.

## Proposed changes

A proposal is stored separately from document text.

- `setProposedText` validates UTF-8 and size but does not mutate the document.
- `proposalDiff` computes a line diff between current text and proposed text.
- `acceptProposal` applies the proposal as a single replacement, records the
  proposal's origin (AI by default) and emits `proposal_accepted`.
- `rejectProposal` clears the preview and emits `proposal_rejected`.
- Normal document edits clear a stale proposal.

`core/diff` uses a bounded LCS table. Inputs beyond 4,000,000 cells or 20,000
lines fall back to a correct but coarse delete-all/insert-all diff.

## Ghost text

Ghost text is a host-provided inline suggestion with an offset, text and origin.
It is display state only.

- Setting or reading ghost text never changes document text or version.
- Acceptance inserts all or a leading code-point count as one undoable edit with
  the suggestion's origin.
- Any edit, undo or redo clears ghost text.
- Read-only documents can display ghost text but cannot accept it.

No AI provider is implemented. The host supplies suggestions.

## Language layer

`LanguageRegistry` builds 40 immutable language definitions once per engine.
Documents hold a `shared_ptr<const Language>`, so they remain valid if the engine
is destroyed.

The lexer is a table-driven, single-pass tokenizer for comments, strings,
numbers, keywords, types, builtins, constants, annotations, function calls,
HTML/XML tags and CSS properties.

The lexer is explicitly **not** Tree-sitter. It does not build a syntax tree
or understand scopes or types. Capability flags advertise syntax, completion,
diagnostics and folding only. Structural parsing is a separate layer and is
bundled for Python, JavaScript, Java, Go, JSON, C, C++ and Rust.

## Language services

- **Completion** merges keywords, types, builtins and document symbols, filters
  by identifier prefix and ranks by kind, occurrence count and proximity. It is
  suppressed inside comments and strings and capped at 100.
- **Diagnostics** cover unmatched/unclosed brackets, unterminated strings and
  unterminated block comments.
- **Folding** covers brace and multi-line block-comment ranges.
- **Find** is literal by default, with optional case, whole-word and
  ECMAScript-regex modes. Search returns a status so malformed patterns are
  reported instead of looking like zero matches.

Analysis is per-version, not incremental. Snapshots move it off the document
lock, but do not make tokenization incremental.

## Structural syntax (Tree-sitter)

`core/syntax` wraps a vendored Tree-sitter runtime and a table of bundled
grammars. The table is the extension point: `core/syntax/syntax.cpp` maps
canonical ids and aliases to a generated parser and a highlight query header.
Adding a language requires a vendored grammar, one table row, one query header
and one line in each CMake source list; nothing above this layer changes.

Current table:

| Grammar | Accepted ids | Root | Query |
|---|---|---|---|
| Python | `python`, `py`, `python3` | `module` | `python_highlights.h` |
| JavaScript (incl. JSX) | `javascript`, `js`, `jsx`, `mjs`, `cjs` | `program` | `javascript_highlights.h` |
| Java | `java` | `program` | `java_highlights.h` |
| Go | `go`, `golang` | `source_file` | `go_highlights.h` |
| JSON | `json` | `document` | `json_highlights.h` |
| C | `c`, `h` | `translation_unit` | `c_highlights.h` |
| C++ | `cpp`, `c++`, `cc`, `cxx`, `hpp` | `translation_unit` | `cpp_highlights.h` |
| Rust | `rust`, `rs` | `source_file` | `rust_highlights.h` |

Aliases resolve to the canonical id, so a tree parsed from `js` reports
`javascript`. Unknown ids return no tree rather than a fabricated one.

- `SyntaxTree::parse` builds a tree from a full text buffer.
- `SyntaxTree::reparse` takes the previous immutable tree, derives one byte
  edit from the old and new text (common prefix/suffix), copies the tree,
  applies `ts_tree_edit` and reparses. Tree-sitter reuses unchanged subtrees.
- `Document` caches the tree by version and language, so repeated
  `syntaxTree()` calls on one version do not parse again.
- Highlights come from a bundled, predicate-free query per grammar.
  Overlapping captures are deduplicated by specificity (keyword, comment,
  string, number, function, type, property, variable, operator, punctuation).
- Errors are `ERROR` and `MISSING` nodes, capped at 500 per query.
- Trees are immutable and safe to read across threads; query cursors are
  created per call.

The parser does not replace the lexer. The remaining 32 language definitions
continue to use `core/language`. There is no grammar loader, no download path
and no public Tree-sitter query API. Grammar additions are compiled in, not
downloaded, until a grammar-pack mechanism exists.

Query strings are parsed by Tree-sitter at first use. A query that confuses a
named node with an anonymous token fails as a whole and yields no highlights,
while parsing still succeeds. The grammar test suite parses valid and malformed
source for every bundled language so this cannot regress silently.

## Background analysis

`BackgroundAnalyzer` owns one worker thread and a latest-wins queue.

- `submit(snapshot)` replaces any pending snapshot; replacing increments the
  dropped counter. The worker captures the submission generation.
- The worker computes tokens, diagnostics and folding ranges, then publishes
  only if its generation is current and no newer version has been submitted.
- A stale result is discarded, so an older analysis can never overwrite newer
  editor state.
- `takeLatest()` is non-blocking; `waitIdle()` exists for tests and shutdown and
  must not be used on the UI thread.
- C ABI: `cc_analyzer_*`; a completed result is an owned `cc_snapshot*` and can
  be queried with the existing snapshot functions.

This is the only CodeCore-owned thread. Documents and the LSP client remain
caller-threaded.

## Viewport geometry

`visibleLineRange()` is allocation-free and designed to run every frame. Given
line count, scroll position, viewport height, line height and overscan, it
returns the half-open line range that should be laid out and drawn. `lineTop()`
and `contentHeight()` saturate instead of overflowing.

The renderer is not part of this repository yet. The geometry lets a future
Flutter or Android renderer process the visible rows plus overscan instead of
the whole document; per-line layout/render caches and dirty-line invalidation
remain future work.

## LSP client

`core/lsp` is independent of the document layer. It provides:

- `json` — a small JSON value, parser and serializer used for LSP payloads.
- `protocol` — `Content-Length` framing and request/notification/response
  classification, with incremental parsing of partial input.
- `client` — the LSP lifecycle: `initialize`/`initialized`, full-text
  `didOpen`/`didChange`/`didClose`/`didSave`, completion, hover, definition,
  signature help, references, document/workspace symbols, rename, formatting,
  code actions, semantic tokens, cancellation, `shutdown`/`exit`, result
  delivery and `publishDiagnostics` capture. Generic JSON requests and
  notifications are supported. Server requests such as
  `workspace/configuration` receive minimal valid replies so a server does not
  stall.

The client is transport-agnostic. It queues outgoing frames and accepts incoming
bytes; the host owns the process, pipe or socket. This keeps the engine usable on
Android, where spawning arbitrary language-server binaries is restricted.

No language servers are bundled. The layer is lexical/semantic infrastructure,
not a replacement for the CodeCore document model or completion engine.

## Local execution

`core/exec` adds one child process per `ExecutionSession`. It is deliberately
independent of documents, analysis and the LSP client: the host supplies an
executable, CodeCore owns only the process plumbing.

- `ExecutionSession::start` creates three pipes (stdin, stdout, stderr), spawns
  the child, and starts two reader threads plus one waiter thread.
- Readers enqueue `ExecutionEvent` chunks behind a mutex. The queue is bounded
  by `maxQueuedOutputBytes` (default 1 MiB); when full, readers block, which
  propagates backpressure through the OS pipe instead of growing memory.
- `poll()` drains output in enqueue order and emits the exit event only after
  both readers have finished. Per-stream order is exact; cross-stream
  interleaving is enqueue order.
- `wait()` uses a condition variable; the waiter thread checks cancellation and
  timeout in short slices, terminates the process tree, then records the exit
  code and state.
- Windows uses `CreateProcessW` with an explicit inheritable-handle list, a
  job object with kill-on-close, and pipes. POSIX uses `fork`/`execve` with
  pre-built argv/envp, a process group, and `waitpid(WNOHANG)` polling. Only
  async-signal-safe calls run in the POSIX child.
- Environment overrides are merged into a copy of the inherited environment;
  the child never receives a partially constructed block.
- Shell mode is explicit and passes the command verbatim to `cmd.exe` or
  `/bin/sh`; ordinary execution never interpolates a shell.

No compiler, interpreter or runtime is linked into the library. On Android,
app-data executables are blocked by platform policy, so runtimes must be
packaged as native libraries or provided by the host; the execution API does
not work around that restriction.

The C ABI exposes the same surface as `cc_execution_*`. Dart and Kotlin wrap it
as `CodeCoreExecution`; both wrappers poll the same events and own the same
native handle. The native session is fully usable without either SDK.

## C ABI

`native/include/codecore/codecore.h` is the only supported native boundary.

- Engine, document and snapshot handles are opaque.
- Returned arrays and strings are allocated through `cc_alloc` and freed with the
  matching `cc_*_free`.
- C++ types and exceptions never cross the boundary.
- The ABI is versioned separately from the library. Current: ABI `6`, library
  `0.5.0`.

## Threading

- Language metadata is immutable after engine construction.
- A document serializes calls with a recursive mutex.
- Change listeners run synchronously on the mutating thread while the document is
  locked. They must not destroy the document or block.
- Snapshots are immutable and may be queried from any thread.
- `BackgroundAnalyzer` uses one worker thread, a latest-wins queue and
  generation-based stale-result rejection. Its completion callback is polling;
  it never touches UI state.
- A document's cached `SyntaxTree` is protected by the document mutex. Once
  returned, the tree is immutable; highlight/error queries create their own
  Tree-sitter query cursors and do not lock the document.
- Dart callbacks are isolate-local; Kotlin callbacks run on the mutating thread.

## Platform SDKs

- **Dart** (`sdk/flutter`): hand-written `dart:ffi`; no package dependencies.
  The package includes an Android Gradle/CMake plugin that builds
  `libcodecore.so` from the shared sources. `SyntaxTree` exposes structural
  highlights and syntax errors for bundled grammars. `CodeCoreExecution`
  streams local process output. `CodeCoreLsp` pumps LSP frames without a
  bundled transport. Publishing needs the native sources vendored into the
  package or a prebuilt AAR.
- **Kotlin** (`sdk/kotlin`): JNI bridge with explicit UTF-8/UTF-16 conversion.
  The Gradle wrapper and NDK version are included. `:assembleDebug` compiles the
  Kotlin SDK and native sources into a debug AAR. `SyntaxTree` exposes
  structural highlights and syntax errors for bundled grammars.
  `CodeCoreExecution` streams local process output. `CodeCoreLsp` exposes the
  same protocol surface; the JNI bridge is smoke-tested through a Java harness.

## Why these choices

- **Piece table** gives cheap inserts and append-only edit storage.
- **Incremental line starts** make position updates cheaper than rescanning text.
- **Per-version analysis** avoids re-lexing unchanged documents.
- **Immutable snapshots** let hosts parallelize analysis safely.
- **Latest-wins background analysis** keeps full scans off the caller's thread
  without publishing stale results.
- **Visible-range geometry** keeps rendering work proportional to the viewport,
  not the document.
- **Tree-sitter for supported languages** gives a real syntax tree and
  incremental reparse without making every language pay for a grammar.
- **C ABI** keeps Dart and Kotlin independent of C++ ABI details.
- **Lexical Level 1** delivers useful highlighting for all 40 definitions
  without grammar binaries; Tree-sitter adds structural parsing where a grammar
  is compiled in.
