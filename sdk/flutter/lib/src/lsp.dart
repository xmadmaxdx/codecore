/// LSP client for CodeCore.
///
/// The client owns protocol state only. The host owns the language-server
/// process, socket or pipe: drain [CodeCoreLsp.takeOutgoing] and write the
/// frames, then pass received bytes to [CodeCoreLsp.feed].
library;

import 'dart:convert';
import 'dart:ffi';

import 'native_bindings.dart' show CodeCoreException;

/// Completed LSP response. [payload] is the raw JSON result or error object.
final class LspResult {
  const LspResult({
    required this.id,
    required this.method,
    required this.payload,
    required this.isError,
  });

  final int id;
  final String method;
  final String payload;
  final bool isError;

  @override
  String toString() => 'LspResult($id, $method, error=$isError)';
}

/// Latest diagnostics published by the server for one document.
final class LspDiagnostics {
  const LspDiagnostics({required this.uri, required this.payload});

  final String uri;
  final String payload;

  @override
  String toString() => 'LspDiagnostics($uri)';
}

typedef _LspCreateNative = Pointer<Void> Function();
typedef _LspCreateDart = Pointer<Void> Function();

typedef _LspDestroyNative = Void Function(Pointer<Void>);
typedef _LspDestroyDart = void Function(Pointer<Void>);

typedef _LspInitializeNative = Void Function(Pointer<Void>, Pointer<Uint8>);
typedef _LspInitializeDart = void Function(Pointer<Void>, Pointer<Uint8>);

typedef _LspFeedNative = Void Function(Pointer<Void>, Pointer<Uint8>, IntPtr);
typedef _LspFeedDart = void Function(Pointer<Void>, Pointer<Uint8>, int);

typedef _LspTakeOutgoingNative = Pointer<Uint8> Function(
    Pointer<Void>, Pointer<IntPtr>);
typedef _LspTakeOutgoingDart = Pointer<Uint8> Function(
    Pointer<Void>, Pointer<IntPtr>);

typedef _LspTakeResultNative = Uint8 Function(Pointer<Void>, Pointer<Int64>,
    Pointer<Pointer<Uint8>>, Pointer<Pointer<Uint8>>, Pointer<Uint8>);
typedef _LspTakeResultDart = int Function(Pointer<Void>, Pointer<Int64>,
    Pointer<Pointer<Uint8>>, Pointer<Pointer<Uint8>>, Pointer<Uint8>);

typedef _LspTakeDiagnosticsNative = Uint8 Function(
    Pointer<Void>, Pointer<Pointer<Uint8>>, Pointer<Pointer<Uint8>>);
typedef _LspTakeDiagnosticsDart = int Function(
    Pointer<Void>, Pointer<Pointer<Uint8>>, Pointer<Pointer<Uint8>>);

typedef _LspBoolNative = Uint8 Function(Pointer<Void>);
typedef _LspBoolDart = int Function(Pointer<Void>);

typedef _LspStringNative = Pointer<Uint8> Function(Pointer<Void>);
typedef _LspStringDart = Pointer<Uint8> Function(Pointer<Void>);

typedef _LspDidOpenNative = Int32 Function(Pointer<Void>, Pointer<Uint8>,
    Pointer<Uint8>, Int32, Pointer<Uint8>, IntPtr);
typedef _LspDidOpenDart = int Function(Pointer<Void>, Pointer<Uint8>,
    Pointer<Uint8>, int, Pointer<Uint8>, int);

typedef _LspDidChangeNative = Int32 Function(
    Pointer<Void>, Pointer<Uint8>, Int32, Pointer<Uint8>, IntPtr);
typedef _LspDidChangeDart = int Function(
    Pointer<Void>, Pointer<Uint8>, int, Pointer<Uint8>, int);

typedef _LspDidSaveNative = Int32 Function(Pointer<Void>, Pointer<Uint8>,
    Pointer<Uint8>, IntPtr, Uint8);
