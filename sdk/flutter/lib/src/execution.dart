import 'dart:convert';
import 'dart:ffi';

import 'models.dart';
import 'native_bindings.dart';

/// One child process started by [CodeCore.startExecution].
///
/// The session owns native pipes and worker threads. Poll [poll] to drain
/// output; the exit event is delivered after all queued output. Call [dispose]
/// when finished, which cancels a running child.
class CodeCoreExecution {
  CodeCoreExecution.internal(this._bindings, this._handle);

  final CodeCoreBindings _bindings;
  final Pointer<Void> _handle;
  bool _disposed = false;

  /// Non-blocking. Returns null when no output or exit event is ready.
  ExecutionEvent? poll() {
    _ensureAlive();
    final eventPointer =
        _bindings.allocate(sizeOf<CcExecutionEvent>()).cast<CcExecutionEvent>();
    try {
      if (_bindings.executionPoll(_handle, eventPointer) == 0) {
        return null;
      }
      final value = eventPointer.ref;
      var data = '';
      if (value.data != nullptr) {
        data = _bindings.readUtf8(value.data, value.length);
        _bindings.stringFree(value.data);
      }
      return ExecutionEvent(
        kind: value.kind == 0
            ? ExecutionEventKind.output
            : ExecutionEventKind.exit,
        stream:
            value.stream == 0 ? ExecutionStream.stdout : ExecutionStream.stderr,
        sequence: value.sequence,
        data: data,
        exitCode: value.exitCode,
        cancelled: value.cancelled != 0,
        timedOut: value.timedOut != 0,
      );
    } finally {
      _bindings.freePointer(eventPointer);
    }
  }

  /// Writes UTF-8 bytes to the child's stdin.
  ///
  /// Throws [CodeCoreException] when the process has exited or the pipe failed.
  void write(String data) {
    _ensureAlive();
    final bytes = utf8.encode(data);
    final pointer = _bindings.allocate(bytes.length + 1);
    try {
      pointer.asTypedList(bytes.length).setAll(0, bytes);
      pointer[bytes.length] = 0;
      final status = _bindings.executionWrite(_handle, pointer, bytes.length);
      if (status != 0) {
        throw CodeCoreException(status, 'execution write failed');
      }
    } finally {
      _bindings.freePointer(pointer);
    }
  }

  void closeStdin() {
    _ensureAlive();
    _bindings.executionCloseStdin(_handle);
  }

  void cancel() {
    _ensureAlive();
    _bindings.executionCancel(_handle);
  }

  /// Waits for the process to exit. Zero timeout checks without blocking.
  bool wait([Duration timeout = Duration.zero]) {
    _ensureAlive();
    final milliseconds = timeout.inMilliseconds;
    final clamped = milliseconds < 0
        ? 0
        : (milliseconds > 0xFFFFFFFF ? 0xFFFFFFFF : milliseconds);
    return _bindings.executionWait(_handle, clamped) != 0;
  }

  bool get isRunning {
    _ensureAlive();
    return _bindings.executionIsRunning(_handle) != 0;
  }

  /// -1 until the process has exited. Signal deaths report 128 + signal.
  int get exitCode {
    _ensureAlive();
    return _bindings.executionExitCode(_handle);
  }

  void dispose() {
    if (_disposed) {
      return;
    }
    _disposed = true;
    _bindings.executionDestroy(_handle);
  }

  void _ensureAlive() {
    if (_disposed) {
      throw StateError('CodeCoreExecution has been disposed');
    }
  }
}
