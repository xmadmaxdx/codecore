/// High-level Dart API for the CodeCore native engine.
library;

import 'dart:convert';
import 'dart:ffi';
import 'dart:io';


import 'models.dart';
import 'native_bindings.dart';

/// Entry point for the CodeCore engine.
///
/// [open] loads the native shared library, creates one engine instance and
/// exposes its immutable language registry. Documents are created with
/// [createDocument] and must be disposed by the caller.
class CodeCore {
  CodeCore._(this._bindings, this._engine);

  final CodeCoreBindings _bindings;
  final Pointer<Void> _engine;
  bool _disposed = false;
  List<CodeCoreLanguage>? _languageCache;

  /// Loads the native library from an explicit path, `CODECORE_LIBRARY_PATH`,
  /// the platform loader path, or common local build directories.
  ///
  /// Throws [StateError] if the library cannot be opened and [CodeCoreException]
  /// if the native engine cannot be created.
  static CodeCore open({String? libraryPath}) {
    final library = _openLibrary(libraryPath);
    final bindings = CodeCoreBindings(library);
    final config = bindings
        .allocate(sizeOf<CcEngineConfig>())
        .cast<CcEngineConfig>();
    config.ref.structSize = sizeOf<CcEngineConfig>();
    config.ref.maxDocumentBytes = 64 * 1024 * 1024;
    final engine = bindings.engineCreate(config);
    bindings.freePointer(config);
    if (engine == nullptr) {
      throw const CodeCoreException(7, 'native engine creation failed');
    }
    return CodeCore._(bindings, engine);
  }

  String get version {
    _ensureAlive();
    return _bindings.nativeVersion;
  }

  int get abiVersion {
    _ensureAlive();
    return _bindings.abiVersion();
  }

  int get languageCount {
    _ensureAlive();
    return _bindings.engineLanguageCount(_engine);
  }

  List<CodeCoreLanguage> get languages {
    _ensureAlive();
    final cached = _languageCache;
    if (cached != null) {
      return List<CodeCoreLanguage>.unmodifiable(cached);
    }
    final count = _bindings.engineLanguageCount(_engine);
    final loaded = <CodeCoreLanguage>[];
    for (var index = 0; index < count; index += 1) {
      final info = _bindings.engineLanguageAt(_engine, index);
      if (info == nullptr) {
        continue;
      }
      final value = info.ref;
      loaded.add(CodeCoreLanguage(
        id: value.id,
        idString: _bindings.readCString(value.idString),
        displayName: _bindings.readCString(value.displayName),
        extensions: _bindings.readCString(value.extensions),
        capabilities: value.capabilities,
      ));
    }
    _languageCache = loaded;
    return List<CodeCoreLanguage>.unmodifiable(loaded);
  }

  /// Returns the native language id, or null when the language is unknown.
  int? findLanguage(String id) {
    _ensureAlive();
    final pointer = _bindings.allocateUtf8(id);
    try {
      final value = _bindings.engineFindLanguage(_engine, pointer);
      return value < 0 ? null : value;
    } finally {
      _bindings.freePointer(pointer);
    }
  }

  /// Creates a document. [language] must be a registered language id string.
  CodeCoreDocument createDocument({
    String language = 'python',
    int tabSize = 4,
    bool insertSpaces = true,
    bool readOnly = false,
  }) {
    _ensureAlive();
    final languageId = findLanguage(language);
    if (languageId == null) {
      throw ArgumentError.value(language, 'language', 'unknown CodeCore language');
    }
    final options = _bindings
        .allocate(sizeOf<CcDocumentOptions>())
        .cast<CcDocumentOptions>();
    options.ref.structSize = sizeOf<CcDocumentOptions>();
    options.ref.languageId = languageId;
    options.ref.tabSize = tabSize;
    options.ref.insertSpaces = insertSpaces ? 1 : 0;
    options.ref.readOnly = readOnly ? 1 : 0;
    final handle = _bindings.documentCreate(_engine, options);
    _bindings.freePointer(options);
    if (handle == nullptr) {
      throw const CodeCoreException(5, 'document creation failed');
    }
    return CodeCoreDocument._(this, handle);
  }

