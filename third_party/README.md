# Vendored third-party code

These trees are committed so native, Android and Flutter builds work offline and
reproducibly. Only the files needed to compile the runtime and the Python grammar
are kept.

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

CodeCore's highlight query lives in `core/syntax/python_highlights.h`; it is a
predicate-free subset of the upstream query so the C API can evaluate it
without a predicate engine.

Updating a grammar means replacing these files, rebuilding, and re-running
`codecore_syntax_tests`.
