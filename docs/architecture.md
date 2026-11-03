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

`LanguageRegistry` builds 23 immutable language definitions once per engine.
Documents hold a `shared_ptr<const Language>`, so they remain valid if the engine
is destroyed.

The lexer is a table-driven, single-pass tokenizer for comments, strings,
numbers, keywords, types, builtins, constants, annotations, function calls,
HTML/XML tags and CSS properties.

It is explicitly **not** Tree-sitter. It does not build a syntax tree or
understand scopes or types. Capability flags advertise syntax, completion,
diagnostics and folding only.

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

## C ABI

`native/include/codecore/codecore.h` is the only supported native boundary.

- Engine, document and snapshot handles are opaque.
- Returned arrays and strings are allocated through `cc_alloc` and freed with the
  matching `cc_*_free`.
- C++ types and exceptions never cross the boundary.
- The ABI is versioned separately from the library. Current: ABI `3`, library
  `0.3.0`.

## Threading

- Language metadata is immutable after engine construction.
- A document serializes calls with a recursive mutex.
- Change listeners run synchronously on the mutating thread while the document is
  locked. They must not destroy the document or block.
- Snapshots are immutable and may be queried from any thread.
- Dart callbacks are isolate-local; Kotlin callbacks run on the mutating thread.

## Platform SDKs

- **Dart** (`sdk/flutter`): hand-written `dart:ffi`; no package dependencies.
  The package includes an Android Gradle/CMake plugin that builds
  `libcodecore.so` from the shared sources. Publishing needs the native sources
  vendored into the package or a prebuilt AAR.
- **Kotlin** (`sdk/kotlin`): JNI bridge with explicit UTF-8/UTF-16 conversion.
  The Gradle wrapper and NDK version are included. `:assembleDebug` compiles the
  Kotlin SDK and native sources into a debug AAR; the JNI surface is also
  smoke-tested through a Java harness with identical symbols.

## Why these choices

- **Piece table** gives cheap inserts and append-only edit storage.
- **Incremental line starts** make position updates cheaper than rescanning text.
- **Per-version analysis** avoids re-lexing unchanged documents.
- **Immutable snapshots** let hosts parallelize analysis safely.
- **C ABI** keeps Dart and Kotlin independent of C++ ABI details.
- **Lexical Level 1** delivers useful highlighting for 23 languages without
  grammar binaries; Tree-sitter remains the planned upgrade.