  void dispose() {
    if (_disposed) {
      return;
    }
    _disposed = true;
    _bindings.engineDestroy(_engine);
  }

  void _ensureAlive() {
    if (_disposed) {
      throw StateError('CodeCore engine has been disposed');
    }
  }

  static DynamicLibrary _openLibrary(String? explicitPath) {
    final candidates = <String>[];
    if (explicitPath != null && explicitPath.isNotEmpty) {
      candidates.add(explicitPath);
    }
    final fromEnvironment = Platform.environment['CODECORE_LIBRARY_PATH'];
    if (fromEnvironment != null && fromEnvironment.isNotEmpty) {
      candidates.add(fromEnvironment);
    }
    if (Platform.isWindows) {
      candidates.addAll(<String>[
        'libcodecore.dll',
        'codecore.dll',
        'build/native/libcodecore.dll',
        'build/native/codecore.dll',
        '../../build/native/libcodecore.dll',
        '../build/native/libcodecore.dll',
      ]);
    } else if (Platform.isMacOS) {
      candidates.addAll(<String>['libcodecore.dylib', 'build/native/libcodecore.dylib']);
    } else {
      candidates.addAll(<String>['libcodecore.so', 'build/native/libcodecore.so']);
    }

    final errors = <String>[];
    for (final candidate in candidates) {
      try {
        return DynamicLibrary.open(candidate);
      } on ArgumentError catch (error) {
        errors.add('$candidate ($error)');
      }
    }
    throw StateError(
      'Unable to open the CodeCore native library. Build it with '
      '"cmake -S . -B build/native && cmake --build build/native" or set '
      'CODECORE_LIBRARY_PATH. Tried: ${errors.join(', ')}',
    );
  }
}

/// A single editable document.
///
/// Offsets are UTF-8 byte offsets. Strings cross the FFI boundary as UTF-8 and
/// are converted to Dart strings. The document owns native memory and must be
/// disposed.
class CodeCoreDocument {
  CodeCoreDocument._(this._core, this._handle);

  final CodeCore _core;
  final Pointer<Void> _handle;
  bool _disposed = false;
  NativeCallable<ChangeCallbackNative>? _changeCallable;
  void Function(ChangeEvent)? _changeHandler;

  CodeCoreBindings get _bindings => _core._bindings;

  String get text {
    _ensureAlive();
    final lengthPointer = _bindings.allocate(sizeOf<IntPtr>()).cast<IntPtr>();
    final textPointer = _bindings.documentGetText(_handle, lengthPointer);
    try {
      if (textPointer == nullptr) {
        return '';
      }
      final length = lengthPointer.value;
      final bytes = textPointer.asTypedList(length);
      return utf8.decode(bytes, allowMalformed: true);
    } finally {
      if (textPointer != nullptr) {
        _bindings.stringFree(textPointer);
      }
      _bindings.freePointer(lengthPointer);
    }
  }

  set text(String value) {
    _ensureAlive();
    final encoded = utf8.encode(value);
    final pointer = _bindings.allocate(encoded.length + 1);
    pointer.asTypedList(encoded.length).setAll(0, encoded);
    try {
      _check(_bindings.documentSetText(_handle, pointer, encoded.length));
    } finally {
      _bindings.freePointer(pointer);
    }
  }

  int get length {
    _ensureAlive();
    return _bindings.documentLength(_handle);
  }

  int get version {
    _ensureAlive();
    return _bindings.documentVersion(_handle);
  }

  int get languageId {
    _ensureAlive();
    return _bindings.documentLanguage(_handle);
  }

  set languageId(int value) {
    _ensureAlive();
    _check(_bindings.documentSetLanguage(_handle, value));
  }