typedef _LspDidSaveDart = int Function(
    Pointer<Void>, Pointer<Uint8>, Pointer<Uint8>, int, int);

typedef _LspUriNative = Int32 Function(Pointer<Void>, Pointer<Uint8>);
typedef _LspUriDart = int Function(Pointer<Void>, Pointer<Uint8>);

typedef _LspRequestNative = Int64 Function(Pointer<Void>, Pointer<Uint8>,
    Pointer<Uint8>, IntPtr);
typedef _LspRequestDart = int Function(
    Pointer<Void>, Pointer<Uint8>, Pointer<Uint8>, int);

typedef _LspNotifyNative = Int32 Function(Pointer<Void>, Pointer<Uint8>,
    Pointer<Uint8>, IntPtr);
typedef _LspNotifyDart = int Function(
    Pointer<Void>, Pointer<Uint8>, Pointer<Uint8>, int);

typedef _LspCancelNative = Void Function(Pointer<Void>, Int64);
typedef _LspCancelDart = void Function(Pointer<Void>, int);

typedef _LspVoidHandleNative = Void Function(Pointer<Void>);
typedef _LspVoidHandleDart = void Function(Pointer<Void>);

final class _LspBindings {
  _LspBindings(this.library) {
    create = library.lookupFunction<_LspCreateNative, _LspCreateDart>(
        'cc_lsp_create');
    destroy = library.lookupFunction<_LspDestroyNative, _LspDestroyDart>(
        'cc_lsp_destroy');
    initialize = library.lookupFunction<_LspInitializeNative,
        _LspInitializeDart>('cc_lsp_initialize');
    feed = library.lookupFunction<_LspFeedNative, _LspFeedDart>('cc_lsp_feed');
    takeOutgoing = library.lookupFunction<_LspTakeOutgoingNative,
        _LspTakeOutgoingDart>('cc_lsp_take_outgoing');
    takeResult = library.lookupFunction<_LspTakeResultNative,
        _LspTakeResultDart>('cc_lsp_take_result');
    takeDiagnostics = library.lookupFunction<_LspTakeDiagnosticsNative,
        _LspTakeDiagnosticsDart>('cc_lsp_take_diagnostics');
    isInitialized = library.lookupFunction<_LspBoolNative, _LspBoolDart>(
        'cc_lsp_is_initialized');
    serverCapabilities = library.lookupFunction<_LspStringNative,
        _LspStringDart>('cc_lsp_server_capabilities');
    lastError = library.lookupFunction<_LspStringNative, _LspStringDart>(
        'cc_lsp_last_error');
    didOpen = library.lookupFunction<_LspDidOpenNative, _LspDidOpenDart>(
        'cc_lsp_did_open');
    didChange = library.lookupFunction<_LspDidChangeNative, _LspDidChangeDart>(
        'cc_lsp_did_change');
    didClose = library.lookupFunction<_LspUriNative, _LspUriDart>(
        'cc_lsp_did_close');
    didSave = library.lookupFunction<_LspDidSaveNative, _LspDidSaveDart>(
        'cc_lsp_did_save');
    request = library.lookupFunction<_LspRequestNative, _LspRequestDart>(
        'cc_lsp_request');
    notify = library.lookupFunction<_LspNotifyNative, _LspNotifyDart>(
        'cc_lsp_notify');
    cancel = library.lookupFunction<_LspCancelNative, _LspCancelDart>(
        'cc_lsp_cancel');
    shutdown = library.lookupFunction<_LspVoidHandleNative,
        _LspVoidHandleDart>('cc_lsp_shutdown');
    exit = library.lookupFunction<_LspVoidHandleNative, _LspVoidHandleDart>(
        'cc_lsp_exit');
    alloc = library.lookupFunction<Pointer<Uint8> Function(IntPtr),
        Pointer<Uint8> Function(int)>('cc_alloc');
    free = library.lookupFunction<Void Function(Pointer<Uint8>),
        void Function(Pointer<Uint8>)>('cc_free');
    stringFree = library.lookupFunction<Void Function(Pointer<Uint8>),
        void Function(Pointer<Uint8>)>('cc_string_free');
  }

