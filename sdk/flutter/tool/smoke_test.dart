// Zero-dependency smoke test for the Dart FFI SDK.
//
// Run from sdk/flutter after building the native library:
//   CODECORE_LIBRARY_PATH=../../build/native/libcodecore.dll \
//     dart run tool/smoke_test.dart
//
// On Linux/macOS use libcodecore.so / libcodecore.dylib.

import 'dart:io';

import 'package:codecore/codecore.dart';

int checks = 0;
int failures = 0;

void check(bool condition, String description) {
  checks += 1;
  if (!condition) {
    failures += 1;
    stdout.writeln('FAIL: $description');
  }
}

bool containsToken(List<TokenSpan> tokens, TokenKind kind) =>
    tokens.any((token) => token.kind == kind);

bool containsLabel(List<CompletionItem> items, String label) =>
    items.any((item) => item.label == label);

void main() {
  CodeCore? core;
  try {
    core = CodeCore.open();
    check(core.version == '0.2.0', 'native version is 0.2.0');
    check(core.abiVersion == 2, 'C ABI version is 2');
    check(core.languageCount == 23, 'language registry has 23 languages');
    check(
      core.languages.any((language) => language.idString == 'python'),
      'python is registered',
    );
    check(core.findLanguage('kotlin') == 1, 'kotlin language id');
    check(core.findLanguage('brainfuck') == null, 'unknown language is rejected');

    final document = core.createDocument(language: 'python');
    const source = '# demo\n'
        'def calculate_total(price, quantity):\n'
        '    """Return the total."""\n'
        '    return price * quantity\n'
        '\n'
        'print(calculate_total(2, 3))\n';
    document.text = source;
    check(document.text == source, 'set/get text round-trips');
    check(document.length == source.length, 'length is byte length');
    check(document.lineCount == 7, 'line count includes trailing empty line');

    final tokens = document.tokenize();
    check(containsToken(tokens, TokenKind.comment), 'comment token');
    check(containsToken(tokens, TokenKind.keyword), 'keyword token');
    check(containsToken(tokens, TokenKind.function), 'function token');
    check(containsToken(tokens, TokenKind.string), 'string token');
    check(containsToken(tokens, TokenKind.number), 'number token');

    final position = document.offsetToPosition(source.indexOf('return'));
    check(position.line == 3, 'offset to line position');
    check(document.positionToOffset(3, 4) == source.indexOf('return'), 'position to offset');

    document.insert(0, 'X');
    check(document.text.startsWith('X# demo'), 'insert at start');
    check(document.undo(), 'undo reports an edit');
    check(document.text == source, 'undo restores text');
    check(document.redo(), 'redo reports an edit');
    check(document.text.startsWith('X# demo'), 'redo restores insert');
    document.clearHistory();
    check(!document.canUndo, 'clear history');

    final completionDoc = core.createDocument(language: 'python');
    completionDoc.text = 'def make_widget(name):\n    return name\n\nmake_';
    final items = completionDoc.complete();
    check(containsLabel(items, 'make_widget'), 'symbol completion');
    check(items.length <= 100, 'completion result is capped');
    completionDoc.dispose();

    final brokenDoc = core.createDocument(language: 'python');
    brokenDoc.text = 'def broken(:\n    pass\n';
    final diagnostics = brokenDoc.diagnose();
    check(diagnostics.isNotEmpty, 'unbalanced code produces diagnostics');
    check(
      diagnostics.any((diagnostic) => diagnostic.message.contains('Unclosed')),
      'unclosed bracket diagnostic',
    );
    brokenDoc.dispose();

    final jsDoc = core.createDocument(language: 'javascript');
    jsDoc.text = 'function outer() {\n  if (ready) {\n    work();\n  }\n}\n';
    final folds = jsDoc.foldingRanges();
    check(folds.length >= 2, 'nested brace folding ranges');
    check(folds.every((range) => range.endLine > range.startLine), 'fold ranges are valid');

    final findDoc = core.createDocument(language: 'javascript');
    findDoc.text = 'Hello hello world\nhell\n';
    check(findDoc.find('hello').length == 2, 'case-insensitive find');
    check(findDoc.find('hello', caseSensitive: true).length == 1, 'case-sensitive find');
    check(findDoc.find('hell', wholeWord: true).length == 1, 'whole-word find');
    findDoc.dispose();

    final bracketDoc = core.createDocument(language: 'javascript');
    bracketDoc.text = 'foo(bar[1])';
    check(bracketDoc.matchBracket(3) == 10, 'matching bracket');
    check(bracketDoc.matchBracket(4) == 10, 'caret after bracket resolves');
    check(bracketDoc.matchBracket(4 + 1) == null, 'non-bracket returns null');
    bracketDoc.dispose();

    var events = 0;
    ChangeEvent? lastEvent;
    document.onChange = (event) {
      events += 1;
      lastEvent = event;
    };
    document.insert(document.length, '\n');
    check(events == 1, 'change listener fired');
    check(lastEvent?.kind == ChangeKind.insert, 'change kind is insert');
    document.onChange = null;
    document.dispose();

    check(core.languages.every((language) => language.supportsSyntax), 'all languages expose syntax');
    check(core.languages.every((language) => language.supportsCompletion), 'all languages expose completion');
  } on Object catch (error, stackTrace) {
    failures += 1;
    stdout.writeln('FATAL: $error');
    stdout.writeln(stackTrace);
  } finally {
    core?.dispose();
  }

  stdout.writeln('Dart FFI smoke test: $checks checks, $failures failures');
  exitCode = failures == 0 ? 0 : 1;
}