  bool get readOnly {
    _ensureAlive();
    return _bindings.documentIsReadOnly(_handle) != 0;
  }

  set readOnly(bool value) {
    _ensureAlive();
    _check(_bindings.documentSetReadOnly(_handle, value ? 1 : 0));
  }

  bool get editable => !readOnly;

  set editable(bool value) {
    _ensureAlive();
    _check(_bindings.documentSetEditable(_handle, value ? 1 : 0));
  }

  EditOrigin get editOrigin {
    _ensureAlive();
    return EditOrigin.fromNative(_bindings.documentEditOrigin(_handle));
  }

  set editOrigin(EditOrigin value) {
    _ensureAlive();
    _check(_bindings.documentSetEditOrigin(_handle, value.nativeValue));
  }

  bool get hasProposal {
    _ensureAlive();
    return _bindings.documentHasProposal(_handle) != 0;
  }

  /// Preview content that does not modify the document until accepted.
  String? get proposal {
    _ensureAlive();
    if (!hasProposal) {
      return null;
    }
    final lengthPointer = _bindings.allocate(sizeOf<IntPtr>()).cast<IntPtr>();
    final textPointer = _bindings.documentGetProposedText(_handle, lengthPointer);
    try {
      if (textPointer == nullptr) {
        return '';
      }
      return utf8.decode(
        textPointer.asTypedList(lengthPointer.value),
        allowMalformed: true,
      );
    } finally {
      if (textPointer != nullptr) {
        _bindings.stringFree(textPointer);
      }
      _bindings.freePointer(lengthPointer);
    }
  }

  set proposal(String? value) {
    _ensureAlive();
    if (value == null) {
      rejectProposal();
      return;
    }
    final encoded = utf8.encode(value);
    final pointer = _bindings.allocate(encoded.length + 1);
    pointer.asTypedList(encoded.length).setAll(0, encoded);
    try {
      _check(_bindings.documentSetProposedText(
        _handle,
        pointer,
        encoded.length,
        EditOrigin.ai.nativeValue,
      ));
    } finally {
      _bindings.freePointer(pointer);
    }
  }

  void setProposal(String text, {EditOrigin origin = EditOrigin.ai}) {
    _ensureAlive();
    final encoded = utf8.encode(text);
    final pointer = _bindings.allocate(encoded.length + 1);
    pointer.asTypedList(encoded.length).setAll(0, encoded);
    try {
      _check(_bindings.documentSetProposedText(
        _handle,
        pointer,
        encoded.length,
        origin.nativeValue,
      ));
    } finally {
      _bindings.freePointer(pointer);
    }
  }

  List<DiffLine> get proposalDiff {
    _ensureAlive();
    if (!hasProposal) {
      return const [];
    }
    final slot = _bindings
        .allocate(sizeOf<Pointer<CcDiffLine>>())
        .cast<Pointer<CcDiffLine>>();
    final count = _bindings.documentProposalDiff(_handle, slot);
    final pointer = slot.value;
    final lines = <DiffLine>[];
    try {
      for (var index = 0; index < count; index += 1) {
        final line = (pointer + index).ref;
        lines.add(DiffLine(
          kind: DiffKind.fromNative(line.kind),
          oldLine: line.oldLine == 0xFFFFFFFF ? -1 : line.oldLine,
          newLine: line.newLine == 0xFFFFFFFF ? -1 : line.newLine,
          text: _bindings.readCString(line.text),
        ));
      }
    } finally {
      if (pointer != nullptr) {
        _bindings.diffLinesFree(pointer, count);
      }
      _bindings.freePointer(slot);
    }
    return lines;
  }

  void acceptProposal() {
    _ensureAlive();
    _check(_bindings.documentAcceptProposal(_handle));
  }

  void rejectProposal() {
    _ensureAlive();
    _check(_bindings.documentRejectProposal(_handle));
  }

