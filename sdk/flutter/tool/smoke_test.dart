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

ExecutionEvent? drainExecution(
    CodeCoreExecution execution, StringBuffer output) {
  final deadline = DateTime.now().add(const Duration(seconds: 5));
  while (DateTime.now().isBefore(deadline)) {
    final event = execution.poll();
    if (event == null) {
      sleep(const Duration(milliseconds: 2));
      continue;
    }
    if (event.kind == ExecutionEventKind.output) {
      output.write(event.data);
    } else {
      return event;
    }
  }
  return null;
}

void main() {
  CodeCore? core;
  try {
    core = CodeCore.open();
    check(core.version == '0.5.0', 'native version is 0.5.0');
    check(core.abiVersion == 6, 'C ABI version is 6');
    check(core.languageCount == 40, 'language registry has 40 languages');
    check(core.findLanguage('perl') == 23, 'perl language id');
    check(core.findLanguage('scala') == 25, 'scala language id');
    check(core.findLanguage('haskell') == 27, 'haskell language id');
    check(core.findLanguage('fortran') == 39, 'fortran language id');
    check(
      core.languages.any((language) => language.idString == 'python'),
      'python is registered',
    );
    check(core.findLanguage('kotlin') == 1, 'kotlin language id');
    check(
        core.findLanguage('brainfuck') == null, 'unknown language is rejected');

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
    check(core.supportsStructuralSyntax('python'), 'python grammar is bundled');
    check(
      core.supportsStructuralSyntax('javascript'),
      'javascript grammar is bundled',
    );
    check(core.supportsStructuralSyntax('java'), 'java grammar is bundled');
    check(core.supportsStructuralSyntax('go'), 'go grammar is bundled');
    check(core.supportsStructuralSyntax('json'), 'json grammar is bundled');
    check(core.supportsStructuralSyntax('rust'), 'rust grammar is bundled');
    check(
      !core.supportsStructuralSyntax('kotlin'),
      'unbundled grammar is reported as missing',
    );

    final javaDocument = core.createDocument(language: 'java');
    javaDocument.text =
        'class Demo { int add(int a, int b) { return a + b; } }\n';
    final javaSyntax = javaDocument.syntaxTree();
    check(javaSyntax != null, 'java syntax tree is available');
    if (javaSyntax != null) {
      check(javaSyntax.language == 'java', 'java tree language');
      check(javaSyntax.rootType == 'program', 'java tree root');
      check(!javaSyntax.hasErrors, 'valid java has no syntax errors');
      check(
        javaSyntax.highlights.any((h) => h.capture == 'keyword'),
        'java keyword highlight',
      );
      javaSyntax.dispose();
    }
    javaDocument.dispose();

    final syntax = document.syntaxTree();
    check(syntax != null, 'python syntax tree is available');
    if (syntax != null) {
      check(syntax.language == 'python', 'syntax tree language');
      check(syntax.rootType == 'module', 'syntax tree root');
      check(syntax.nodeCount > 10, 'syntax tree node count');
      check(!syntax.hasErrors, 'valid source has no syntax errors');
      check(syntax.errors.isEmpty, 'valid source error list is empty');
      final highlights = syntax.highlights;
      check(highlights.isNotEmpty, 'syntax highlights are present');
      check(
        highlights.any((highlight) => highlight.capture == 'keyword'),
        'keyword syntax highlight',
      );
      check(
        highlights.any((highlight) => highlight.capture == 'function'),
        'function syntax highlight',
      );
      check(
        highlights.every((highlight) => highlight.start < highlight.end),
        'syntax highlight ranges are valid',
      );
      syntax.dispose();
    }

    final jsSyntaxDoc = core.createDocument(language: 'javascript');
    jsSyntaxDoc.text = '// note\n'
        'function add(a, b) {\n'
        '  return a + b;\n'
        '}\n'
        "add(1, 2);\n";
    final jsSyntax = jsSyntaxDoc.syntaxTree();
    check(jsSyntax != null, 'javascript syntax tree is available');
    if (jsSyntax != null) {
      check(jsSyntax.language == 'javascript', 'javascript tree language');
      check(jsSyntax.rootType == 'program', 'javascript tree root');
      check(!jsSyntax.hasErrors, 'valid javascript has no syntax errors');
      check(
        jsSyntax.highlights.any((highlight) => highlight.capture == 'keyword'),
        'javascript keyword highlight',
      );
      jsSyntax.dispose();
    }
    jsSyntaxDoc.dispose();

    final noGrammarDoc = core.createDocument(language: 'kotlin');
    check(noGrammarDoc.syntaxTree() == null, 'no grammar means no syntax tree');
    noGrammarDoc.dispose();

    final position = document.offsetToPosition(source.indexOf('return'));
    check(position.line == 3, 'offset to line position');
    check(document.positionToOffset(3, 4) == source.indexOf('return'),
        'position to offset');

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
    final brokenSyntax = brokenDoc.syntaxTree();
    check(brokenSyntax != null, 'broken python still produces a tree');
    if (brokenSyntax != null) {
      check(brokenSyntax.hasErrors, 'broken source reports syntax errors');
      check(brokenSyntax.errors.isNotEmpty, 'broken source error list');
      check(
        brokenSyntax.errors.first.nodeType == 'ERROR' ||
            brokenSyntax.errors.first.missing,
        'syntax error is an ERROR or MISSING node',
      );
      brokenSyntax.dispose();
    }
    brokenDoc.dispose();

    final jsDoc = core.createDocument(language: 'javascript');
    jsDoc.text = 'function outer() {\n  if (ready) {\n    work();\n  }\n}\n';
    final folds = jsDoc.foldingRanges();
    check(folds.length >= 2, 'nested brace folding ranges');
    check(folds.every((range) => range.endLine > range.startLine),
        'fold ranges are valid');

    final findDoc = core.createDocument(language: 'javascript');
    findDoc.text = 'Hello hello world\nhell\n';
    check(findDoc.find('hello').length == 2, 'case-insensitive find');
    check(findDoc.find('hello', caseSensitive: true).length == 1,
        'case-sensitive find');
    check(findDoc.find('hell', wholeWord: true).length == 1, 'whole-word find');
    check(findDoc.find(r'h.llo', regex: true).length == 2, 'regex find');
    check(
      findDoc.find(r'^hell', regex: true).length == 2,
      'multiline regex anchors',
    );
    var invalidRegexThrew = false;
    try {
      findDoc.find('(', regex: true);
    } on CodeCoreException {
      invalidRegexThrew = true;
    }
    check(invalidRegexThrew, 'invalid regex throws');
    findDoc.dispose();

    final multiDoc = core.createDocument(language: 'python');
    multiDoc.text = 'one\ntwo\nthree\n';
    multiDoc.setSelection(0, 0);
    multiDoc.addCursor(4, 4);
    check(multiDoc.cursorCount == 2, 'multi cursor count');
    check(multiDoc.cursorAt(1).focus == 4, 'multi cursor offset');
    multiDoc.insertAtCursors('# ');
    check(multiDoc.text == '# one\n# two\nthree\n', 'multi cursor insert');
    check(multiDoc.selection.focus == 2, 'primary caret after multi insert');
    check(
        multiDoc.cursorAt(1).focus == 8, 'secondary caret after multi insert');
    check(multiDoc.undo(), 'multi insert undo');
    check(multiDoc.text == 'one\ntwo\nthree\n', 'multi insert undo text');
    check(multiDoc.redo(), 'multi insert redo');
    check(multiDoc.undo(), 'multi insert undo again');

    multiDoc.setSelection(0, 3);
    multiDoc.addCursor(4, 7);
    multiDoc.eraseAtCursors();
    check(multiDoc.text == '\n\nthree\n', 'multi selection erase');
    check(multiDoc.cursorCount == 2, 'multi erase keeps cursors');
    check(multiDoc.undo(), 'multi erase undo');
    check(multiDoc.text == 'one\ntwo\nthree\n', 'multi erase undo text');
    multiDoc.clearCursors();
    check(multiDoc.cursorCount == 1, 'clear cursors');
    multiDoc.dispose();

    final bracketDoc = core.createDocument(language: 'javascript');
    bracketDoc.text = 'foo(bar[1])';
    check(bracketDoc.matchBracket(3) == 10, 'matching bracket');
    check(bracketDoc.matchBracket(4) == 10, 'caret after bracket resolves');
    check(bracketDoc.matchBracket(4 + 1) == null, 'non-bracket returns null');
    bracketDoc.dispose();

    final agentDoc = core.createDocument(language: 'python');
    agentDoc.text = 'one\ntwo\n';
    agentDoc.editOrigin = EditOrigin.user;
    var agentEvents = 0;
    ChangeEvent? agentEvent;
    agentDoc.onChange = (event) {
      agentEvents += 1;
      agentEvent = event;
    };
    agentDoc.insert(0, '# ');
    check(agentEvents > 0, 'agent document change listener fired');
    check(agentEvent?.origin == EditOrigin.user, 'change event origin');
    check(agentDoc.undo(), 'undo user edit');

    agentDoc.setProposal('one\nchanged\nthree\n', origin: EditOrigin.ai);
    check(agentDoc.hasProposal, 'proposal is stored');
    check(agentDoc.text == 'one\ntwo\n', 'proposal does not mutate document');
    final diff = agentDoc.proposalDiff;
    check(diff.any((line) => line.kind == DiffKind.insert),
        'proposal diff has insertions');
    check(diff.any((line) => line.kind == DiffKind.delete),
        'proposal diff has deletions');
    agentDoc.acceptProposal();
    check(agentDoc.text == 'one\nchanged\nthree\n',
        'proposal acceptance is atomic');
    check(agentEvent?.kind == ChangeKind.proposalAccepted,
        'proposal accepted event');
    check(agentEvent?.origin == EditOrigin.ai, 'proposal origin is AI');
    check(agentDoc.undo(), 'proposal acceptance is one undo step');
    check(agentDoc.text == 'one\ntwo\n', 'undo restores pre-proposal text');

    agentDoc.showGhostText(agentDoc.length, 'return 1', origin: EditOrigin.ai);
    check(agentDoc.ghostText?.text == 'return 1', 'ghost text is exposed');
    check(agentDoc.text == 'one\ntwo\n', 'ghost text is not inserted');
    agentDoc.acceptGhostText(maxCodePoints: 6);
    check(agentDoc.text.endsWith('return'), 'partial ghost acceptance');
    check(agentEvent?.kind == ChangeKind.ghostTextAccepted,
        'ghost accepted event');
    agentDoc.undo();
    agentDoc.showGhostText(0, 'x');
    agentDoc.dismissGhostText();
    check(agentDoc.ghostText == null, 'ghost text dismissal');

    final snapshot = agentDoc.createSnapshot();
    agentDoc.text = 'replacement\n';
    agentDoc.restoreSnapshot(snapshot);
    check(agentDoc.text == 'one\ntwo\n', 'snapshot restore');
    check(agentEvent?.kind == ChangeKind.snapshotRestored,
        'snapshot restore event');
    check(agentDoc.undo(), 'snapshot restore is undoable');
    check(agentDoc.text == 'replacement\n', 'undo after snapshot restore');
    snapshot.dispose();

    agentDoc.editable = false;
    agentDoc.setProposal('blocked\n');
    var readOnlyFailed = false;
    try {
      agentDoc.acceptProposal();
    } on CodeCoreException {
      readOnlyFailed = true;
    }
    check(readOnlyFailed, 'read-only proposal acceptance is rejected');
    agentDoc.editable = true;
    agentDoc.onChange = null;
    agentDoc.dispose();

    var events = 0;
    ChangeEvent? lastEvent;
    document.onChange = (event) {
      events += 1;
      lastEvent = event;
    };
    document.insert(document.length, '\n');
    check(events == 1, 'change listener fired');
    check(lastEvent?.kind == ChangeKind.insert, 'change kind is insert');
    check(lastEvent?.origin == EditOrigin.host, 'default origin is host');
    document.onChange = null;
    document.dispose();

    final lsp = core.createLsp();
    lsp.initialize('file:///project');
    final initializeFrame = lsp.takeOutgoing();
    check(
      initializeFrame != null && initializeFrame.contains('initialize'),
      'LSP initialize frame',
    );
    const capabilitiesBody = '{"jsonrpc":"2.0","id":1,"result":'
        '{"capabilities":{"hoverProvider":true}}}';
    lsp.feed(
      'Content-Length: ${capabilitiesBody.length}\r\n\r\n$capabilitiesBody',
    );
    check(lsp.isInitialized, 'LSP initialized after response');
    check(
      lsp.serverCapabilities.contains('hoverProvider'),
      'LSP server capabilities',
    );
    lsp.didOpen('file:///project/main.py', 'python', 1, 'print(1)\n');
    String? didOpenFrame;
    for (var attempt = 0; attempt < 4; attempt += 1) {
      didOpenFrame = lsp.takeOutgoing();
      if (didOpenFrame != null &&
          didOpenFrame.contains('textDocument/didOpen')) {
        break;
      }
    }
    check(
      didOpenFrame != null && didOpenFrame.contains('textDocument/didOpen'),
      'LSP didOpen frame',
    );
    lsp.dispose();

    final execution = core.startExecution(
      command: 'echo codecore-exec',
      shell: true,
    );
    final executionOutput = StringBuffer();
    final executionExit = drainExecution(execution, executionOutput);
    check(executionExit != null, 'execution exit event');
    check(executionExit?.exitCode == 0, 'execution exit code');
    check(executionOutput.toString().contains('codecore-exec'),
        'execution stdout');
    check(!execution.isRunning, 'execution is finished');
    execution.dispose();

    final exitExecution = core.startExecution(command: 'exit 3', shell: true);
    final exitEvent = drainExecution(exitExecution, StringBuffer());
    check(exitEvent?.exitCode == 3, 'execution nonzero exit');
    exitExecution.dispose();

    var executionStartFailed = false;
    try {
      core.startExecution(command: 'codecore-command-does-not-exist-xyz');
    } on CodeCoreException {
      executionStartFailed = true;
    }
    check(executionStartFailed, 'execution start failure throws');

    check(core.languages.every((language) => language.supportsSyntax),
        'all languages expose syntax');
    check(core.languages.every((language) => language.supportsCompletion),
        'all languages expose completion');
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
