# Architecture

CodeCore v0.1 is the engine foundation. It is deliberately layered so that a
Flutter UI, a Kotlin Android UI, or a native host can all use the same document
behavior without sharing C++ implementation details.

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
                 ├── undo / redo records
                 ├── line index (lazy)
                 ├── selection + cursor
                 ├── lexer (Level 1 language data)
                 └── completion / diagnostics / folding / find
```

## Text buffer

`TextBuffer` is a piece table:

- The original document text is immutable.
- Inserted text is appended to a growable add buffer.
- The document is a sequence of pieces referencing the original or add buffer.
- Edits split pieces at boundaries and merge adjacent contiguous pieces.
- The add buffer is compacted once it exceeds both 4 MB and the original
  document size.

This avoids copying the full document on every keystroke. Materializing the
whole document is still O(n); v0.1 exposes range tokenization but the lexer
scans from the beginning of the text to preserve multiline state. A rope or
incremental token cache is future work.

## Document model

`Document` owns the buffer, language pointer, selection, history and the cached
line index.

- Offsets are UTF-8 byte offsets. Edits validate that offsets do not split a
  UTF-8 code point and that inserted text is valid UTF-8.
- Lines are stored as `{start, contentEnd, end}`. `contentEnd` excludes the
  line ending, so `\r\n` is one ending.
- Columns are measured in Unicode code points.
- The line index is rebuilt lazily after an edit. It is exact and simple; an
  incremental index is a later optimization.
- Every mutating call increments a document version and emits a change event
  (`set_text`, `insert`, `erase`, `replace`, `undo`, `redo`).

## Undo and redo

Each edit records its offset plus the removed and inserted strings.

- Adjacent single-character typing is coalesced into one record for up to one
  second, capped at 256 bytes.
- Erases are discrete records.
- The history limit defaults to 200 and is configurable; 0 disables history.
- A new edit clears the redo stack.

Storing removed text means a large deletion can hold a large record until it is
trimmed. That is an accepted v0.1 trade-off and is listed in
[status.md](status.md).

## Language layer

`LanguageRegistry` builds 23 immutable language definitions once per engine.
Documents hold a `shared_ptr<const Language>`, so they remain valid if the
engine is destroyed.

The lexer is a table-driven, single-pass tokenizer. It handles:

- Line and block comments (including Lua's block-comment form).
- Single, triple and backtick strings.
- Numbers with common prefixes, exponents and suffixes.
- Keywords, types, builtins, constants, annotations and function calls.
- HTML/XML tags, attributes and attribute values.
- CSS/SCSS property detection inside blocks.
- Case-insensitive keyword lookup for SQL.

It is explicitly **not** Tree-sitter. It does not build a syntax tree, does not
understand scopes or types, and cannot provide semantic claims. The public
capability flags advertise syntax, completion, diagnostics and folding only;
there is no "semantic" flag.

## Language services

- **Completion** merges keywords, types, builtins and document symbols, filters
  by the identifier prefix at the cursor, and ranks by kind, occurrence count
  and proximity. If the cursor is inside a comment or string, completion returns
  no items. Results are capped at 100.
- **Diagnostics** are conservative: unmatched/mismatched brackets, unclosed
  brackets, unterminated strings and unterminated block comments. Brackets
  inside comments and strings are ignored because only punctuation tokens are
  considered.
- **Folding** produces brace ranges and multi-line block-comment ranges. It does
  not implement indentation-based folding yet.
- **Find** is a literal, case-configurable, whole-word-configurable scan. Regex
  search is not implemented.

All services run synchronously. Hosts should not call them on a UI thread for
large documents; the next milestone moves them behind an async worker API.

## C ABI

`native/include/codecore/codecore.h` is the only supported native boundary.

- `cc_engine` and `cc_document` are opaque.
- Arrays returned through out-parameters are allocated with `cc_alloc`/`malloc`
  and freed by the matching `cc_*_free` function.
- Single strings are freed with `cc_string_free`.
- Language metadata pointers are owned by the engine.
- C++ types, exceptions and ownership never cross the boundary.

The implementation wraps core objects in `cc_document` handles that also own the
language shared references needed for `cc_document_set_language`.

## Threading

- The language registry is immutable after engine construction.
- A document serializes calls with a recursive mutex.
- Change listeners run synchronously on the mutating thread while the document
  is locked. A listener must not destroy the document and should not block.
- The Dart FFI SDK installs an isolate-local native callback, so mutations must
  originate on the same Dart isolate that created the document.

## Platform SDKs

- **Dart** (`sdk/flutter`): hand-written `dart:ffi` bindings; no package
  dependencies. Native allocation uses `cc_alloc`; UTF-8 conversion happens at
  the boundary.
- **Kotlin** (`sdk/kotlin`): JNI bridge with explicit UTF-8 ↔ UTF-16 conversion.
  The Android library builds `libcodecore_jni.so` from the same core sources.
  The bridge is compiled and smoke-tested via a Java harness; the Kotlin module
  itself was not compiled on the development machine.

## Why these choices

- **Piece table** gives cheap inserts and an append-only history of edits.
- **C ABI** keeps Dart and Kotlin independent of the C++ compiler and ABI.
- **Lexical Level 1** delivers useful highlighting for 23 languages without
  shipping grammar binaries; Tree-sitter remains the planned Level 1 upgrade.
- **Separate engine and language data** lets grammar packs replace the lexer
  later without changing the public document API.