  final DynamicLibrary library;
  late final _LspCreateDart create;
  late final _LspDestroyDart destroy;
  late final _LspInitializeDart initialize;
  late final _LspFeedDart feed;
  late final _LspTakeOutgoingDart takeOutgoing;
  late final _LspTakeResultDart takeResult;
  late final _LspTakeDiagnosticsDart takeDiagnostics;
  late final _LspBoolDart isInitialized;
  late final _LspStringDart serverCapabilities;
  late final _LspStringDart lastError;
  late final _LspDidOpenDart didOpen;
  late final _LspDidChangeDart didChange;
  late final _LspUriDart didClose;
  late final _LspDidSaveDart didSave;
  late final _LspRequestDart request;
  late final _LspNotifyDart notify;
  late final _LspCancelDart cancel;
  late final _LspVoidHandleDart shutdown;
  late final _LspVoidHandleDart exit;
  late final Pointer<Uint8> Function(int) alloc;
  late final void Function(Pointer<Uint8>) free;
  late final void Function(Pointer<Uint8>) stringFree;
}

/// Transport-agnostic LSP client.
///
/// Byte strings passed to [feed] must be the raw LSP frames from the server.
/// [takeOutgoing] returns complete frames, including `Content-Length` headers.
final class CodeCoreLsp {
  CodeCoreLsp._(this._bindings, this._handle);

  /// Opens a client on an already-loaded native library.
  factory CodeCoreLsp.open(DynamicLibrary library) {
    final bindings = _LspBindings(library);
    final handle = bindings.create();
    if (handle == nullptr) {
      throw const CodeCoreException(5, 'LSP client allocation failed');
    }
    return CodeCoreLsp._(bindings, handle);
  }

  final _LspBindings _bindings;
  final Pointer<Void> _handle;
  bool _disposed = false;

  bool get isDisposed => _disposed;

  void initialize(String rootUri) {
    _ensureAlive();
    final pointer = _utf8(rootUri);
    try {
      _bindings.initialize(_handle, pointer);
    } finally {
      _free(pointer);
    }
  }

  bool get isInitialized {
    _ensureAlive();
    return _bindings.isInitialized(_handle) != 0;
  }

  String get serverCapabilities {
    _ensureAlive();
    return _read(_bindings.serverCapabilities(_handle));
  }

  String get lastError {
    _ensureAlive();
    return _read(_bindings.lastError(_handle));
  }

  /// Feeds raw bytes received from the language server.
  void feed(String chunk) {
    _ensureAlive();
    final pointer = _utf8(chunk);
    try {
      _bindings.feed(_handle, pointer, utf8.encode(chunk).length);
    } finally {
      _free(pointer);
    }
  }

  /// Returns the next complete outgoing frame, or null when the queue is empty.
  String? takeOutgoing() {
    _ensureAlive();
    final lengthPointer = _bindings.alloc(sizeOf<IntPtr>()).cast<IntPtr>();
    try {
      final pointer = _bindings.takeOutgoing(_handle, lengthPointer);
      if (pointer == nullptr) {
        return null;
      }
      try {
        return utf8.decode(
          pointer.asTypedList(lengthPointer.value),
          allowMalformed: true,
        );
      } finally {
        _bindings.stringFree(pointer);
      }
    } finally {
      _bindings.free(lengthPointer.cast<Uint8>());
    }
  }