  /// The current host-provided inline suggestion, if any.
  GhostText? get ghostText {
    _ensureAlive();
    final offsetPointer = _bindings.allocate(sizeOf<IntPtr>()).cast<IntPtr>();
    final lengthPointer = _bindings.allocate(sizeOf<IntPtr>()).cast<IntPtr>();
    final originPointer = _bindings.allocate(sizeOf<Uint32>()).cast<Uint32>();
    final textPointer = _bindings.documentGetGhostText(
      _handle,
      offsetPointer,
      lengthPointer,
      originPointer,
    );
    try {
      if (textPointer == nullptr) {
        return null;
      }
      return GhostText(
        offset: offsetPointer.value,
        text: utf8.decode(
          textPointer.asTypedList(lengthPointer.value),
          allowMalformed: true,
        ),
        origin: EditOrigin.fromNative(originPointer.value),
      );
    } finally {
      if (textPointer != nullptr) {
        _bindings.stringFree(textPointer);
      }
      _bindings.freePointer(originPointer);
      _bindings.freePointer(lengthPointer);
      _bindings.freePointer(offsetPointer);
    }
  }

  void showGhostText(
    int offset,
    String text, {
    EditOrigin origin = EditOrigin.ai,
  }) {
    _ensureAlive();
    final encoded = utf8.encode(text);
    final pointer = _bindings.allocate(encoded.length + 1);
    pointer.asTypedList(encoded.length).setAll(0, encoded);
    try {
      _check(_bindings.documentSetGhostText(
        _handle,
        offset,
        pointer,
        encoded.length,
        origin.nativeValue,
      ));
    } finally {
      _bindings.freePointer(pointer);
    }
  }

  /// Accepts all ghost text, or only [maxCodePoints] leading code points.
  void acceptGhostText({int maxCodePoints = 0}) {
    _ensureAlive();
    _check(_bindings.documentAcceptGhostText(_handle, maxCodePoints));
  }

  void dismissGhostText() {
    _ensureAlive();
    _check(_bindings.documentDismissGhostText(_handle));
  }

  CodeCoreSnapshot createSnapshot() {
    _ensureAlive();
    final handle = _bindings.snapshotCreate(_handle);
    if (handle == nullptr) {
      throw const CodeCoreException(5, 'snapshot creation failed');
    }
    return CodeCoreSnapshot._(_bindings, handle);
  }

  void restoreSnapshot(CodeCoreSnapshot snapshot) {
    _ensureAlive();
    _check(_bindings.documentRestoreSnapshot(_handle, snapshot._handle));
  }

  int get tabSize {
    _ensureAlive();
    return _bindings.documentTabSize(_handle);
  }

  String get lastError {
    _ensureAlive();
    return _bindings.readCString(_bindings.documentLastError(_handle));
  }

  void insert(int offset, String text) {
    _ensureAlive();
    final encoded = utf8.encode(text);
    final pointer = _bindings.allocate(encoded.length + 1);
    pointer.asTypedList(encoded.length).setAll(0, encoded);
    try {
      _check(_bindings.documentInsert(_handle, offset, pointer, encoded.length));
    } finally {
      _bindings.freePointer(pointer);
    }
  }

  void erase(int start, int end) {
    _ensureAlive();
    _check(_bindings.documentErase(_handle, start, end));
  }

  void replace(int start, int end, String text) {
    _ensureAlive();
    final encoded = utf8.encode(text);
    final pointer = _bindings.allocate(encoded.length + 1);
    pointer.asTypedList(encoded.length).setAll(0, encoded);
    try {
      _check(_bindings.documentReplace(_handle, start, end, pointer, encoded.length));
    } finally {
      _bindings.freePointer(pointer);
    }
  }

  bool get canUndo {
    _ensureAlive();
    return _bindings.documentCanUndo(_handle) != 0;
  }

  bool get canRedo {
    _ensureAlive();
    return _bindings.documentCanRedo(_handle) != 0;
  }

