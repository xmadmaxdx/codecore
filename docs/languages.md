# Languages

CodeCore has 40 lexical language definitions. Every definition supplies
extensions, keywords, types, builtins, comment markers and quote characters for
tokenization, completion, diagnostics and folding.

Structural parsing is a separate layer. Eight languages have a compiled-in
Tree-sitter grammar: Python, JavaScript, Java, Go, JSON, C, C++ and Rust. Every
other language uses the lexical layer until a grammar is added to the build.

| ID | Language | Extensions | Structural |
|---:|---|---|---|
| 0 | Python | `.py`, `.pyw`, `.pyi` | Tree-sitter |
| 1 | Kotlin | `.kt`, `.kts` | lexical |
| 2 | Java | `.java` | Tree-sitter |
| 3 | JavaScript | `.js`, `.mjs`, `.cjs`, `.jsx` | Tree-sitter |
| 4 | TypeScript | `.ts`, `.tsx`, `.mts`, `.cts` | lexical |
| 5 | C | `.c`, `.h` | Tree-sitter |
| 6 | C++ | `.cpp`, `.cc`, `.cxx`, `.hpp`, `.hxx`, `.hh` | Tree-sitter |
| 7 | C# | `.cs` | lexical |
| 8 | Go | `.go` | Tree-sitter |
| 9 | Rust | `.rs` | Tree-sitter |
| 10 | Dart | `.dart` | lexical |
| 11 | PHP | `.php` | lexical |
| 12 | Ruby | `.rb`, `.rake`, `Gemfile` | lexical |
| 13 | Swift | `.swift` | lexical |
| 14 | HTML | `.html`, `.htm`, `.xhtml` | lexical |
| 15 | CSS | `.css` | lexical |
| 16 | SCSS | `.scss` | lexical |
| 17 | SQL | `.sql` | lexical |
| 18 | JSON | `.json` | Tree-sitter |
| 19 | XML | `.xml`, `.svg`, `.plist` | lexical |
| 20 | YAML | `.yml`, `.yaml` | lexical |
| 21 | Bash | `.sh`, `.bash`, `.zsh` | lexical |
| 22 | Lua | `.lua` | lexical |
| 23 | Perl | `.pl`, `.pm`, `.t` | lexical |
| 24 | R | `.r`, `.R`, `.Rmd` | lexical |
| 25 | Scala | `.scala`, `.sc` | lexical |
| 26 | Groovy | `.groovy`, `.gradle`, `.gvy` | lexical |
| 27 | Haskell | `.hs`, `.lhs` | lexical |
| 28 | Elixir | `.ex`, `.exs` | lexical |
| 29 | Erlang | `.erl`, `.hrl` | lexical |
| 30 | Clojure | `.clj`, `.cljs`, `.cljc`, `.edn` | lexical |
| 31 | OCaml | `.ml`, `.mli` | lexical |
| 32 | F# | `.fs`, `.fsi`, `.fsx` | lexical |
| 33 | Visual Basic | `.vb` | lexical |
| 34 | PowerShell | `.ps1`, `.psm1`, `.psd1` | lexical |
| 35 | Objective-C | `.m`, `.mm` | lexical |
| 36 | Assembly | `.asm`, `.s`, `.inc` | lexical |
| 37 | MATLAB | `.m` | lexical |
| 38 | Julia | `.jl` | lexical |
| 39 | Fortran | `.f`, `.f90`, `.f95`, `.f03`, `.f08` | lexical |

## Bundled structural grammars

| Grammar | Accepted ids | Root node | Notes |
|---|---|---|---|
| Python | `python`, `py`, `python3` | `module` | Comments, strings, numbers, keywords |
| JavaScript | `javascript`, `js`, `jsx`, `mjs`, `cjs` | `program` | JSX parses with the JavaScript grammar |
| Java | `java` | `program` | Classes, methods, annotations, keywords |
| Go | `go`, `golang` | `source_file` | Functions, types, selectors, keywords |
| JSON | `json` | `document` | Keys, strings, numbers, literals; no comments |
| C | `c`, `h` | `translation_unit` | Functions, declarations, types, keywords |
| C++ | `cpp`, `c++`, `cc`, `cxx`, `hpp` | `translation_unit` | Classes, functions, types, keywords |
| Rust | `rust`, `rs` | `source_file` | Functions, types, comments, keywords |

Aliases resolve to the canonical id, so a tree parsed from `rs` reports
`rust`. The alias `h` selects the C grammar; C++ headers use `hpp`.

Every grammar supplies:

- A concrete syntax tree through `cc_document_syntax_tree` and the Dart/Kotlin
  `syntaxTree()` wrappers.
- Structural highlights from a predicate-free Tree-sitter query.
- Real `ERROR` and `MISSING` nodes through `hasErrors()` / `errors()`.

The parser preserves unchanged subtrees across `reparse()`. Parsing,
highlights and error queries are syntax-only; they do not resolve names or
types. Kotlin, TypeScript, Swift and the other lexical languages still get
tokenization, completion, diagnostics and folding, but `syntaxTree()` returns
`null` until their grammar is compiled in.

## Adding structural support

A grammar is compiled in, not fetched at runtime. The extension path is
documented in [third_party/README.md](../third_party/README.md): vendor the
parser, add a predicate-free highlight query, add one row to the grammar table
in `core/syntax/syntax.cpp`, and add the sources to the three CMake builds
(native, Kotlin Android and Flutter Android).

Highlight queries must use the exact named/anonymous distinction of the
grammar. Quoting a named keyword such as `"super"` or naming an anonymous token
such as `(nullptr)` makes the whole query fail to compile; that grammar then
returns no highlights even though parsing still works.

## Verification

`codecore_syntax_tests` checks every bundled grammar with valid source for root
type, error-free parse, node count, structural highlights and a keyword capture,
plus malformed source that must report syntax errors. `codecore_tests` covers
all 40 lexical definitions by creating a document, tokenizing a sample and
exercising completion, diagnostics and folding.
