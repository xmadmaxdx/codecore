package dev.codecore

import dev.codecore.nativebridge.NativeCodeCore

/**
 * A native-backed document.
 *
 * Offsets are UTF-8 byte offsets, matching the C ABI and the Dart SDK. Text
 * crossing this API is converted to and from UTF-16 by the JNI bridge.
 *
 * Instances are thread-safe but should be closed with [close] when finished.
 */
class CodeCoreDocument internal constructor(initialHandle: Long) : AutoCloseable {
    private var handle: Long = initialHandle
    private val lock = Any()
    private var lastErrorMessage: String = ""

    val isClosed: Boolean
        get() = synchronized(lock) { handle == 0L }

    var text: String
        get() = synchronized(lock) {
            NativeCodeCore.nativeGetText(nativeHandle())
        }
        set(value) = synchronized(lock) {
            checkStatus(NativeCodeCore.nativeSetText(nativeHandle(), value))
        }

    val length: Long
        get() = synchronized(lock) { NativeCodeCore.nativeLength(nativeHandle()) }

    val version: Long
        get() = synchronized(lock) { NativeCodeCore.nativeDocumentVersion(nativeHandle()) }

    val lineCount: Long
        get() = synchronized(lock) { NativeCodeCore.nativeLineCount(nativeHandle()) }

    val languageId: Int
        get() = synchronized(lock) { NativeCodeCore.nativeLanguageId(nativeHandle()) }

    var readOnly: Boolean
        get() = synchronized(lock) { NativeCodeCore.nativeIsReadOnly(nativeHandle()) }
        set(value) = synchronized(lock) {
            checkStatus(NativeCodeCore.nativeSetReadOnly(nativeHandle(), value))
        }

    val canUndo: Boolean
        get() = synchronized(lock) { NativeCodeCore.nativeCanUndo(nativeHandle()) }

    val canRedo: Boolean
        get() = synchronized(lock) { NativeCodeCore.nativeCanRedo(nativeHandle()) }

    fun insert(offset: Int, value: String) {
        synchronized(lock) {
            checkStatus(NativeCodeCore.nativeInsert(nativeHandle(), offset, value))
        }
    }

    fun erase(start: Int, end: Int) {
        synchronized(lock) {
            checkStatus(NativeCodeCore.nativeErase(nativeHandle(), start, end))
        }
    }

    fun replace(start: Int, end: Int, value: String) {
        synchronized(lock) {
            checkStatus(NativeCodeCore.nativeReplace(nativeHandle(), start, end, value))
        }
    }

    /** Returns true when an edit was undone. */
    fun undo(): Boolean {
        synchronized(lock) {
            val target = nativeHandle()
            if (!NativeCodeCore.nativeCanUndo(target)) {
                return false
            }
            checkStatus(NativeCodeCore.nativeUndo(target))
            return true
        }
    }

    /** Returns true when an edit was redone. */
    fun redo(): Boolean {
        synchronized(lock) {
            val target = nativeHandle()
            if (!NativeCodeCore.nativeCanRedo(target)) {
                return false
            }
            checkStatus(NativeCodeCore.nativeRedo(target))
            return true
        }
    }

    fun clearHistory() {
        synchronized(lock) { NativeCodeCore.nativeClearHistory(nativeHandle()) }
    }

    fun tokenize(start: Int = 0, end: Int = -1): List<CodeCoreToken> {
        synchronized(lock) {
            val target = nativeHandle()
            val actualEnd = if (end < 0) lengthOf(target).toInt() else end
            val packed = NativeCodeCore.nativeTokenize(target, start, actualEnd)
            val result = ArrayList<CodeCoreToken>(packed.size / 3)
            var index = 0
            while (index + 2 < packed.size) {
                result.add(
                    CodeCoreToken(
                        start = packed[index],
                        end = packed[index + 1],
                        kind = TokenKind.fromNative(packed[index + 2]),
                    ),
                )
                index += 3
            }
            return result
        }
    }

    fun complete(offset: Int = -1): List<CompletionItem> {
        synchronized(lock) {
            val target = nativeHandle()
            val actualOffset = if (offset < 0) lengthOf(target).toInt() else offset
            return NativeCodeCore.nativeComplete(target, actualOffset).mapNotNull { line ->
                val parts = line.split(SEPARATOR, limit = 5)
                if (parts.size != 5) {
                    null
                } else {
                    CompletionItem(
                        label = parts[0],
                        insertText = parts[1],
                        detail = parts[2],
                        kind = CompletionKind.fromNative(parts[3].toIntOrNull() ?: 1),
                        score = parts[4].toIntOrNull() ?: 0,
                    )
                }
            }
        }
    }

    fun diagnose(): List<CodeCoreDiagnostic> {
        synchronized(lock) {
            return NativeCodeCore.nativeDiagnose(nativeHandle()).mapNotNull { line ->
                val parts = line.split(SEPARATOR, limit = 5)
                if (parts.size != 5) {
                    null
                } else {
                    CodeCoreDiagnostic(
                        start = parts[0].toIntOrNull() ?: 0,
                        end = parts[1].toIntOrNull() ?: 0,
                        severity = DiagnosticSeverity.fromNative(parts[2].toIntOrNull() ?: 3),
                        message = parts[3],
                        source = parts[4],
                    )
                }
            }
        }
    }

    fun find(
        query: String,
        caseSensitive: Boolean = false,
        wholeWord: Boolean = false,
    ): List<SearchRange> {
        synchronized(lock) {
            val packed = NativeCodeCore.nativeFind(nativeHandle(), query, caseSensitive, wholeWord)
            val result = ArrayList<SearchRange>(packed.size / 2)
            var index = 0
            while (index + 1 < packed.size) {
                result.add(SearchRange(packed[index], packed[index + 1]))
                index += 2
            }
            return result
        }
    }

    fun foldingRanges(): List<FoldingRange> {
        synchronized(lock) {
            val packed = NativeCodeCore.nativeFoldingRanges(nativeHandle())
            val result = ArrayList<FoldingRange>(packed.size / 3)
            var index = 0
            while (index + 2 < packed.size) {
                result.add(
                    FoldingRange(
                        startLine = packed[index],
                        endLine = packed[index + 1],
                        kind = FoldingKind.fromNative(packed[index + 2]),
                    ),
                )
                index += 3
            }
            return result
        }
    }

    fun matchBracket(offset: Int): Int? {
        synchronized(lock) {
            val match = NativeCodeCore.nativeMatchBracket(nativeHandle(), offset)
            return if (match < 0) null else match
        }
    }

    override fun close() {
        synchronized(lock) {
            if (handle == 0L) {
                return
            }
            NativeCodeCore.nativeDestroy(handle)
            handle = 0
        }
    }

    private fun nativeHandle(): Long {
        check(handle != 0L) { "CodeCoreDocument has been closed" }
        return handle
    }

    private fun lengthOf(target: Long): Long = NativeCodeCore.nativeLength(target)

    private fun checkStatus(status: Int) {
        if (status == 0) {
            return
        }
        lastErrorMessage = NativeCodeCore.nativeLastError(handle)
        throw CodeCoreException(
            status,
            if (lastErrorMessage.isBlank()) "native status $status" else lastErrorMessage,
        )
    }

    private companion object {
        const val SEPARATOR = '\u001F'
    }
}