  /// Returns true when an edit was undone.
  bool undo() {
    _ensureAlive();
    if (!canUndo) {
      return false;
    }
    _check(_bindings.documentUndo(_handle));
    return true;
  }

  /// Returns true when an edit was redone.
  bool redo() {
    _ensureAlive();
    if (!canRedo) {
      return false;
    }
    _check(_bindings.documentRedo(_handle));
    return true;
  }

  void clearHistory() {
    _ensureAlive();
    _bindings.documentClearHistory(_handle);
  }

  void setHistoryLimit(int limit) {
    _ensureAlive();
    _bindings.documentSetHistoryLimit(_handle, limit);
  }

  int get lineCount {
    _ensureAlive();
    return _bindings.documentLineCount(_handle);
  }

  LineInfo lineAt(int index) {
    _ensureAlive();
    final pointer = _bindings.allocate(sizeOf<CcLineInfo>()).cast<CcLineInfo>();
    try {
      _check(_bindings.documentLineAt(_handle, index, pointer));
      final value = pointer.ref;
      return LineInfo(
        start: value.start,
        contentEnd: value.contentEnd,
        end: value.end,
      );
    } finally {
      _bindings.freePointer(pointer);
    }
  }

  TextPosition offsetToPosition(int offset) {
    _ensureAlive();
    final pointer = _bindings.allocate(sizeOf<CcPosition>()).cast<CcPosition>();
    try {
      _check(_bindings.documentOffsetToPosition(_handle, offset, pointer));
      final value = pointer.ref;
      return TextPosition(line: value.line, column: value.column);
    } finally {
      _bindings.freePointer(pointer);
    }
  }

  int positionToOffset(int line, int column) {
    _ensureAlive();
    final pointer = _bindings.allocate(sizeOf<IntPtr>()).cast<IntPtr>();
    try {
      _check(_bindings.documentPositionToOffset(_handle, line, column, pointer));
      return pointer.value;
    } finally {
      _bindings.freePointer(pointer);
    }
  }

  Selection get selection {
    _ensureAlive();
    final value = _bindings.documentGetSelection(_handle);
    return Selection(anchor: value.anchor, focus: value.focus);
  }

  void setSelection(int anchor, int focus) {
    _ensureAlive();
    _check(_bindings.documentSetSelection(_handle, anchor, focus));
  }

  void moveCursor(CursorMove move, {bool extendSelection = false}) {
    _ensureAlive();
    _check(_bindings.documentMoveCursor(
      _handle,
      move.index,
      extendSelection ? 1 : 0,
    ));
  }

  /// Returns the matching bracket offset, or null when the caret is not on a
  /// bracket or no partner exists.
  int? matchBracket(int offset) {
    _ensureAlive();
    final pointer = _bindings.allocate(sizeOf<IntPtr>()).cast<IntPtr>();
    try {
      final status = _bindings.documentMatchBracket(_handle, offset, pointer);
      if (status == 0) {
        return pointer.value;
      }
      if (status == 6) {
        return null;
      }
      _check(status);
      return null;
    } finally {
      _bindings.freePointer(pointer);
    }
  }

  List<TokenSpan> tokenize({int start = 0, int? end}) {
    _ensureAlive();
    final actualEnd = end ?? length;
    final slot = _bindings
        .allocate(sizeOf<Pointer<CcToken>>())
        .cast<Pointer<CcToken>>();
    final count = _bindings.documentTokenize(_handle, start, actualEnd, slot);
    final pointer = slot.value;
    final tokens = <TokenSpan>[];
    try {
      for (var index = 0; index < count; index += 1) {
        final token = (pointer + index).ref;
        tokens.add(TokenSpan(
          start: token.start,
          end: token.end,
          kind: TokenKind.fromNative(token.kind),
        ));
      }
    } finally {
      if (pointer != nullptr) {
        _bindings.tokensFree(pointer);
      }
      _bindings.freePointer(slot);
    }
    return tokens;
  }

