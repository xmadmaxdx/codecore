# Dart / Flutter SDK

Package: `sdk/flutter`, name `codecore`, version `0.3.0`.

The package is a Dart FFI SDK plus a Flutter Android plugin. It contains no
editor widgets yet; the widget/renderer layer is a separate milestone.

## Native library

### Desktop / tests

```bash
cmake -S . -B build/native
cmake --build build/native --config Release
```

The loader checks, in order:

1. `CodeCore.open(libraryPath: '...')`.
2. `CODECORE_LIBRARY_PATH`.
3. Platform loader names (`codecore.dll`, `libcodecore.so`, `libcodecore.dylib`).
4. Common local build paths such as `build/native/libcodecore.dll`.

### Android

`pubspec.yaml` declares an Android plugin. `sdk/flutter/android/CMakeLists.txt`
builds `libcodecore.so` from the shared `core/` and `native/` sources, and
Gradle packages it in the plugin AAR. No manual `jniLibs` copy is required for a
local/path dependency on this repository.

The plugin pins NDK `28.2.13676358`. The Flutter app supplies `sdk.dir` through
its own `local.properties`.

For publishing to pub.dev, the native sources must live inside the package
because the current CMake file references the repository root. Either vendor the
source tree under `sdk/flutter/android/native/` for a release tarball or ship a
prebuilt AAR. The local/path integration in this repository is ready; the
publish packaging step is not automated.

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
offsets when placing the caret.

## Permissions and edit origin

```dart
doc.readOnly = true;       // display, select and copy only
doc.editable = false;      // same state, clearer intent
doc.editOrigin = EditOrigin.ai;

doc.onChange = (event) {
  print('${event.kind} origin=${event.origin} v${event.version}');
};
```

Origins are `user`, `ai`, `host`, `formatter` and `externalTool`. The origin is
stored in undo history and emitted with change events.

## Proposals and diffs

```dart
doc.setProposal(
  'def total(a, b):\n    return a + b + 1\n',
  origin: EditOrigin.ai,
);

for (final line in doc.proposalDiff) {
  print('${line.kind.name} old=${line.oldLine} new=${line.newLine} ${line.text}');
}

doc.acceptProposal(); // atomic, one undo step
// or: doc.rejectProposal();
```

`proposal` is preview state. It does not change `doc.text` until accepted.
`DiffLine.oldLine` and `newLine` are `-1` on the missing side.

## Ghost text

```dart
doc.showGhostText(doc.length, '\nprint(total(1, 2))', origin: EditOrigin.ai);

final suggestion = doc.ghostText;   // GhostText? {offset, text, origin}
doc.acceptGhostText(maxCodePoints: 6);
// or: doc.dismissGhostText();
```

Ghost text is never inserted before acceptance. Any edit, undo or redo clears
it. Partial acceptance counts Unicode code points.

## Snapshots

```dart
final snapshot = doc.createSnapshot();
doc.text = 'temporary text\n';
doc.restoreSnapshot(snapshot); // one undoable edit
print(snapshot.text);
snapshot.dispose();
```

A snapshot remains valid after its document changes or is disposed. Always
dispose snapshots.

## Language services

```dart
final tokens = doc.tokenize();
final items = doc.complete();
final problems = doc.diagnose();
final folds = doc.foldingRanges();
final matches = doc.find('total', wholeWord: true);
final partner = doc.matchBracket(offset);
```

Completion is suppressed inside strings and comments. Diagnostics are lexical
only. Capability flags on `core.languages` reflect what is implemented.

## Testing

```bash
cd sdk/flutter
dart pub get
dart analyze
CODECORE_LIBRARY_PATH=../../build/native/libcodecore.dll \
  dart run tool/smoke_test.dart
```

Last run: analyze clean; smoke test **61 checks / 0 failures**.

## Limitations

- No Flutter widgets, controller widget, gutter, theme or rendering.
- No async language services; do not call them on the UI thread for large files.
- Single selection only.
- No regex search.
- No semantic intelligence or execution.
- Android plugin publishing needs the native sources vendored or an AAR; local
  path integration in this repository works from the shared source tree.
