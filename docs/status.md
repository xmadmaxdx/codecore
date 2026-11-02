# Status — CodeCore Core v0.3.0

Release identifiers:

| Identifier | Value |
|---|---|
| Core library version | `0.3.0` (`cc_version_string`) |
| C ABI version | `3` (`cc_abi_version`) |
| Dart package | `codecore 0.3.0` |
| Kotlin module | `dev.codecore` 0.3.0 sources |
| Gradle wrapper | 9.3.1 |
| Pinned Android NDK | 28.2.13676358 |

This file is the honest capability matrix. A checked item is implemented and
covered by the verification below; it does not imply production hardening.

## Implemented and tested

| Area | Details |
|---|---|
| Text buffer | Piece table, append-buffer compaction, UTF-8 validation |
| Document model | Set/get text, byte offsets, lines, positions, versions |
| Line index | Incremental line-start updates, randomized insert/erase/replace checks |
| Analysis cache | One immutable `Analysis` per text version; repeated queries reuse it |
| Editing | Insert, erase, replace, UTF-8 boundary checks, read-only/editable mode |
| Undo/redo | Inverse records, typing coalescing, history limits, edit origins |
| Selection/cursor | Get/set selection, left/right/word/line/up/down/doc moves |
| Brackets | Matching bracket lookup, bracket-aware diagnostics |
| Languages | 23 language definitions with comments, strings, keywords, types, builtins |
| Syntax | Lexical tokens for comments, strings, numbers, keywords, tags, attributes, CSS properties |
| Completion | Keywords, types, builtins, document symbols; prefix filter, ranking, 100-item cap |
| Diagnostics | Unmatched/unclosed brackets, unterminated strings and block comments |
| Folding | Brace ranges, multi-line comment ranges |
| Find | Literal search with case and whole-word options |
| Snapshots | Create, query, compare through text access, restore as one undoable edit |
| Proposals | Preview text, bounded line diff, accept/reject, atomic accept |
| Ghost text | Host-provided inline suggestion, partial acceptance, dismissal, auto-clear on edit |
| Edit origins | User, AI, host, formatter, external tool; stored in history and emitted in events |
| Change events | Versioned set/insert/erase/replace/undo/redo/proposal/snapshot/ghost events with origin |
| C ABI | Opaque handles, explicit ownership, allocation/free functions, language metadata |
| Dart SDK | FFI loader, documents, editing, tokens, completion, diagnostics, folding, find, proposals, ghost text, snapshots, change listener |
| JNI bridge | Compiled; Java harness covers the Kotlin-visible surface |
| Kotlin SDK | Sources include selection, cursor, language switching, change listeners, proposals, ghost text and snapshots |

## Verification performed

Environment: Windows, Flutter 3.47.7 / Dart 3.13.5, MinGW-w64 GCC 16.1.0,
CMake 3.20+, JDK 21, NDK 28.2.13676358.

| Check | Command | Result |
|---|---|---|
| Native build | `cmake --build build/native --config Release` | Success |
| Native tests | `./build/native/codecore_tests.exe` | **2,967 checks, 0 failures** |
| CTest | `ctest --test-dir build/native --output-on-failure` | 1/1 passed |
| Dart analyze | `dart analyze` in `sdk/flutter` | No issues |
| Dart FFI smoke | `dart run tool/smoke_test.dart` | **61 checks, 0 failures** |
| JNI build | `cmake --build build/jni` | Success |
| JNI smoke | `java -Djava.library.path=build/jni -cp build/jni/classes JniSmokeTest` | **72 checks, 0 failures** |
| Kotlin Gradle build | `gradlew.bat :assembleDebug` | **BUILD SUCCESSFUL**; Kotlin and Android/JNI sources compiled, debug AAR produced |
| Flutter Android plugin | Not built on this machine | Plugin sources and CMake build are present |
| Performance | Manual benchmark only | No speed or memory target claimed |

## Source present, not built on this machine

| Area | State |
|---|---|
| Kotlin SDK | Compiled successfully with `:assembleDebug` (Gradle 9.3.1, JDK 21 targeting JVM 17, NDK 28.2.13676358). |
| Android AAR | Debug AAR produced by `:assembleDebug`. No APK or release build was produced. |
| Flutter plugin | Android Gradle/CMake plugin sources are present. A Flutter app build was not run here. |

## Planned, not implemented

Do not treat any of these as available:

- Tree-sitter parsing, grammars and syntax queries.
- LSP client, semantic completion, hover, definition, references, rename,
  formatting, code actions, semantic diagnostics.
- AI model providers, prompt orchestration, agent loops, chat, multi-file edits.
- Flutter editor widgets and Kotlin/Compose views.
- Editor rendering, gutter, minimap, tabs, themes (UI layer).
- Package manager, downloads, signature verification, atomic install/rollback.
- Execution API, runtime manager, terminal, debugging, build system.
- Project/workspace management, git integration, symbol indexing.
- Multi-cursor editing and multi-selection API.
- Regex search and indentation-based folding.
- Incremental tokenization; lexical analysis still scans the whole document per
  version, though unchanged versions are cached.
- Performance benchmarks with a target, memory profiling and low-end device
  testing.

## Known limitations

1. **Lexical, not semantic.** Highlighting and diagnostics are heuristic. A
   string that looks like code is a string; macro-expanded types are unknown.
2. **Analysis is per-version, not incremental.** Each edit invalidates the
   document analysis cache. Snapshots let hosts move that work off the document
   lock, but they do not make tokenization incremental.
3. **Line index updates are O(number of lines).** Lookups are binary search;
   edits rebuild the line-start vector. This is measured by the manual benchmark,
   not yet optimized.
4. **Snapshots copy text.** They are lightweight compared with a full document
   clone, but they are not structural sharing.
5. **Proposal diffs are bounded.** The LCS table is capped; very large inputs
   fall back to a correct but coarse delete-all/insert-all diff.
6. **Single selection.** One anchor/focus pair; multiple cursors are not
   implemented.
7. **No async API.** Language services are synchronous. Hosts must schedule them
   away from the UI thread for large files.
8. **JNI offsets are `Int`.** Documents over 2 GB cannot be addressed through
   the Kotlin bridge; the default engine limit is 64 MB.
9. **Dart callbacks are isolate-local.** Change notifications must be used from
   the isolate that owns the document.
10. **No UI.** There is no Flutter widget, Compose view, gutter, theme or
    renderer in this release.
