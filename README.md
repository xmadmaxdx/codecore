# CodeCore

CodeCore is an embeddable code-editor engine: a shared C++ core with a stable C
ABI, plus Dart FFI and Kotlin/JNI integration layers. It is designed to power
editors inside Flutter and Android applications without shipping a compiler or
requiring a cloud account.

**Current release: CodeCore Core v0.3.0.** The C ABI is version `3`; the Dart
package is `codecore 0.3.0`. These version numbers move independently of the
editor-widget milestone, which does not exist yet.

## What changed in 0.3

- Incremental line indexing, versioned analysis caches and immutable snapshots.
- Agent-facing editing: edit origins, proposed content with line diffs, atomic
  accept/reject, host-provided ghost text with partial acceptance, and snapshot
  restoration.
- Kotlin parity for selection, cursor movement, language switching, change
  listeners, proposals, ghost text and snapshots.
- Regex search with status reporting across the C ABI, Dart and Kotlin.
- Multi-cursor editing: add/clear/move carets, atomic multi-insert and
  multi-erase as single undo steps.
- A Flutter Android plugin that builds and packages `libcodecore.so`; manual
  `jniLibs` copying is no longer required in this repository.

## Status

Implemented and tested:

| Area | State |
|---|---|
| Piece-table UTF-8 text buffer | Implemented, tested |
| Incremental line index and position mapping | Implemented, tested with randomized edits |
| Document model and versioned text | Implemented, tested |
| Insert / erase / replace / read-only / editable | Implemented, tested |
| Undo / redo, typing coalescing, edit origins | Implemented, tested |
| Selection, cursor movement, bracket matching | Implemented, tested |
| Multi-cursor editing and atomic grouped edits | Implemented; compile-verified |
| LSP client (JSON-RPC framing, lifecycle, sync, completion/hover/definition, diagnostics) | Implemented; compile-verified through the C ABI |
| 23 language definitions and lexical tokens | Implemented, tested |
| Keyword / symbol / builtin completion | Implemented, tested |
| Bracket and unterminated-literal diagnostics | Implemented, tested |
| Brace and block-comment folding | Implemented, tested |
| Literal and regex find (case, whole-word) | Implemented, tested |
| C ABI with explicit ownership, events and origin metadata | Implemented, tested |
| Immutable snapshots and snapshot restore | Implemented, tested |
| Proposed changes, bounded line diff, atomic accept/reject | Implemented, tested |
| Ghost-text state, partial acceptance, dismissal | Implemented, tested |
| Dart FFI SDK | Implemented, tested |
| Kotlin JNI bridge | Compiled and exercised through a Java harness |
| Flutter Android plugin build | Added; builds the same native sources; not built inside a Flutter app here |

Not implemented yet — see [docs/status.md](docs/status.md):

- Tree-sitter grammars and incremental syntax trees.
- Dart/Kotlin LSP transport bindings (the shared C ABI client is available).
- Semantic intelligence, rename, formatting and code actions.
- AI model providers, agent orchestration, chat or multi-file workflows.
- Flutter editor widgets and Kotlin/Compose views (the UI is a separate layer).
- Package manager, downloads, signing and verification.
- Execution API and local runtimes (Phase B).

## Repository layout

```text
core/                 C++ engine (buffer, document, analysis, diff, snapshots)
core/lsp/             JSON-RPC codec, LSP framing and client session
native/include/       Public C ABI header
native/src/           C ABI implementation
native/jni/           JNI bridge
native/android/       Shared Android NDK build definition
sdk/flutter/          Dart FFI package and Flutter Android plugin
sdk/kotlin/           Kotlin SDK, Gradle wrapper and NDK build
tests/core/           Native test suite and randomized line-index checks
tests/jni/            Java harness for the JNI ABI
tests/performance/    Manual benchmark executable
docs/                 Architecture, API notes and status
```

## Build the native core

Requirements: CMake 3.20+, a C++17 compiler.

```bash
cmake -S . -B build/native
cmake --build build/native --config Release
ctest --test-dir build/native --output-on-failure
```

Artifacts:

- `build/native/libcodecore.a` — static library.
- `build/native/libcodecore.dll` (`.so` / `.dylib`) — shared library for FFI.

### JNI bridge

