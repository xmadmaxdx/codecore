/// Public value types shared by the CodeCore Dart API.
library;

enum TokenKind {
  plain,
  comment,
  keyword,
  type,
  builtin,
  string,
  number,
  operator,
  punctuation,
  identifier,
  function,
  annotation,
  constant,
  tag,
  attribute,
  property,
  invalid;

  static TokenKind fromNative(int value) {
    if (value < 0 || value >= TokenKind.values.length) {
      return TokenKind.plain;
    }
    return TokenKind.values[value];
  }
}

enum CompletionKind {
  keyword,
  symbol,
  function,
  class_,
  type,
  builtin,
  property;

  static CompletionKind fromNative(int value) {
    if (value < 0 || value >= CompletionKind.values.length) {
      return CompletionKind.symbol;
    }
    return CompletionKind.values[value];
  }
}

enum DiagnosticSeverity {
  error(1),
  warning(2),
  info(3),
  hint(4);

  const DiagnosticSeverity(this.nativeValue);
  final int nativeValue;

  static DiagnosticSeverity fromNative(int value) {
    for (final severity in DiagnosticSeverity.values) {
      if (severity.nativeValue == value) {
        return severity;
      }
    }
    return DiagnosticSeverity.info;
  }
}

enum FoldingKind {
  brace,
  comment;

  static FoldingKind fromNative(int value) =>
      value == 0 ? FoldingKind.brace : FoldingKind.comment;
}

enum ChangeKind {
  setText,
  insert,
  erase,
  replace,
  undo,
  redo;

  static ChangeKind fromNative(int value) {
    if (value < 0 || value >= ChangeKind.values.length) {
      return ChangeKind.setText;
    }
    return ChangeKind.values[value];
  }
}

enum CursorMove {
  left,
  right,
  wordLeft,
  wordRight,
  lineStart,
  lineEnd,
  up,
  down,
  documentStart,
  documentEnd,
}

class CodeCoreLanguage {
  const CodeCoreLanguage({
    required this.id,
    required this.idString,
    required this.displayName,
    required this.extensions,
    required this.capabilities,
  });

  final int id;
  final String idString;
  final String displayName;
  final String extensions;
  final int capabilities;

  bool get supportsSyntax => (capabilities & 0x1) != 0;
  bool get supportsCompletion => (capabilities & 0x2) != 0;
  bool get supportsDiagnostics => (capabilities & 0x4) != 0;
  bool get supportsFolding => (capabilities & 0x8) != 0;

  @override
  String toString() => 'CodeCoreLanguage($idString)';
}

class TokenSpan {
  const TokenSpan({required this.start, required this.end, required this.kind});

  final int start;
  final int end;
  final TokenKind kind;

  @override
  String toString() => 'TokenSpan($start..$end, ${kind.name})';
}

class LineInfo {
  const LineInfo({
    required this.start,
    required this.contentEnd,
    required this.end,
  });

  final int start;
  final int contentEnd;
  final int end;
}

class TextPosition {
  const TextPosition({required this.line, required this.column});

  final int line;
  final int column;

  @override
  String toString() => 'TextPosition($line, $column)';
}

class Selection {
  const Selection({required this.anchor, required this.focus});

  final int anchor;
  final int focus;

  bool get isCollapsed => anchor == focus;
  int get start => anchor < focus ? anchor : focus;
  int get end => anchor < focus ? focus : anchor;
}

class CompletionItem {
  const CompletionItem({
    required this.label,
    required this.insertText,
    required this.detail,
    required this.kind,
    required this.score,
  });

  final String label;
  final String insertText;
  final String detail;
  final CompletionKind kind;
  final int score;
}

class CodeCoreDiagnostic {
  const CodeCoreDiagnostic({
    required this.start,
    required this.end,
    required this.severity,
    required this.message,
    required this.source,
  });

  final int start;
  final int end;
  final DiagnosticSeverity severity;
  final String message;
  final String source;
}

class FoldingRange {
  const FoldingRange({
    required this.startLine,
    required this.endLine,
    required this.kind,
  });

  final int startLine;
  final int endLine;
  final FoldingKind kind;
}

class SearchMatch {
  const SearchMatch({required this.start, required this.end});

  final int start;
  final int end;
}

class ChangeEvent {
  const ChangeEvent({
    required this.version,
    required this.kind,
    required this.start,
    required this.removedLength,
    required this.insertedLength,
  });

  final int version;
  final ChangeKind kind;
  final int start;
  final int removedLength;
  final int insertedLength;
}
