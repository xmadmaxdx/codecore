/// CodeCore — an embeddable code editor engine.
///
/// v0.3 exposes the native document model, lexical language layer, completion,
/// diagnostics, folding, search, change notifications, proposals, ghost text
/// and snapshots through Dart FFI.
library;

export 'src/codecore.dart' show CodeCore, CodeCoreDocument, CodeCoreSnapshot;
export 'src/models.dart';
export 'src/native_bindings.dart' show CodeCoreException;
