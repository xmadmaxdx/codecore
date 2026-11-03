# Status — CodeCore Core v0.6.0

Release identifiers:

| Identifier | Value |
|---|---|
| Core library version | `0.6.0` (`cc_version_string`) |
| C ABI version | `7` (`cc_abi_version`) |
| Dart package | `codecore 0.6.0` |
| Kotlin module | `dev.codecore` 0.6.0 sources |
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
| Background analysis | One worker; latest submission wins, stale results are discarded, tokens/diagnostics/folding are computed off-thread |
| Viewport geometry | Allocation-free visible-line range with overscan, line tops and content height |
| Editing | Insert, erase, replace, UTF-8 boundary checks, read-only/editable mode |
| Undo/redo | Inverse records, typing coalescing, history limits, edit origins |
| Selection/cursor | Get/set selection, left/right/word/line/up/down/doc moves |
| Multi-cursor | Add/clear/query cursors, move all carets, atomic multi-insert and multi-erase with one undo entry |
| LSP | JSON-RPC 2.0 plus Content-Length framing, initialize handshake, full-text sync, completion, hover, definition, signature help, references, document/workspace symbols, rename, formatting, code actions, semantic tokens, cancellation, publishDiagnostics and server request replies. C ABI, Dart and Kotlin surfaces are implemented and build-verified. |
| Brackets | Matching bracket lookup, bracket-aware diagnostics |
| Languages | 40 language definitions with comments, strings, keywords, types, builtins |
| Syntax | Lexical tokens for comments, strings, numbers, keywords, tags, attributes, CSS properties |
| Structural syntax | Tree-sitter runtime plus eight grammars (Python, JavaScript, Java, Go, JSON, C, C++, Rust), incremental reparse, structural highlights, syntax errors, node lookup. Core, C ABI, Dart and Kotlin SDKs. |
| Local execution | One process per session with piped stdin/stdout/stderr, polled output events, per-stream ordering, timeouts, cancellation, exit codes, cwd and environment overrides. Core, C ABI, Dart and Kotlin SDKs; no runtime is bundled. |
| Completion | Keywords, types, builtins, document symbols; prefix filter, ranking, 100-item cap |
| Diagnostics | Unmatched/unclosed brackets, unterminated strings and block comments |
| Folding | Brace ranges, multi-line comment ranges |
| Find | Literal search with case and whole-word options; ECMAScript regex search with status reporting |
| Snapshots | Create, query, compare through text access, restore as one undoable edit |
| Proposals | Preview text, bounded line diff, accept/reject, atomic accept |
| Ghost text | Host-provided inline suggestion, partial acceptance, dismissal, auto-clear on edit |
| Edit origins | User, AI, host, formatter, external tool; stored in history and emitted in events |
| Change events | Versioned set/insert/erase/replace/undo/redo/proposal/snapshot/ghost events with origin |
| C ABI | Opaque handles, explicit ownership, allocation/free functions, language metadata |
| Dart SDK | FFI loader, documents, editing, tokens, structural syntax, execution, completion, diagnostics, folding, find, proposals, ghost text, snapshots, change listener, LSP client |
| JNI bridge | Compiled; Java harness covers the Kotlin-visible surface |
| Kotlin SDK | Selection, cursor, multi-cursor, language switching, change listeners, proposals, ghost text, snapshots, structural syntax, execution and an LSP client |

## Verification performed

Environment: Windows, Flutter 3.47.7 / Dart 3.13.5, MinGW-w64 GCC 16.1.0,
CMake 3.20+, JDK 21, NDK 28.2.13676358.

| Check | Command | Result |
|---|---|---|
| Native build | `cmake --build build/native --config Release` | Success |
| Native tests | `./build/native/codecore_tests.exe` | **3,361 checks, 0 failures** |
| LSP tests | `./build/native/codecore_lsp_tests.exe` | **59 checks, 0 failures** |
| Viewport/background tests | `./build/native/codecore_performance_tests.exe` | **36 checks, 0 failures** |
| Tree-sitter syntax tests | `./build/native/codecore_syntax_tests.exe` | **434 checks, 0 failures** |
| Execution tests | `./build/native/codecore_execution_tests.exe` | **79 checks, 0 failures** |
| CTest | `ctest --test-dir build/native --output-on-failure` | 5/5 passed |
| Dart analyze | `dart analyze` in `sdk/flutter` | No issues |
| Dart FFI smoke | `dart run tool/smoke_test.dart` | **124 checks, 0 failures** |
| JNI build | `cmake --build build/native -DCODECORE_BUILD_JNI=ON` | Success |
| JNI smoke | `java -Djava.library.path=build/native-jni -cp build/jni-test JniSmokeTest` | **175 checks, 0 failures** |
| Kotlin Gradle build | `gradlew.bat :assembleDebug` | **BUILD SUCCESSFUL**; Kotlin and Android/JNI sources compiled, debug AAR produced |
| Android native build | `libcodecore_jni.so` for 4 ABIs | Tree-sitter runtime and all eight grammars linked in |
| LSP SDK surface | `dart analyze`, `gradlew.bat :assembleDebug` | Clean / BUILD SUCCESSFUL |
| Flutter Android plugin | Not built on this machine | Plugin sources and CMake build are present |
| Benchmark | `build/native/codecore_benchmark.exe` | Results recorded below; no target claimed |

## Source present, not built on this machine

| Area | State |
|---|---|
| Kotlin SDK | Compiled successfully with `:assembleDebug` (Gradle 9.3.1, JDK 21 targeting JVM 17, NDK 28.2.13676358). |
| Android AAR | Debug AAR produced by `:assembleDebug`. No APK or release build was produced. |
| Flutter plugin | Android Gradle/CMake plugin sources are present. A Flutter app build was not run here. |

