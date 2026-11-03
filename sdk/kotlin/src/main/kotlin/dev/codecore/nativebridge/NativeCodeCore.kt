package dev.codecore.nativebridge

/**
 * Raw JNI entry points. Applications should use `dev.codecore.CodeCore` and
 * `dev.codecore.CodeCoreDocument` instead.
 */
object NativeCodeCore {
    init {
        System.loadLibrary("codecore_jni")
    }

    @JvmStatic
    external fun nativeVersion(): String

    @JvmStatic
    external fun nativeCreate(language: String, tabSize: Int, insertSpaces: Boolean): Long

    @JvmStatic
    external fun nativeDestroy(handle: Long)

    @JvmStatic
    external fun nativeSetText(handle: Long, text: String): Int

    @JvmStatic
    external fun nativeGetText(handle: Long): String

    @JvmStatic
    external fun nativeLength(handle: Long): Long

    @JvmStatic
    external fun nativeDocumentVersion(handle: Long): Long

    @JvmStatic
    external fun nativeLineCount(handle: Long): Long

    @JvmStatic
    external fun nativeLanguageId(handle: Long): Int

    @JvmStatic
    external fun nativeLastError(handle: Long): String

    @JvmStatic
    external fun nativeInsert(handle: Long, offset: Int, text: String): Int

    @JvmStatic
    external fun nativeErase(handle: Long, start: Int, end: Int): Int

    @JvmStatic
    external fun nativeReplace(handle: Long, start: Int, end: Int, text: String): Int

    @JvmStatic
    external fun nativeUndo(handle: Long): Int

    @JvmStatic
    external fun nativeRedo(handle: Long): Int

    @JvmStatic
    external fun nativeCanUndo(handle: Long): Boolean

    @JvmStatic
    external fun nativeCanRedo(handle: Long): Boolean

    @JvmStatic
    external fun nativeClearHistory(handle: Long)

    @JvmStatic
    external fun nativeSetReadOnly(handle: Long, readOnly: Boolean): Int

    @JvmStatic
    external fun nativeIsReadOnly(handle: Long): Boolean

    @JvmStatic
    external fun nativeTokenize(handle: Long, start: Int, end: Int): IntArray

    @JvmStatic
    external fun nativeSyntaxLanguageSupported(language: String): Boolean

    @JvmStatic
    external fun nativeSyntaxTree(handle: Long): Long

    @JvmStatic
    external fun nativeSyntaxTreeDestroy(tree: Long)

    @JvmStatic
    external fun nativeSyntaxLanguage(tree: Long): String

    @JvmStatic
    external fun nativeSyntaxRootType(tree: Long): String

    @JvmStatic
    external fun nativeSyntaxNodeCount(tree: Long): Int

    @JvmStatic
    external fun nativeSyntaxHasErrors(tree: Long): Boolean

    @JvmStatic
    external fun nativeSyntaxHighlights(tree: Long): Array<String>

    @JvmStatic
    external fun nativeSyntaxErrors(tree: Long): Array<String>

    @JvmStatic
    external fun nativeComplete(handle: Long, offset: Int): Array<String>

    @JvmStatic
    external fun nativeDiagnose(handle: Long): Array<String>

    @JvmStatic
    external fun nativeFind(
        handle: Long,
        query: String,
        caseSensitive: Boolean,
        wholeWord: Boolean,
        regex: Boolean,
    ): LongArray

    @JvmStatic
    external fun nativeFoldingRanges(handle: Long): IntArray

    @JvmStatic
    external fun nativeMatchBracket(handle: Long, offset: Int): Int

    @JvmStatic
    external fun nativeSetEditable(handle: Long, editable: Boolean): Int

    @JvmStatic
    external fun nativeEditOrigin(handle: Long): Int

    @JvmStatic
    external fun nativeSetEditOrigin(handle: Long, origin: Int): Int

    @JvmStatic
    external fun nativeSetProposal(handle: Long, text: String, origin: Int): Int

    @JvmStatic
    external fun nativeHasProposal(handle: Long): Boolean

    @JvmStatic
    external fun nativeProposal(handle: Long): String?

    @JvmStatic
    external fun nativeProposalDiff(handle: Long): Array<String>

    @JvmStatic
    external fun nativeAcceptProposal(handle: Long): Int

    @JvmStatic
    external fun nativeRejectProposal(handle: Long): Int

    @JvmStatic
    external fun nativeSetGhostText(handle: Long, offset: Int, text: String, origin: Int): Int

    @JvmStatic
    external fun nativeGhostText(handle: Long): String?

    @JvmStatic
    external fun nativeAcceptGhostText(handle: Long, maxCodePoints: Int): Int

    @JvmStatic
    external fun nativeDismissGhostText(handle: Long): Int

