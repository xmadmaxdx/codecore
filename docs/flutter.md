# Dart / Flutter SDK

Package: `sdk/flutter`, name `codecore`, version `0.5.0`.


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

## Multi-cursor

```dart
doc.setSelection(0, 0);
doc.addCursor(4, 4);

print(doc.cursorCount);          // 2
print(doc.cursorAt(1).focus);    // 4

doc.insertAtCursors('# ');       // atomic, one undo step
doc.moveCursors(CursorMove.wordRight);
doc.eraseAtCursors();            // backspace at every caret

doc.clearCursors();              // keep the primary
```

Overlapping selections merge before the edit. Undo and redo apply the entire
multi-cursor edit as one entry. Change events fire once per affected range.

## Structural syntax

Tree-sitter is bundled for `python` and `javascript` (including JSX and the
`js`/`jsx`/`mjs`/`cjs` aliases). `syntaxTree()` returns `null` for a language
without a grammar; lexical tokens still work for every language.

```dart
if (core.supportsStructuralSyntax('python')) {
  final tree = doc.syntaxTree();
  if (tree != null) {
    print(tree.rootType);   // module
    print(tree.nodeCount);
    print(tree.hasErrors);  // ERROR / MISSING nodes present

    for (final highlight in tree.highlights) {
      print('${highlight.start}..${highlight.end} ${highlight.capture}');
    }
    for (final error in tree.errors) {
      print('${error.nodeType} at ${error.startLine}:${error.startColumn}');
    }
    tree.dispose();
  }
}
```

- `start`/`end` are UTF-8 byte offsets. Columns are bytes within the row, matching
  Tree-sitter, not Unicode code points.
- A tree is an immutable snapshot of one document version. Dispose it when done.
- Repeated `doc.syntaxTree()` calls on an unchanged version reuse the native
  tree; edits reparse incrementally from the previous tree.
- Highlights are structural, not semantic. There is no name or type resolution.

## Execution

`startExecution` runs a child process. CodeCore ships no runtime; pass the
command the host wants to run. Poll output and dispose the session when done.

```dart
final run = core.startExecution(
  command: 'python3',
  arguments: ['-c', 'print("hello")'],
  timeout: const Duration(seconds: 10),
);

while (true) {
  final event = run.poll();
  if (event == null) {
    sleep(const Duration(milliseconds: 5));
    continue;
  }
  if (event.kind == ExecutionEventKind.output) {
    stdout.write(event.data);
  } else {
    print('exit ${event.exitCode} cancelled=${event.cancelled}');
    break;
  }
}
run.dispose();
```

`write` sends stdin, `closeStdin` sends EOF, `cancel` terminates the process
tree, `wait` blocks with a timeout, and `exitCode` is -1 until exit. Output is
bounded (1 MiB by default); the host must keep polling to avoid backpressure.
The exit event is delivered after all queued output. On Android, app-data
executables are blocked by platform policy; package runtimes as native
libraries.

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
final definitions = doc.find(r'\bdef\s+\w+', regex: true);
final partner = doc.matchBracket(offset);
```

Completion is suppressed inside strings and comments. Diagnostics are lexical
only. Regex search uses ECMAScript syntax, is capped at 10,000 matches and
throws `CodeCoreException` with status 1 for an invalid pattern. Capability
flags on `core.languages` reflect what is implemented.

## LSP

`core.createLsp()` returns a `CodeCoreLsp` backed by the shared native client.
The host owns the language-server process, pipe or socket; CodeCore handles
framing, request ids, lifecycle, document sync and result/diagnostic queues.

```dart
final lsp = core.createLsp();
lsp.initialize('file:///project');

// Drain every frame and write it to your transport.
for (var frame = lsp.takeOutgoing(); frame != null; frame = lsp.takeOutgoing()) {
  server.write(frame);
}

// Feed bytes received from the server back into the client.
lsp.feed(await server.read());

lsp.didOpen('file:///project/main.py', 'python', 1, doc.text);
final id = lsp.completion('file:///project/main.py', 2, 4);

for (var result = lsp.takeResult(); result != null; result = lsp.takeResult()) {
  print('${result.method}: ${result.payload} ${result.isError}');
}
for (var report = lsp.takeDiagnostics(); report != null; report = lsp.takeDiagnostics()) {
  print('${report.uri}: ${report.payload}');
}
```

Convenience methods: `completion`, `hover`, `definition`, `signatureHelp`,
`references`, `documentSymbols`, `workspaceSymbols`, `rename`, `formatting`,
`codeAction` and `semanticTokens`. For any other method use `request(method,
params)` / `requestRaw(method, json)`, or `notify` / `notifyRaw`. Results are
raw JSON; `isError` identifies an error response. `cancel(id)`, `shutdown()`,
`exit()` and `dispose()` are provided.

- Positions are LSP UTF-16 line/character coordinates, not CodeCore byte offsets.
- Sync is full-text (`TextDocumentSyncKind.Full`).
- Frames are capped at 16 MB. No language server is bundled.

## Testing

```bash
cd sdk/flutter
dart pub get
dart analyze
CODECORE_LIBRARY_PATH=../../build/native/libcodecore.dll \
  dart run tool/smoke_test.dart
```

Last run: analyze clean; smoke test **111 checks / 0 failures**. The smoke test
asserts ABI 6 and covers structural syntax, LSP, documents, editing and agent
features.

## Limitations

- No Flutter widgets, controller widget, gutter, theme or rendering.
- No async language services; do not call them on the UI thread for large files.
- Multi-cursor is supported; overlapping selections merge before an edit.
- Regex patterns run in the native ECMAScript engine; pathological patterns are
  not sandboxed.
- Structural syntax is Python and JavaScript only; other languages use the
  lexical tokenizer.
- No semantic intelligence or runtime management. Execution runs only what the
  host supplies; no interpreter is bundled.
- Android plugin publishing needs the native sources vendored or an AAR; local
  path integration in this repository works from the shared source tree.