  List<CompletionItem> complete([int? offset]) {
    _ensureAlive();
    final target = offset ?? length;
    final slot = _bindings
        .allocate(sizeOf<Pointer<CcCompletionItem>>())
        .cast<Pointer<CcCompletionItem>>();
    final count = _bindings.documentComplete(_handle, target, slot);
    final pointer = slot.value;
    final items = <CompletionItem>[];
    try {
      for (var index = 0; index < count; index += 1) {
        final item = (pointer + index).ref;
        items.add(CompletionItem(
          label: _bindings.readCString(item.label),
          insertText: _bindings.readCString(item.insertText),
          detail: _bindings.readCString(item.detail),
          kind: CompletionKind.fromNative(item.kind),
          score: item.score,
        ));
      }
    } finally {
      if (pointer != nullptr) {
        _bindings.completionsFree(pointer, count);
      }
      _bindings.freePointer(slot);
    }
    return items;
  }

  List<CodeCoreDiagnostic> diagnose() {
    _ensureAlive();
    final slot = _bindings
        .allocate(sizeOf<Pointer<CcDiagnostic>>())
        .cast<Pointer<CcDiagnostic>>();
    final count = _bindings.documentDiagnose(_handle, slot);
    final pointer = slot.value;
    final diagnostics = <CodeCoreDiagnostic>[];
    try {
      for (var index = 0; index < count; index += 1) {
        final diagnostic = (pointer + index).ref;
        diagnostics.add(CodeCoreDiagnostic(
          start: diagnostic.start,
          end: diagnostic.end,
          severity: DiagnosticSeverity.fromNative(diagnostic.severity),
          message: _bindings.readCString(diagnostic.message),
          source: _bindings.readCString(diagnostic.source),
        ));
      }
    } finally {
      if (pointer != nullptr) {
        _bindings.diagnosticsFree(pointer, count);
      }
      _bindings.freePointer(slot);
    }
    return diagnostics;
  }

  List<FoldingRange> foldingRanges() {
    _ensureAlive();
    final slot = _bindings
        .allocate(sizeOf<Pointer<CcFoldingRange>>())
        .cast<Pointer<CcFoldingRange>>();
    final count = _bindings.documentFoldingRanges(_handle, slot);
    final pointer = slot.value;
    final ranges = <FoldingRange>[];
    try {
      for (var index = 0; index < count; index += 1) {
        final range = (pointer + index).ref;
        ranges.add(FoldingRange(
          startLine: range.startLine,
          endLine: range.endLine,
          kind: FoldingKind.fromNative(range.kind),
        ));
      }
    } finally {
      if (pointer != nullptr) {
        _bindings.foldingRangesFree(pointer);
      }
      _bindings.freePointer(slot);
    }
    return ranges;
  }

  /// Finds literal text, or ECMAScript regular expressions when [regex] is true.
  ///
  /// Throws [CodeCoreException] with status 1 when [regex] is true and the
  /// pattern is invalid.
  List<SearchMatch> find(
    String query, {
    bool caseSensitive = false,
    bool wholeWord = false,
    bool regex = false,
  }) {
    _ensureAlive();
    final queryBytes = utf8.encode(query);
    final queryPointer = _bindings.allocate(queryBytes.length + 1);
    queryPointer.asTypedList(queryBytes.length).setAll(0, queryBytes);
    final options = _bindings
        .allocate(sizeOf<CcSearchOptions>())
        .cast<CcSearchOptions>();
    options.ref.structSize = sizeOf<CcSearchOptions>();
    options.ref.caseSensitive = caseSensitive ? 1 : 0;
    options.ref.wholeWord = wholeWord ? 1 : 0;
    options.ref.regex = regex ? 1 : 0;
    final slot = _bindings
        .allocate(sizeOf<Pointer<CcRange>>())
        .cast<Pointer<CcRange>>();
    final countSlot = _bindings.allocate(sizeOf<IntPtr>()).cast<IntPtr>();
    try {
      final status = _bindings.documentSearch(
        _handle,
        queryPointer,
        queryBytes.length,
        options,
        slot,
        countSlot,
      );
      if (status != 0) {
        throw CodeCoreException(
          status,
          _bindings.readCString(_bindings.documentLastError(_handle)),
        );
      }
      final count = countSlot.value;
      final pointer = slot.value;
      final matches = <SearchMatch>[];
      try {
        for (var index = 0; index < count; index += 1) {
          final range = (pointer + index).ref;
          matches.add(SearchMatch(start: range.start, end: range.end));
        }
      } finally {
        if (pointer != nullptr) {
          _bindings.searchRangesFree(pointer);
        }
      }
      return matches;
    } finally {
      _bindings.freePointer(countSlot);
      _bindings.freePointer(slot);
      _bindings.freePointer(options);
      _bindings.freePointer(queryPointer);
    }
  }

