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

enum class CursorMove(val nativeValue: Int) {
    LEFT(0),
    RIGHT(1),
    WORD_LEFT(2),
    WORD_RIGHT(3),
    LINE_START(4),
    LINE_END(5),
    UP(6),
    DOWN(7),
    DOCUMENT_START(8),
    DOCUMENT_END(9),
}

enum class ChangeKind(val nativeValue: Int) {
    SET_TEXT(0),
    INSERT(1),
    ERASE(2),
    REPLACE(3),
    UNDO(4),
    REDO(5),
    PROPOSAL_ACCEPTED(6),
    PROPOSAL_REJECTED(7),
    SNAPSHOT_RESTORED(8),
    GHOST_TEXT_ACCEPTED(9),
    GHOST_TEXT_DISMISSED(10);

    companion object {
        fun fromNative(value: Int): ChangeKind =
            values().firstOrNull { it.nativeValue == value } ?: SET_TEXT
    }
}

data class Selection(val anchor: Int, val focus: Int) {
    val isCollapsed: Boolean get() = anchor == focus
    val start: Int get() = if (anchor < focus) anchor else focus
    val end: Int get() = if (anchor < focus) focus else anchor
}

data class ChangeEvent(
    val version: Long,
    val kind: ChangeKind,
    val start: Int,
    val removedLength: Int,
    val insertedLength: Int,
    val origin: EditOrigin,
)

fun interface CodeCoreChangeListener {
    fun onChange(event: ChangeEvent)
}

enum class EditOrigin(val nativeValue: Int) {
    USER(0),
    AI(1),
    HOST(2),
    FORMATTER(3),
    EXTERNAL_TOOL(4);

    companion object {
        fun fromNative(value: Int): EditOrigin =
            values().firstOrNull { it.nativeValue == value } ?: HOST
    }
}

enum class DiffKind(val nativeValue: Int) {
    EQUAL(0),
    INSERT(1),
    DELETE(2);

    companion object {
        fun fromNative(value: Int): DiffKind =
            values().firstOrNull { it.nativeValue == value } ?: EQUAL
    }
}

data class GhostText(
    val offset: Int,
    val text: String,
    val origin: EditOrigin,
)

data class DiffLine(
    val kind: DiffKind,
    val oldLine: Int,
    val newLine: Int,
    val text: String,
)

data class SyntaxHighlight(
    val start: Int,
    val end: Int,
    val kind: TokenKind,
    val capture: String,
)

/**
 * An `ERROR` or `MISSING` node from Tree-sitter.
 *
 * Rows are zero-based. Columns are bytes within the row, matching Tree-sitter,
 * not Unicode code points.
 */
data class CodeCoreSyntaxError(
    val start: Int,
    val end: Int,
    val startLine: Int,
    val startColumn: Int,
    val endLine: Int,
    val endColumn: Int,
    val missing: Boolean,
    val nodeType: String,
)

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

enum class ExecutionEventKind {
    OUTPUT,
    EXIT,
}

enum class ExecutionStream {
    STDOUT,
    STDERR,
}

/** One output chunk or the final exit event from a CodeCore execution session. */
data class ExecutionEvent(
    val kind: ExecutionEventKind,
    val stream: ExecutionStream,
    val sequence: Long,
    val data: String,
    val exitCode: Int,
    val cancelled: Boolean,
    val timedOut: Boolean,
)

class CodeCoreException(val code: Int, message: String) : RuntimeException(message)
