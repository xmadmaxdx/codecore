# CodeCore

CodeCore is an embeddable code-editor engine: a shared C++ core with a stable C
ABI, plus Dart FFI and Kotlin/JNI integration layers. It is designed to power
editors inside Flutter and Android applications without shipping a compiler or
requiring a cloud account.

This repository contains **CodeCore Core v0.1** — the engine foundation. The
editor widget layer and the full IDE features come in later milestones.

## Status

Implemented and tested:

| Area | State |
|---|---|
| Piece-table UTF-8 text buffer | Implemented, tested |
| Document model, line index, positions | Implemented, tested |
| Insert / erase / replace | Implemented, tested |
| Undo / redo with typing coalescing and history limits | Implemented, tested |
| Selection, cursor movement, bracket matching | Implemented, tested |
| 23 language definitions | Implemented, tested |
| Lexical syntax tokens (comments, strings, numbers, keywords, types, tags, CSS properties) | Implemented, tested |
| Keyword / symbol / builtin completion | Implemented, tested |
| Bracket and unterminated-literal diagnostics | Implemented, tested |
| Brace and block-comment folding ranges | Implemented, tested |
| Literal find (case, whole-word) | Implemented, tested |
| C ABI with explicit ownership and change events | Implemented, tested |
| Dart FFI SDK | Implemented, tested |
| Kotlin JNI bridge | Compiled; Java smoke test passes; Kotlin module not compiled (no Kotlin/Gradle toolchain) |

Not implemented yet — see [docs/status.md](docs/status.md) for the honest matrix:

- Tree-sitter incremental parsing and grammar packs.
- LSP client, semantic intelligence, rename, formatting.
- AI ghost-text providers and inline completion.
- Editor UI widgets (Flutter) and Android views.
- Package manager, downloads, signing and verification.
- Execution API and local runtimes (Phase B).
- Performance benchmarks and memory profiling.

## Repository layout

```text
core/                 C++ engine (text buffer, document, language, engine)
native/include/       Public C ABI header
native/src/           C ABI implementation
native/jni/           JNI bridge
sdk/flutter/          Dart FFI package (`codecore`)
sdk/kotlin/           Kotlin SDK sources
tests/core/           Native test suite
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

### Windows / MinGW

```bash
cmake -S . -B build/native -G "MinGW Makefiles"
cmake --build build/native -j
./build/native/codecore_tests.exe
```

The native suite currently runs **446 checks with 0 failures** and CTest passes.

### JNI bridge

```bash
cmake -S . -B build/native-jni -G "MinGW Makefiles" \
  -DCODECORE_BUILD_JNI=ON -DCODECORE_BUILD_TESTS=OFF
cmake --build build/native-jni

mkdir -p build/jni-test
javac -d build/jni-test \
  tests/jni/dev/codecore/nativebridge/NativeCodeCore.java \
  tests/jni/JniSmokeTest.java
java -Djava.library.path=build/native-jni -cp build/jni-test JniSmokeTest
```

It exercises the same entry points the Kotlin wrapper declares, without
requiring Kotlin tooling. See [docs/kotlin.md](docs/kotlin.md).

## Verification

| Check | Result |
|---|---|
| Native engine tests | 446 checks, 0 failures |
| CTest | 1/1 passed |
| Dart static analysis | No issues |
| Dart FFI smoke test | 38 checks, 0 failures |
| JNI smoke test (Java) | 27 checks, 0 failures |
| Kotlin module compile | Not run — no Kotlin/Gradle toolchain installed |
| Performance benchmarks | Not run — no claims made |

## Use from Dart

The Dart package lives in `sdk/flutter`. It has no package dependencies; native
memory is allocated through the C ABI.

```dart
import 'package:codecore/codecore.dart';

final core = CodeCore.open();          // or CodeCore.open(libraryPath: '...')
final doc = core.createDocument(language: 'python');

doc.text = 'def total(a, b):\n    return a + b\n';
doc.insert(doc.length, '\nprint(total(2, 3))\n');

for (final token in doc.tokenize()) {
  print('${token.start}..${token.end} ${token.kind.name}');
}

for (final item in doc.complete(doc.length)) {
  print('${item.label} (${item.kind.name})');
}

doc.dispose();
core.dispose();
```

The loader checks `libraryPath`, then `CODECORE_LIBRARY_PATH`, then the platform
loader path and common local build directories.

Run the FFI smoke test after building the native library:

```bash
cd sdk/flutter
dart pub get
CODECORE_LIBRARY_PATH=../../build/native/libcodecore.dll \
  dart run tool/smoke_test.dart
```

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
- v0.1's language layer is lexical only. It is not a parser and must not be
  presented as semantic language support.

## Architecture in one paragraph

The engine owns a piece-table buffer and a lazily rebuilt line index. Edits
produce inverse records for undo; repeated adjacent typing coalesces into one
history entry. Language data is an immutable registry of 23 lexer definitions
loaded once per engine. Documents hold a shared reference to their language, so
they remain valid even if the engine is destroyed. The C ABI is the only
boundary seen by Dart and Kotlin; C++ types never cross it.

More detail: [docs/architecture.md](docs/architecture.md).

## License

No license has been selected yet. Treat the source as all rights reserved until
a license file is added.