```bash
cmake -S . -B build/jni -DCODECORE_BUILD_JNI=ON -DCODECORE_BUILD_TESTS=OFF
cmake --build build/jni --config Release

mkdir -p build/jni/classes
javac -d build/jni/classes \
  tests/jni/dev/codecore/nativebridge/NativeCodeCore.java \
  tests/jni/JniSmokeTest.java
java -Djava.library.path=build/jni -cp build/jni/classes JniSmokeTest
```

### Kotlin / Android

The Kotlin module includes the Gradle wrapper. Set `sdk.dir` in
`sdk/kotlin/local.properties`, then:

```bash
cd sdk/kotlin
./gradlew.bat :assembleDebug        # Windows
./gradlew :assembleDebug            # macOS / Linux
```

The module pins NDK `28.2.13676358`. If your NDK is elsewhere, install that
version or update `ndkVersion` in `sdk/kotlin/build.gradle.kts`.

## Verification

> **Runtime verification note (November 3, 2026):** this machine's Device Guard
> policy began blocking newly built binaries (`SecurityServicesRunning=2`,
> error 4551). Counts below are from the last successful run before that
> policy took effect. Changes since then are compile-verified only.

Last verified on the development machine:

| Check | Result |
|---|---|
| Native engine tests | **2,982 checks, 0 failures** |
| CTest | 1/1 passed |
| Dart static analysis | No issues |
| Dart FFI smoke test | **64 checks, 0 failures** |
| JNI smoke test (Java) | **74 checks, 0 failures** |
| Kotlin module compile | `gradlew.bat :assembleDebug` — **BUILD SUCCESSFUL**, debug AAR produced (NDK 28.2.13676358) |
| Benchmark | Manual harness exists; no speed target is claimed |

## Use from Dart

```dart
import 'package:codecore/codecore.dart';

final core = CodeCore.open();          // or CodeCore.open(libraryPath: '...')
final doc = core.createDocument(language: 'python');

doc.text = 'def total(a, b):\n    return a + b\n';
doc.insert(doc.length, '\nprint(total(2, 3))\n');

for (final token in doc.tokenize()) {
  print('${token.start}..${token.end} ${token.kind.name}');
}

doc.dispose();
core.dispose();
```

Agent-facing example:

```dart
doc.editOrigin = EditOrigin.ai;
doc.setProposal('def total(a, b):\n    return a + b + 1\n');
for (final line in doc.proposalDiff) {
  print('${line.kind.name}: ${line.text}');
}
doc.acceptProposal();       // one atomic, undoable edit

doc.showGhostText(doc.length, '\nprint(total(1, 2))');
doc.acceptGhostText();      // never inserted before acceptance
```

On Android, use the package as a local/path Flutter dependency. Its plugin
builds `libcodecore.so` from the shared native sources. See
[docs/flutter.md](docs/flutter.md) for packaging notes before publishing.

## C ABI

```c
#include <codecore/codecore.h>

cc_engine* engine = cc_engine_create(NULL);
cc_document* doc = cc_document_create(engine, NULL);

cc_document_set_text(doc, "print(1)\n", 9);
cc_document_insert(doc, 0, "# hi\n", 5);
cc_document_undo(doc);

size_t length = 0;
char* text = cc_document_get_text(doc, &length);
/* use text */
cc_string_free(text);
cc_document_destroy(doc);
cc_engine_destroy(engine);
```

Rules that matter:

- All offsets are UTF-8 byte offsets on code-point boundaries.
- Every returned array or string is owned by the caller and must be freed with
  the matching `cc_*_free` function.
- A document serializes its own calls; listeners run on the mutating thread.
- The language layer is lexical only. It is not a parser and must not be
  presented as semantic language support.
- Proposals and ghost text are preview state. They do not mutate the document
  until explicitly accepted.

## Architecture in one paragraph

The engine owns a piece-table buffer, an incrementally maintained line index and
one immutable analysis cache per text version. Edits produce inverse records for
undo; repeated adjacent typing coalesces into one history entry, and every record
carries an edit origin. Language data is an immutable registry of 23 lexer
definitions loaded once per engine. Snapshots copy text and language, so analysis
can move to a worker thread without holding the document lock. The C ABI is the
only boundary seen by Dart and Kotlin; C++ types never cross it.

More detail: [docs/architecture.md](docs/architecture.md).

## License

No license has been selected yet. Treat the source as all rights reserved until
a license file is added.
