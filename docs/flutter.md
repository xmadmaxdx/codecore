# Dart / Flutter SDK

Package: `sdk/flutter`, name `codecore`, version `0.1.0`.

This is a pure Dart FFI package. It contains no Flutter widgets yet; the editor
widget layer is a later milestone. It can be used from Flutter or from a Dart
CLI process.

## Loading the native library

Build the shared library first:

```bash
cmake -S . -B build/native
cmake --build build/native
```

Then:

```dart
import 'package:codecore/codecore.dart';

final core = CodeCore.open();
```

Resolution order:

1. Explicit `CodeCore.open(libraryPath: '...')`.
2. `CODECORE_LIBRARY_PATH`.
3. Platform loader names (`codecore.dll`, `libcodecore.so`, `libcodecore.dylib`).
4. Common local build paths such as `build/native/libcodecore.dll`.

For Android, package the shared library as a Flutter plugin's `jniLibs`; the
loader opens `libcodecore.so` by name. A ready-made Flutter plugin is not part of
v0.1 yet.

## Documents

```dart
final doc = core.createDocument(language: 'python', tabSize: 4);

doc.text = 'def total(a, b):\n    return a + b\n';
doc.insert(doc.length, '\nprint(total(2, 3))\n');

print(doc.lineCount);
print(doc.offsetToPosition(doc.length));

final selection = doc.selection;
doc.setSelection(0, 4);
doc.moveCursor(CursorMove.wordRight, extendSelection: true);

doc.undo();
doc.redo();
doc.dispose();
```

Offsets are UTF-8 bytes. `document.text` converts to a Dart string; keep byte
offsets when placing the caret, because Dart string indexes are UTF-16 code
units and do not match CodeCore offsets for non-ASCII text.

## Language services

```dart
final tokens = doc.tokenize();          // TokenSpan {start, end, kind}
final items = doc.complete();           // at end of document by default
final problems = doc.diagnose();
final folds = doc.foldingRanges();
final matches = doc.find('total', wholeWord: true);

final partner = doc.matchBracket(offset); // null when none
```

Completion is suppressed inside strings and comments. Diagnostics are lexical
only. The capability flags on `core.languages` reflect what is implemented.

## Change notifications

```dart
doc.onChange = (event) {
  print('${event.kind} at ${event.start} v${event.version}');
};
```

The callback is installed with `NativeCallable.isolateLocal`, so it must be used
from the isolate that owns the document. Do not block in the callback; it runs
while the native document is locked.

Set `doc.onChange = null` to detach.

## Testing

```bash
cd sdk/flutter
dart pub get
dart analyze
CODECORE_LIBRARY_PATH=../../build/native/libcodecore.dll \
  dart run tool/smoke_test.dart
```

Result on the development machine: analyze clean, smoke test 38 checks / 0
failures.

## Limitations

- No Flutter widgets, controller widget, gutter, theme or rendering.
- No async language services; do not call them on the UI thread for large files.
- Single selection only.
- No regex search.
- No semantic intelligence or execution.
- Android packaging is not wired into a plugin yet.
