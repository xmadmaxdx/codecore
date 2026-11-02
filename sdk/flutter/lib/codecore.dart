/// CodeCore — an embeddable code editor engine.
///
/// v0.1 exposes the native document model, lexical language layer, completion,
/// diagnostics, folding, search and change notifications through Dart FFI.
library;

export 'src/codecore.dart' show CodeCore, CodeCoreDocument;
export 'src/models.dart';
export 'src/native_bindings.dart' show CodeCoreException;
