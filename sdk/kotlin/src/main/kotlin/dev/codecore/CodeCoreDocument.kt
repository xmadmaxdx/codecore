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

    var languageId: Int
        get() = synchronized(lock) { NativeCodeCore.nativeLanguageId(nativeHandle()) }
        set(value) = synchronized(lock) {
            checkStatus(NativeCodeCore.nativeSetLanguage(nativeHandle(), value))
        }

    var readOnly: Boolean
        get() = synchronized(lock) { NativeCodeCore.nativeIsReadOnly(nativeHandle()) }
        set(value) = synchronized(lock) {
            checkStatus(NativeCodeCore.nativeSetReadOnly(nativeHandle(), value))
        }

    var editable: Boolean
        get() = !readOnly
        set(value) = synchronized(lock) {
            checkStatus(NativeCodeCore.nativeSetEditable(nativeHandle(), value))
        }

    var editOrigin: EditOrigin
        get() = synchronized(lock) {
            EditOrigin.fromNative(NativeCodeCore.nativeEditOrigin(nativeHandle()))
        }
        set(value) = synchronized(lock) {
            checkStatus(NativeCodeCore.nativeSetEditOrigin(nativeHandle(), value.nativeValue))
        }

    val hasProposal: Boolean
        get() = synchronized(lock) { NativeCodeCore.nativeHasProposal(nativeHandle()) }

    /** Preview content; it does not change [text] until [acceptProposal]. */
    fun setProposal(value: String, origin: EditOrigin = EditOrigin.AI) {
        synchronized(lock) {
            checkStatus(NativeCodeCore.nativeSetProposal(nativeHandle(), value, origin.nativeValue))
        }
    }

    fun proposal(): String? = synchronized(lock) {
        NativeCodeCore.nativeProposal(nativeHandle())
    }

    fun proposalDiff(): List<DiffLine> = synchronized(lock) {
        NativeCodeCore.nativeProposalDiff(nativeHandle()).mapNotNull { line ->
            val parts = line.split(SEPARATOR, limit = 4)
            if (parts.size != 4) {
                null
            } else {
                DiffLine(
                    kind = DiffKind.fromNative(parts[0].toIntOrNull() ?: 0),
                    oldLine = parts[1].toIntOrNull() ?: -1,
                    newLine = parts[2].toIntOrNull() ?: -1,
                    text = parts[3],
                )
            }
        }
    }

    fun acceptProposal() {
        synchronized(lock) { checkStatus(NativeCodeCore.nativeAcceptProposal(nativeHandle())) }
    }

    fun rejectProposal() {
        synchronized(lock) { checkStatus(NativeCodeCore.nativeRejectProposal(nativeHandle())) }
    }

    fun ghostText(): GhostText? = synchronized(lock) {
        val line = NativeCodeCore.nativeGhostText(nativeHandle())
        if (line == null) {
            null
        } else {
            val parts = line.split(SEPARATOR, limit = 3)
            if (parts.size != 3) {
                null
            } else {
                GhostText(
                    offset = parts[0].toIntOrNull() ?: 0,
                    origin = EditOrigin.fromNative(parts[1].toIntOrNull() ?: 1),
                    text = parts[2],
                )
            }
        }
    }

    fun showGhostText(offset: Int, value: String, origin: EditOrigin = EditOrigin.AI) {
        synchronized(lock) {
            checkStatus(
                NativeCodeCore.nativeSetGhostText(
                    nativeHandle(),
                    offset,
                    value,
                    origin.nativeValue,
                ),
            )
        }
    }

    /** Accepts all ghost text, or only [maxCodePoints] leading code points. */
    fun acceptGhostText(maxCodePoints: Int = 0) {
        synchronized(lock) {
            checkStatus(NativeCodeCore.nativeAcceptGhostText(nativeHandle(), maxCodePoints))
        }
    }

    fun dismissGhostText() {
        synchronized(lock) {
            checkStatus(NativeCodeCore.nativeDismissGhostText(nativeHandle()))
        }
    }

    fun createSnapshot(): CodeCoreSnapshot {
        synchronized(lock) {
            val snapshot = NativeCodeCore.nativeCreateSnapshot(nativeHandle())
            check(snapshot != 0L) { "snapshot creation failed" }
            return CodeCoreSnapshot(snapshot)
        }
    }

    fun restoreSnapshot(snapshot: CodeCoreSnapshot) {
        synchronized(lock) {
            checkStatus(NativeCodeCore.nativeRestoreSnapshot(nativeHandle(), snapshot.nativeHandle()))
        }
    }

    val selection: Selection
        get() = synchronized(lock) {
            val packed = NativeCodeCore.nativeGetSelection(nativeHandle())
            Selection(
                anchor = (packed ushr 32).toInt(),
                focus = packed.toInt(),
            )
        }

    fun setSelection(anchor: Int, focus: Int) {
        synchronized(lock) {
            checkStatus(NativeCodeCore.nativeSetSelection(nativeHandle(), anchor, focus))
        }
    }

    fun moveCursor(move: CursorMove, extendSelection: Boolean = false) {
        synchronized(lock) {
            checkStatus(NativeCodeCore.nativeMoveCursor(nativeHandle(), move.nativeValue, extendSelection))
        }
    }

    /** Number of selections; index 0 is the primary selection. */
    val cursorCount: Int
        get() = synchronized(lock) { NativeCodeCore.nativeCursorCount(nativeHandle()) }

    fun cursorAt(index: Int): Selection {
        synchronized(lock) {
            val packed = NativeCodeCore.nativeCursorAt(nativeHandle(), index)
            require(packed >= 0) { "cursor index $index is out of range" }
            return Selection(
                anchor = (packed ushr 32).toInt(),
                focus = packed.toInt(),
            )
        }
    }

    fun addCursor(anchor: Int, focus: Int) {
        synchronized(lock) {
            checkStatus(NativeCodeCore.nativeAddCursor(nativeHandle(), anchor, focus))
        }
    }

    fun clearCursors() {
        synchronized(lock) { NativeCodeCore.nativeClearCursors(nativeHandle()) }
    }

    fun moveCursors(move: CursorMove, extendSelection: Boolean = false) {
        synchronized(lock) {
            checkStatus(
                NativeCodeCore.nativeMoveCursors(nativeHandle(), move.nativeValue, extendSelection),
            )
        }
    }

    /** Replaces every selection with [value] as one undo step. */
    fun insertAtCursors(value: String) {
        synchronized(lock) {
            checkStatus(NativeCodeCore.nativeMultiInsert(nativeHandle(), value))
        }
    }

    /** Erases every selection, or one code point per collapsed caret. */
    fun eraseAtCursors(backward: Boolean = true) {
        synchronized(lock) {
            checkStatus(NativeCodeCore.nativeMultiErase(nativeHandle(), backward))
        }
    }

    /** Installs a synchronous change listener. Pass null to detach. */
    fun setChangeListener(listener: CodeCoreChangeListener?) {
        synchronized(lock) {
            if (handle == 0L) {
                return
            }
            NativeCodeCore.nativeSetChangeListener(handle, null)
            changeListenerAdapter = null
            if (listener == null) {
                return
            }
            val adapter = ListenerAdapter(listener)
            changeListenerAdapter = adapter
            NativeCodeCore.nativeSetChangeListener(handle, adapter)
        }
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

    /**
     * Parses the current version with Tree-sitter and returns an owned tree.
     *
     * Returns null when the document's language has no bundled grammar. The
     * tree is an immutable snapshot of this version; close it when done.
     */
    fun syntaxTree(): SyntaxTree? = synchronized(lock) {
        val tree = NativeCodeCore.nativeSyntaxTree(nativeHandle())
        if (tree == 0L) null else SyntaxTree(tree)
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

    /**
     * Finds literal text, or ECMAScript regular expressions when [regex] is true.
     *
     * The first element of the native result is the status; it is checked here so
     * an invalid pattern throws [CodeCoreException] instead of returning nothing.
     */
    fun find(
        query: String,
        caseSensitive: Boolean = false,
        wholeWord: Boolean = false,
        regex: Boolean = false,
    ): List<SearchRange> {
        synchronized(lock) {
            val target = nativeHandle()
            val packed = NativeCodeCore.nativeFind(
                target,
                query,
                caseSensitive,
                wholeWord,
                regex,
            )
            if (packed.isEmpty()) {
                return emptyList()
            }
            val status = packed[0].toInt()
            if (status != 0) {
                lastErrorMessage = NativeCodeCore.nativeLastError(target)
                throw CodeCoreException(
                    status,
                    if (lastErrorMessage.isBlank()) "native status $status" else lastErrorMessage,
                )
            }
            val result = ArrayList<SearchRange>((packed.size - 1) / 2)
            var index = 1
            while (index + 1 < packed.size) {
                result.add(SearchRange(packed[index].toInt(), packed[index + 1].toInt()))
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
            NativeCodeCore.nativeSetChangeListener(handle, null)
            changeListenerAdapter = null
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

    private var changeListenerAdapter: NativeChangeListener? = null

    private class ListenerAdapter(
        private val callback: CodeCoreChangeListener,
    ) : NativeChangeListener {
        override fun onChange(
            version: Long,
            kind: Int,
            start: Int,
            removedLength: Int,
            insertedLength: Int,
            origin: Int,
        ) {
            callback.onChange(
                ChangeEvent(
                    version = version,
                    kind = ChangeKind.fromNative(kind),
                    start = start,
                    removedLength = removedLength,
                    insertedLength = insertedLength,
                    origin = EditOrigin.fromNative(origin),
                ),
            )
        }
    }

    private companion object {
        const val SEPARATOR = '\u001F'
    }
}

internal interface NativeChangeListener {
    fun onChange(
        version: Long,
        kind: Int,
        start: Int,
        removedLength: Int,
        insertedLength: Int,
        origin: Int,
    )
}
