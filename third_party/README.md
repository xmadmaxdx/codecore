# Vendored third-party code

These trees are committed so native, Android and Flutter builds work offline and
reproducibly. Only the files needed to compile the runtime and the bundled
grammars are kept.

## tree-sitter

- Source: https://github.com/tree-sitter/tree-sitter
- Snapshot: shallow clone of `main`, 2026-11-03.
- Kept: `lib/` (C runtime and public `tree_sitter/api.h`), `LICENSE`,
  `README.md`.
- License: MIT. See `tree-sitter/LICENSE`.

## tree-sitter-python

- Source: https://github.com/tree-sitter/tree-sitter-python
- Snapshot: shallow clone of `main`, 2026-11-03.
- Kept: `src/` (generated `parser.c`, `scanner.c`, node types),
  `queries/highlights.scm`, `LICENSE`.
- License: MIT. See `tree-sitter-python/LICENSE`.

## tree-sitter-javascript

- Source: https://github.com/tree-sitter/tree-sitter-javascript
- Snapshot: tag `v0.25.0`, language ABI 15.
- Kept: `src/` (generated `parser.c`, `scanner.c`, `tree_sitter/parser.h`,
  node types), `queries/highlights.scm`, `LICENSE`.
- License: MIT. See `tree-sitter-javascript/LICENSE`.
- Covers JavaScript and JSX; `grammar.js` is not kept because the generated
  parser is what the build consumes.

CodeCore's highlight queries live in `core/syntax/*_highlights.h`. They are
predicate-free subsets of the upstream queries so the C query API can evaluate
them without a predicate engine.

## Adding or updating a grammar

1. Vendor `src/parser.c`, `src/scanner.c`, `src/tree_sitter/*.h`, `LICENSE` and
   the upstream `queries/highlights.scm` here.
2. Add a predicate-free query header in `core/syntax/`.
3. Add one row to `kGrammars` in `core/syntax/syntax.cpp`.
4. Add the parser and scanner to the three CMake builds: root `CMakeLists.txt`,
   `native/android/CMakeLists.txt`, `sdk/flutter/android/CMakeLists.txt`.
5. Extend `tests/core/test_syntax.cpp` with a parse, highlight, error and
   incremental-reparse case; run `codecore_syntax_tests`.

No document, C ABI, Dart or Kotlin code changes are needed: those layers only
ask whether a language has a grammar and consume the generic tree API.