## Planned, not implemented

Do not treat any of these as available:

- Structural grammars beyond the eight compiled-in languages; no runtime grammar loader or grammar package yet.
- Language-server process, pipe and socket managers; no server is bundled.
- Semantic intelligence independent of LSP.
- AI model providers, prompt orchestration, agent loops, chat, multi-file edits.
- Flutter editor widgets and Kotlin/Compose views.
- Editor rendering, gutter, minimap, tabs, themes (UI layer).
- Package manager, downloads, signature verification, atomic install/rollback.
- Runtime registry, bundled runtimes, PTY/terminal, debugging and build system.
- Execution on Android at runtime: the POSIX path is compiled into the NDK
  build, but app-data executables are blocked by platform policy and were not
  run on a device here.
- Project/workspace management, git integration, symbol indexing.
- Indentation-based folding (brace and block-comment folding only).
- Incremental tokenization; the background worker moves the full scan off the
  caller's thread, but an edited document is still re-lexed from the start.
- Per-line layout/render caches and partial invalidation (viewport math is ready;
  the renderer is a separate layer).
- Performance benchmarks with a target, memory profiling and low-end device
  testing.

## Known limitations

1. **Lexical or syntactic, never semantic.** The fallback highlighter is
   heuristic. Tree-sitter supplies a concrete syntax tree for the eight bundled
   languages, not types, scopes or name resolution. A string that looks like
   code is still a string.
2. **Analysis is per-version, not incremental.** Each edit invalidates the
   document analysis cache. `BackgroundAnalyzer` moves the full scan and
   diagnostics off the caller's thread and rejects stale results, but it does not
   make tokenization incremental.
3. **Line index updates are O(number of lines).** Lookups are binary search;
   edits rebuild the line-start vector. This is measured by the manual benchmark,
   not yet optimized.
4. **Snapshots copy text.** They are lightweight compared with a full document
   clone, but they are not structural sharing.
5. **Proposal diffs are bounded.** The LCS table is capped; very large inputs
   fall back to a correct but coarse delete-all/insert-all diff.
6. **Document language services are synchronous.** `BackgroundAnalyzer` is the
   async path for tokens, diagnostics and folding; completion and search remain
   synchronous and should be scheduled by the host for large files.
7. **JNI offsets are `Int`.** Documents over 2 GB cannot be addressed through
   the Kotlin bridge; the default engine limit is 64 MB.
8. **Dart callbacks are isolate-local.** Change notifications must be used from
   the isolate that owns the document.
9. **No UI.** There is no Flutter widget, Compose view, gutter, theme or
   renderer in this release.
10. **Regex search is host-controlled.** Patterns use C++ ECMAScript semantics,
    are capped at 10,000 ranges and are not protected against pathological
    backtracking. Validate or constrain patterns in trusted code.
11. **Structural grammars are compile-time.** Eight grammars are linked into
    every build; adding another language means vendoring its parser and
    highlight query. There is no runtime grammar package or download path yet,
    so Kotlin, TypeScript, Swift and the remaining lexical languages still get
    no syntax tree.
12. **Tree traversal is per call.** `nodeCount()`, highlights and errors walk or
    query the tree each time a syntax handle is built. The tree itself is cached
    per document version and reparsed incrementally, but the handle is not a
    per-frame render cache.
13. **LSP is transport-only.** The C ABI, Dart and Kotlin clients handle
    protocol framing, lifecycle, requests and diagnostics, but the host owns
    the process/socket. Document sync is full-text, positions use UTF-16
    coordinates, frames are capped at 16 MB and no language server is bundled.

## Performance snapshot

Manual benchmark, 1,000,100-byte Python document (33,373 lines, 116,802 tokens)
on the development machine:

| Measurement | Result |
|---|---|
| `set_text` | 18.9 ms |
| First full tokenize | 233.1 ms |
| Cached tokenize call | 5.3 ms (copies 116k tokens) |
| Insert at offset 0 (1 MB document) | 5.6 ms average |
| Snapshot creation | 0.8 ms |
| Cached snapshot tokenize | 3.2 ms |

These are single-run numbers on one machine, not a target. They show the
current shape: analysis is cached per version, edits update the line index
incrementally, and snapshot creation is cheap, while the first tokenize after
an edit is still a full scan.

## 0.6 additions and verification

| Area | Details |
|---|---|
| Incremental tokenization | `TokenCache` keeps the previous text, tokens and safe restart offsets. An edit re-lexes from the edited line to the first converging safe boundary and shifts the untouched tail. Unterminated strings and block comments still consume to end of text and keep their flags. Markup languages re-lex fully. |
| Terminal | `TerminalSession` streams shell bytes with a bounded backlog. POSIX/Android use `posix_openpt` with `TIOCSWINSZ` resize; Windows uses piped `cmd.exe` and reports no PTY. Exposed through `cc_terminal_*`. |
| Runtime registry | 22 runtimes resolved through PATH (Windows `PATHEXT`, Store aliases skipped), explicit version probing, unified `plan`/`run` with a compiler step. Exposed through `cc_runtime_*`. |

| Check | Command | Result |
|---|---|---|
| Incremental tokenization | `./build/native/codecore_incremental_tests.exe` | **426 checks, 0 failures** |
| Terminal | `./build/native/codecore_terminal_tests.exe` | **17 checks, 0 failures** |
| Runtime registry | `./build/native/codecore_runtime_tests.exe` | **39 checks, 0 failures** |
| CTest | `ctest --test-dir build/native` | **8/8 passed** |

Total native checks on Windows: **4,451, 0 failures**.

Not in 0.6: package manager, language-server process management and semantic
intelligence independent of LSP. Those remain open in UPDATED_PLAN.md. Editor
widgets and rendering were explicitly excluded from this release.
