package dev.codecore

import dev.codecore.nativebridge.NativeCodeCore

/**
 * A structural syntax tree produced by Tree-sitter.
 *
 * The tree is immutable and snapshots one document version. It owns native
 * memory and must be closed with [close]. Repeated
 * [CodeCoreDocument.syntaxTree] calls on an unchanged version reuse the native
 * tree; edits reparse incrementally from the previous tree.
 */
class SyntaxTree internal constructor(initialHandle: Long) : AutoCloseable {
    private var handle: Long = initialHandle
    private val lock = Any()

    val isClosed: Boolean
        get() = synchronized(lock) { handle == 0L }

    /** Grammar id, for example `python`. */
    val language: String
        get() = synchronized(lock) { NativeCodeCore.nativeSyntaxLanguage(nativeHandle()) }

    /** Root node type, for example `module` for Python. */
    val rootType: String
        get() = synchronized(lock) { NativeCodeCore.nativeSyntaxRootType(nativeHandle()) }

    val nodeCount: Int
        get() = synchronized(lock) { NativeCodeCore.nativeSyntaxNodeCount(nativeHandle()) }

    val hasErrors: Boolean
        get() = synchronized(lock) { NativeCodeCore.nativeSyntaxHasErrors(nativeHandle()) }

    /**
     * Structural highlights in source order.
     *
     * [SyntaxHighlight.start] and [SyntaxHighlight.end] are UTF-8 byte offsets.
     */
    fun highlights(): List<SyntaxHighlight> = synchronized(lock) {
        NativeCodeCore.nativeSyntaxHighlights(nativeHandle()).mapNotNull { line ->
            val parts = line.split(SEPARATOR, limit = 4)
            if (parts.size != 4) {
                null
            } else {
                SyntaxHighlight(
                    start = parts[0].toIntOrNull() ?: 0,
                    end = parts[1].toIntOrNull() ?: 0,
                    kind = TokenKind.fromNative(parts[2].toIntOrNull() ?: 0),
                    capture = parts[3],
                )
            }
        }
    }

    /** `ERROR` and `MISSING` nodes in source order. */
    fun errors(): List<CodeCoreSyntaxError> = synchronized(lock) {
        NativeCodeCore.nativeSyntaxErrors(nativeHandle()).mapNotNull { line ->
            val parts = line.split(SEPARATOR, limit = 8)
            if (parts.size != 8) {
                null
            } else {
                CodeCoreSyntaxError(
                    start = parts[0].toIntOrNull() ?: 0,
                    end = parts[1].toIntOrNull() ?: 0,
                    startLine = parts[2].toIntOrNull() ?: 0,
                    startColumn = parts[3].toIntOrNull() ?: 0,
                    endLine = parts[4].toIntOrNull() ?: 0,
                    endColumn = parts[5].toIntOrNull() ?: 0,
                    missing = parts[6] == "1",
                    nodeType = parts[7],
                )
            }
        }
    }

    override fun close() {
        synchronized(lock) {
            if (handle == 0L) {
                return
            }
            NativeCodeCore.nativeSyntaxTreeDestroy(handle)
            handle = 0
        }
    }

    private fun nativeHandle(): Long {
        check(handle != 0L) { "SyntaxTree has been closed" }
        return handle
    }

    private companion object {
        const val SEPARATOR = '\u001F'
    }
}