  /// Set to receive synchronous change notifications. Pass null to detach.
  set onChange(void Function(ChangeEvent)? handler) {
    _ensureAlive();
    _bindings.documentSetChangeListener(_handle, nullptr, nullptr);
    _changeCallable?.close();
    _changeCallable = null;
    _changeHandler = null;
    if (handler == null) {
      return;
    }
    _changeHandler = handler;
    final callable = NativeCallable<ChangeCallbackNative>.isolateLocal(
      (Pointer<Void> userData, Pointer<CcChangeEvent> event) {
        final value = event.ref;
        _changeHandler?.call(ChangeEvent(
          version: value.version,
          kind: ChangeKind.fromNative(value.kind),
          start: value.start,
          removedLength: value.removedLength,
          insertedLength: value.insertedLength,
          origin: EditOrigin.fromNative(value.origin),
        ));
      },
    );
    _changeCallable = callable;
    _bindings.documentSetChangeListener(
      _handle,
      callable.nativeFunction,
      nullptr,
    );
  }

  void dispose() {
    if (_disposed) {
      return;
    }
    _disposed = true;
    if (_changeCallable != null) {
      _bindings.documentSetChangeListener(_handle, nullptr, nullptr);
      _changeCallable!.close();
      _changeCallable = null;
    }
    _bindings.documentDestroy(_handle);
  }

  void _check(int status) {
    if (status != 0) {
      throw CodeCoreException(status, lastError);
    }
  }

  void _ensureAlive() {
    if (_disposed) {
      throw StateError('CodeCoreDocument has been disposed');
    }
  }
}

/// Immutable document snapshot owned by the caller.
class CodeCoreSnapshot {
  CodeCoreSnapshot._(this._bindings, this._handle);

  final CodeCoreBindings _bindings;
  final Pointer<Void> _handle;
  bool _disposed = false;

  int get version {
    _ensureAlive();
    return _bindings.snapshotVersion(_handle);
  }

  int get length {
    _ensureAlive();
    return _bindings.snapshotLength(_handle);
  }

  int get languageId {
    _ensureAlive();
    return _bindings.snapshotLanguage(_handle);
  }

  String get text {
    _ensureAlive();
    final lengthPointer = _bindings.allocate(sizeOf<IntPtr>()).cast<IntPtr>();
    final textPointer = _bindings.snapshotGetText(_handle, lengthPointer);
    try {
      if (textPointer == nullptr) {
        return '';
      }
      return utf8.decode(
        textPointer.asTypedList(lengthPointer.value),
        allowMalformed: true,
      );
    } finally {
      if (textPointer != nullptr) {
        _bindings.stringFree(textPointer);
      }
      _bindings.freePointer(lengthPointer);
    }
  }

  void dispose() {
    if (_disposed) {
      return;
    }
    _disposed = true;
    _bindings.snapshotDestroy(_handle);
  }

  void _ensureAlive() {
    if (_disposed) {
      throw StateError('CodeCoreSnapshot has been disposed');
    }
  }
}
