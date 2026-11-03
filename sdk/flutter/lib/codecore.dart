/// CodeCore — an embeddable code editor engine.
///
/// v0.4 exposes the native document model, lexical language layer, structural
/// Tree-sitter syntax for supported languages, completion, diagnostics, folding,
/// search, change notifications, proposals, ghost text, snapshots and an LSP
/// client through Dart FFI.
library;

export 'src/codecore.dart'
    show CodeCore, CodeCoreDocument, CodeCoreSnapshot, SyntaxTree;
export 'src/lsp.dart' show CodeCoreLsp, LspResult, LspDiagnostics;
export 'src/models.dart';
export 'src/native_bindings.dart' show CodeCoreException;