  LspResult? takeResult() {
    _ensureAlive();
    final idPointer = _bindings.alloc(sizeOf<Int64>()).cast<Int64>();
    final methodSlot =
        _bindings.alloc(sizeOf<Pointer<Uint8>>()).cast<Pointer<Uint8>>();
    final payloadSlot =
        _bindings.alloc(sizeOf<Pointer<Uint8>>()).cast<Pointer<Uint8>>();
    final errorPointer = _bindings.alloc(sizeOf<Uint8>()).cast<Uint8>();
    try {
      final available = _bindings.takeResult(
        _handle,
        idPointer,
        methodSlot,
        payloadSlot,
        errorPointer,
      );
      if (available == 0) {
        return null;
      }
      final method = methodSlot.value;
      final payload = payloadSlot.value;
      try {
        return LspResult(
          id: idPointer.value,
          method: _read(method),
          payload: _read(payload),
          isError: errorPointer.value != 0,
        );
      } finally {
        _bindings.stringFree(method);
        _bindings.stringFree(payload);
      }
    } finally {
      _bindings.free(errorPointer.cast<Uint8>());
      _bindings.free(payloadSlot.cast<Uint8>());
      _bindings.free(methodSlot.cast<Uint8>());
      _bindings.free(idPointer.cast<Uint8>());
    }
  }

  LspDiagnostics? takeDiagnostics() {
    _ensureAlive();
    final uriSlot =
        _bindings.alloc(sizeOf<Pointer<Uint8>>()).cast<Pointer<Uint8>>();
    final payloadSlot =
        _bindings.alloc(sizeOf<Pointer<Uint8>>()).cast<Pointer<Uint8>>();
    try {
      if (_bindings.takeDiagnostics(_handle, uriSlot, payloadSlot) == 0) {
        return null;
      }
      final uri = uriSlot.value;
      final payload = payloadSlot.value;
      try {
        return LspDiagnostics(uri: _read(uri), payload: _read(payload));
      } finally {
        _bindings.stringFree(uri);
        _bindings.stringFree(payload);
      }
    } finally {
      _bindings.free(payloadSlot.cast<Uint8>());
      _bindings.free(uriSlot.cast<Uint8>());
    }
  }

  void didOpen(String uri, String languageId, int version, String text) {
    _ensureAlive();
    final uriPointer = _utf8(uri);
    final languagePointer = _utf8(languageId);
    final textPointer = _utf8(text);
    try {
      _check(_bindings.didOpen(_handle, uriPointer, languagePointer, version,
          textPointer, utf8.encode(text).length));
    } finally {
      _free(textPointer);
      _free(languagePointer);
      _free(uriPointer);
    }
  }

  void didChange(String uri, int version, String text) {
    _ensureAlive();
    final uriPointer = _utf8(uri);
    final textPointer = _utf8(text);
    try {
      _check(_bindings.didChange(
          _handle, uriPointer, version, textPointer, utf8.encode(text).length));
    } finally {
      _free(textPointer);
      _free(uriPointer);
    }
  }

  void didClose(String uri) {
    _ensureAlive();
    final pointer = _utf8(uri);
    try {
      _check(_bindings.didClose(_handle, pointer));
    } finally {
      _free(pointer);
    }
  }

  void didSave(String uri, String text, {bool includeText = true}) {
    _ensureAlive();
    final uriPointer = _utf8(uri);
    final textPointer = _utf8(text);
    try {
      _check(_bindings.didSave(_handle, uriPointer, textPointer,
          utf8.encode(text).length, includeText ? 1 : 0));
    } finally {
      _free(textPointer);
      _free(uriPointer);
    }
  }

  int completion(String uri, int line, int character) =>
      _positionRequest('textDocument/completion', uri, line, character);

  int hover(String uri, int line, int character) =>
      _positionRequest('textDocument/hover', uri, line, character);

  int definition(String uri, int line, int character) =>
      _positionRequest('textDocument/definition', uri, line, character);

  int signatureHelp(String uri, int line, int character) =>
      _positionRequest('textDocument/signatureHelp', uri, line, character);

  int references(String uri, int line, int character,
          {bool includeDeclaration = true}) =>
      request('textDocument/references', {
        'textDocument': {'uri': uri},
        'position': {'line': line, 'character': character},
        'context': {'includeDeclaration': includeDeclaration},
      });

  int documentSymbols(String uri) =>
      request('textDocument/documentSymbol', {
        'textDocument': {'uri': uri},
      });

