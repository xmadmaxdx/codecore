package dev.codecore

enum class TokenKind(val nativeValue: Int) {
    PLAIN(0),
    COMMENT(1),
    KEYWORD(2),
    TYPE(3),
    BUILTIN(4),
    STRING(5),
    NUMBER(6),
    OPERATOR(7),
    PUNCTUATION(8),
    IDENTIFIER(9),
    FUNCTION(10),
    ANNOTATION(11),
    CONSTANT(12),
    TAG(13),
    ATTRIBUTE(14),
    PROPERTY(15),
    INVALID(16);

    companion object {
        fun fromNative(value: Int): TokenKind =
            values().firstOrNull { it.nativeValue == value } ?: PLAIN
    }
}

enum class CompletionKind(val nativeValue: Int) {
    KEYWORD(0),
    SYMBOL(1),
    FUNCTION(2),
    CLASS(3),
    TYPE(4),
    BUILTIN(5),
    PROPERTY(6);

    companion object {
        fun fromNative(value: Int): CompletionKind =
            values().firstOrNull { it.nativeValue == value } ?: SYMBOL
    }
}

enum class DiagnosticSeverity(val nativeValue: Int) {
    ERROR(1),
    WARNING(2),
    INFO(3),
    HINT(4);

    companion object {
        fun fromNative(value: Int): DiagnosticSeverity =
            values().firstOrNull { it.nativeValue == value } ?: INFO
    }
}

enum class FoldingKind(val nativeValue: Int) {
    BRACE(0),
    COMMENT(1);

    companion object {
        fun fromNative(value: Int): FoldingKind =
            values().firstOrNull { it.nativeValue == value } ?: BRACE
    }
}

data class CodeCoreToken(
    val start: Int,
    val end: Int,
    val kind: TokenKind,
)

data class CompletionItem(
    val label: String,
    val insertText: String,
    val detail: String,
    val kind: CompletionKind,
    val score: Int,
)

data class CodeCoreDiagnostic(
    val start: Int,
    val end: Int,
    val severity: DiagnosticSeverity,
    val message: String,
    val source: String,
)

data class SearchRange(val start: Int, val end: Int)

data class FoldingRange(
    val startLine: Int,
    val endLine: Int,
    val kind: FoldingKind,
)

class CodeCoreException(val code: Int, message: String) : RuntimeException(message)
