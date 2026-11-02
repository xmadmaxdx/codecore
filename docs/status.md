# Status — CodeCore Core v0.1

This file is the honest capability matrix. A checked box means the behavior is
implemented and covered by the verification listed below; it does not imply
production hardening or performance validation.

## Implemented and tested

| Area | Details |
|---|---|
| Text buffer | Piece table, append buffer compaction, UTF-8 validation |
| Document model | Set/get text, byte offsets, lines, positions, versions |
| Editing | Insert, erase, replace, UTF-8 boundary checks, read-only mode |
| Undo/redo | Inverse records, typing coalescing, configurable history limit |
| Selection/cursor | Get/set selection, left/right/word/line/up/down/doc moves |
| Brackets | Matching bracket lookup, bracket-aware diagnostics |
| Languages | 23 language definitions with comments, strings, keywords, types, builtins |
| Syntax | Lexical token spans for comments, strings, numbers, keywords, tags, attributes, CSS properties |
| Completion | Keywords, types, builtins, document symbols; prefix filter, ranking, 100-item cap, comment/string suppression |
| Diagnostics | Unmatched/unclosed brackets, unterminated strings and block comments |
| Folding | Brace ranges, multi-line comment ranges |
| Find | Literal search with case and whole-word options |
| Change events | Versioned set/insert/erase/replace/undo/redo events through the C ABI |
| C ABI | Opaque handles, explicit ownership, allocation/free functions, language metadata |
| Dart SDK | FFI loader, documents, editing, tokens, completion, diagnostics, folding, find, change listener |
| JNI bridge | Compiled; Java smoke test exercises creation, editing, undo, tokens, completion, diagnostics, find, folding, brackets, read-only |

## Source present, not built on this machine

| Area | State |
|---|---|
| Kotlin SDK | Sources and Gradle/CMake module are present. No Kotlin compiler or Gradle installation was available, so the Kotlin module was not compiled. The JNI ABI it targets was compiled and tested through a Java harness with identical package/class/method names. |
| Android library | `native/android/CMakeLists.txt` and the Android module exist, but no APK/AAR was built. |

## Planned, not implemented

Do not treat any of these as available:

- Tree-sitter parsing, grammars and syntax queries.
- LSP client, semantic completion, hover, definition, references, rename,
  formatting, code actions, semantic diagnostics.
- AI completion providers, context builder, ghost text and partial acceptance.
- Flutter editor widgets and Kotlin/Compose views.
- Editor rendering, gutter, minimap, tabs, themes (UI layer).
- Package manager, download, signature verification, atomic install/rollback.
- Execution API, runtime manager, terminal, debugging, build system.
- Project/workspace management, git integration, symbol indexing.
- Multi-cursor editing API and undo grouping API.
- Regex search.
- Indentation-based folding.
- Incremental line index and incremental tokenization.
- Performance benchmarks, memory profiling and low-end device testing.

## Verification performed

Environment: Windows, Flutter 3.47.7 / Dart 3.13.5, MinGW-w64 GCC 16.1.0,
CMake 3.20+, JDK 21 at `D:\Android\jdk-21`.

| Check | Command | Result |
|---|---|---|
| Native build | `cmake --build build/native -j 8` | Success |
| Native tests | `./build/native/codecore_tests.exe` | 446 checks, 0 failures |
| CTest | `ctest --test-dir build/native --output-on-failure` | 1/1 passed |
| Dart analyze | `dart analyze` in `sdk/flutter` | No issues |
| Dart FFI smoke | `dart run tool/smoke_test.dart` with `CODECORE_LIBRARY_PATH` | 38 checks, 0 failures |
| JNI build | `cmake -S . -B build/native-jni -DCODECORE_BUILD_JNI=ON` | Success |
| JNI smoke | `java -Djava.library.path=build/native-jni -cp build/jni-test JniSmokeTest` | 27 checks, 0 failures |
| Kotlin compile | Not run | No Kotlin/Gradle toolchain installed |

## Known limitations that affect design

1. **Lexical, not semantic.** Highlighting and diagnostics are heuristic. A
   string that looks like code is a string; a macro-expanded type is not known.
2. **Full-document scans.** Tokenization, diagnostics, folding and completion
   scan the whole document and rebuild the line index on demand. This is fine
   for small and medium files; it is not yet validated for 10 MB documents.
3. **Undo stores deleted text.** Very large deletions increase memory until the
   history is trimmed.
4. **Single selection.** The core exposes one anchor/focus selection; multiple
   cursors are not implemented.
5. **No async API.** Language services are synchronous. Hosts must schedule
   them off the UI thread for large files.
6. **Language level claims are conservative.** All 23 languages are advertised
   at lexical support only. No language is advertised with semantic
   intelligence or execution.
7. **JNI offsets are `Int`.** Documents over 2 GB cannot be addressed through
   the Kotlin bridge as written; the default engine limit is 64 MB.
8. **Dart callbacks are isolate-local.** Change notifications must be used from
   the isolate that owns the document.

## Performance

No benchmark suite exists yet, so no performance target is claimed. The
architecture was chosen to avoid full-document copies on edit, but latency,
memory, startup time and battery behavior have not been measured. A benchmark
harness is the next engineering task after the editor UI layer.