    @JvmStatic
    external fun nativeCreateSnapshot(handle: Long): Long

    @JvmStatic
    external fun nativeSnapshotDestroy(handle: Long)

    @JvmStatic
    external fun nativeSnapshotVersion(handle: Long): Long

    @JvmStatic
    external fun nativeSnapshotLength(handle: Long): Long

    @JvmStatic
    external fun nativeSnapshotLanguage(handle: Long): Int

    @JvmStatic
    external fun nativeSnapshotText(handle: Long): String

    @JvmStatic
    external fun nativeRestoreSnapshot(document: Long, snapshot: Long): Int

    @JvmStatic
    external fun nativeGetSelection(handle: Long): Long

    @JvmStatic
    external fun nativeSetSelection(handle: Long, anchor: Int, focus: Int): Int

    @JvmStatic
    external fun nativeMoveCursor(handle: Long, move: Int, extend: Boolean): Int

    @JvmStatic
    external fun nativeCursorCount(handle: Long): Int

    @JvmStatic
    external fun nativeCursorAt(handle: Long, index: Int): Long

    @JvmStatic
    external fun nativeAddCursor(handle: Long, anchor: Int, focus: Int): Int

    @JvmStatic
    external fun nativeClearCursors(handle: Long)

    @JvmStatic
    external fun nativeMoveCursors(handle: Long, move: Int, extend: Boolean): Int

    @JvmStatic
    external fun nativeMultiInsert(handle: Long, text: String): Int

    @JvmStatic
    external fun nativeMultiErase(handle: Long, backward: Boolean): Int

    @JvmStatic
    external fun nativeSetLanguage(handle: Long, languageId: Int): Int

    @JvmStatic
    external fun nativeSetChangeListener(handle: Long, listener: Any?)

    @JvmStatic
    external fun nativeLspCreate(): Long

    @JvmStatic
    external fun nativeLspDestroy(handle: Long)

    @JvmStatic
    external fun nativeLspInitialize(handle: Long, rootUri: String)

    @JvmStatic
    external fun nativeLspFeed(handle: Long, data: String)

    @JvmStatic
    external fun nativeLspTakeOutgoing(handle: Long): String?

    @JvmStatic
    external fun nativeLspTakeResult(handle: Long): Array<String>?

    @JvmStatic
    external fun nativeLspTakeDiagnostics(handle: Long): Array<String>?

    @JvmStatic
    external fun nativeLspIsInitialized(handle: Long): Boolean

    @JvmStatic
    external fun nativeLspServerCapabilities(handle: Long): String

    @JvmStatic
    external fun nativeLspLastError(handle: Long): String

    @JvmStatic
    external fun nativeLspDidOpen(
        handle: Long,
        uri: String,
        languageId: String,
        version: Int,
        text: String,
    ): Int

    @JvmStatic
    external fun nativeLspDidChange(handle: Long, uri: String, version: Int, text: String): Int

    @JvmStatic
    external fun nativeLspDidClose(handle: Long, uri: String): Int

    @JvmStatic
    external fun nativeLspDidSave(
        handle: Long,
        uri: String,
        text: String,
        includeText: Boolean,
    ): Int

    @JvmStatic
    external fun nativeLspRequest(handle: Long, method: String, params: String): Long

    @JvmStatic
    external fun nativeLspNotify(handle: Long, method: String, params: String): Int

    @JvmStatic
    external fun nativeLspCancel(handle: Long, id: Long)

    @JvmStatic
    external fun nativeLspShutdown(handle: Long)

    @JvmStatic
    external fun nativeLspExit(handle: Long)

    @JvmStatic
    external fun nativeExecutionSupported(): Boolean

    @JvmStatic
    external fun nativeExecutionStart(
        command: String,
        args: Array<String>,
        workingDirectory: String?,
        envKeys: Array<String>,
        envValues: Array<String>,
        timeoutMs: Int,
        maxQueuedBytes: Int,
        shell: Boolean,
    ): Long

    @JvmStatic
    external fun nativeExecutionLastError(): String

    @JvmStatic
    external fun nativeExecutionPoll(handle: Long): Array<String>?

    @JvmStatic
    external fun nativeExecutionWrite(handle: Long, data: String): Int

    @JvmStatic
    external fun nativeExecutionCloseStdin(handle: Long)

    @JvmStatic
    external fun nativeExecutionCancel(handle: Long)

    @JvmStatic
    external fun nativeExecutionWait(handle: Long, timeoutMs: Int): Boolean

    @JvmStatic
    external fun nativeExecutionIsRunning(handle: Long): Boolean

    @JvmStatic
    external fun nativeExecutionExitCode(handle: Long): Int

    @JvmStatic
    external fun nativeExecutionDestroy(handle: Long)
}