  int workspaceSymbols(String query) =>
      request('workspace/symbol', {'query': query});

  int rename(String uri, int line, int character, String newName) =>
      request('textDocument/rename', {
        'textDocument': {'uri': uri},
        'position': {'line': line, 'character': character},
        'newName': newName,
      });

  int formatting(String uri, {int tabSize = 4, bool insertSpaces = true}) =>
      request('textDocument/formatting', {
        'textDocument': {'uri': uri},
        'options': {'tabSize': tabSize, 'insertSpaces': insertSpaces},
      });

  int codeAction(
    String uri,
    int startLine,
    int startCharacter,
    int endLine,
    int endCharacter,
  ) =>
      request('textDocument/codeAction', {
        'textDocument': {'uri': uri},
        'range': {
          'start': {'line': startLine, 'character': startCharacter},
          'end': {'line': endLine, 'character': endCharacter},
        },
        'context': {'diagnostics': <Object>[]},
      });

  int semanticTokens(String uri) =>
      request('textDocument/semanticTokens/full', {
        'textDocument': {'uri': uri},
      });

  /// Sends an arbitrary LSP request. Returns the request id.
  int request(String method, Map<String, Object?> params) =>
      requestRaw(method, jsonEncode(params));

  int requestRaw(String method, String paramsJson) {
    _ensureAlive();
    final methodPointer = _utf8(method);
    final paramsPointer = _utf8(paramsJson);
    try {
      final id = _bindings.request(_handle, methodPointer, paramsPointer,
          utf8.encode(paramsJson).length);
      if (id < 0) {
        throw CodeCoreException(1, lastError);
      }
      return id;
    } finally {
      _free(paramsPointer);
      _free(methodPointer);
    }
  }

  /// Sends an arbitrary LSP notification.
  void notify(String method, Map<String, Object?> params) =>
      notifyRaw(method, jsonEncode(params));

  void notifyRaw(String method, String paramsJson) {
    _ensureAlive();
    final methodPointer = _utf8(method);
    final paramsPointer = _utf8(paramsJson);
    try {
      _check(_bindings.notify(_handle, methodPointer, paramsPointer,
          utf8.encode(paramsJson).length));
    } finally {
      _free(paramsPointer);
      _free(methodPointer);
    }
  }

  void cancel(int id) {
    _ensureAlive();
    _bindings.cancel(_handle, id);
  }

  void shutdown() {
    _ensureAlive();
    _bindings.shutdown(_handle);
  }

  void exit() {
    _ensureAlive();
    _bindings.exit(_handle);
  }

  int _positionRequest(
      String method, String uri, int line, int character) {
    return request(method, {
      'textDocument': {'uri': uri},
      'position': {'line': line, 'character': character},
    });
  }

  Pointer<Uint8> _utf8(String value) {
    final bytes = utf8.encode(value);
    final pointer = _bindings.alloc(bytes.length + 1);
    if (pointer == nullptr) {
      throw const CodeCoreException(5, 'LSP string allocation failed');
    }
    pointer.asTypedList(bytes.length).setAll(0, bytes);
    pointer[bytes.length] = 0;
    return pointer;
  }

  void _free(Pointer<Uint8> pointer) {
    _bindings.free(pointer);
  }

  String _read(Pointer<Uint8> pointer) {
    if (pointer == nullptr) {
      return '';
    }
    final bytes = <int>[];
    var index = 0;
    while (pointer[index] != 0) {
      bytes.add(pointer[index]);
      index += 1;
    }
    return utf8.decode(bytes, allowMalformed: true);
  }

  void _check(int status) {
    if (status != 0) {
      throw CodeCoreException(status, lastError);
    }
  }

  void _ensureAlive() {
    if (_disposed) {
      throw StateError('CodeCoreLsp has been disposed');
    }
  }

  void dispose() {
    if (_disposed) {
      return;
    }
    _disposed = true;
    _bindings.destroy(_handle);
  }

  /// Alias for [dispose].
  void close() => dispose();
}
