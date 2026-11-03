package dev.codecore.nativebridge;

/**
 * Java mirror of the Kotlin JNI bridge used by the smoke test.
 *
 * It deliberately uses the same package, class and method names as the Kotlin
 * object so the JNI symbols resolve identically.
 */
public final class NativeCodeCore {
    private NativeCodeCore() {}

    static {
        System.loadLibrary("codecore_jni");
    }

    public static native String nativeVersion();

    public static native long nativeCreate(String language, int tabSize, boolean insertSpaces);

    public static native void nativeDestroy(long handle);

    public static native int nativeSetText(long handle, String text);

    public static native String nativeGetText(long handle);

    public static native long nativeLength(long handle);

    public static native long nativeDocumentVersion(long handle);

    public static native long nativeLineCount(long handle);

    public static native int nativeLanguageId(long handle);

    public static native String nativeLastError(long handle);

    public static native int nativeInsert(long handle, int offset, String text);

    public static native int nativeErase(long handle, int start, int end);

    public static native int nativeReplace(long handle, int start, int end, String text);

    public static native int nativeUndo(long handle);

    public static native int nativeRedo(long handle);

    public static native boolean nativeCanUndo(long handle);

    public static native boolean nativeCanRedo(long handle);

    public static native void nativeClearHistory(long handle);

    public static native int nativeSetReadOnly(long handle, boolean readOnly);

    public static native boolean nativeIsReadOnly(long handle);

    public static native int[] nativeTokenize(long handle, int start, int end);

    public static native boolean nativeSyntaxLanguageSupported(String language);

    public static native long nativeSyntaxTree(long handle);

    public static native void nativeSyntaxTreeDestroy(long tree);

    public static native String nativeSyntaxLanguage(long tree);

    public static native String nativeSyntaxRootType(long tree);

    public static native int nativeSyntaxNodeCount(long tree);

    public static native boolean nativeSyntaxHasErrors(long tree);

    public static native String[] nativeSyntaxHighlights(long tree);

    public static native String[] nativeSyntaxErrors(long tree);

    public static native String[] nativeComplete(long handle, int offset);

    public static native String[] nativeDiagnose(long handle);

    public static native long[] nativeFind(
            long handle, String query, boolean caseSensitive, boolean wholeWord, boolean regex);

    public static native int[] nativeFoldingRanges(long handle);

    public static native int nativeMatchBracket(long handle, int offset);

    public static native int nativeSetEditable(long handle, boolean editable);

    public static native int nativeEditOrigin(long handle);

    public static native int nativeSetEditOrigin(long handle, int origin);

    public static native int nativeSetProposal(long handle, String text, int origin);

    public static native boolean nativeHasProposal(long handle);

    public static native String nativeProposal(long handle);

    public static native String[] nativeProposalDiff(long handle);

    public static native int nativeAcceptProposal(long handle);

    public static native int nativeRejectProposal(long handle);

    public static native int nativeSetGhostText(
            long handle, int offset, String text, int origin);

    public static native String nativeGhostText(long handle);

    public static native int nativeAcceptGhostText(long handle, int maxCodePoints);

    public static native int nativeDismissGhostText(long handle);

    public static native long nativeCreateSnapshot(long handle);

    public static native void nativeSnapshotDestroy(long handle);

    public static native long nativeSnapshotVersion(long handle);

    public static native long nativeSnapshotLength(long handle);

    public static native int nativeSnapshotLanguage(long handle);

    public static native String nativeSnapshotText(long handle);

    public static native int nativeRestoreSnapshot(long document, long snapshot);

    public static native long nativeGetSelection(long handle);

    public static native int nativeSetSelection(long handle, int anchor, int focus);

    public static native int nativeMoveCursor(long handle, int move, boolean extend);

    public static native int nativeCursorCount(long handle);

    public static native long nativeCursorAt(long handle, int index);

    public static native int nativeAddCursor(long handle, int anchor, int focus);

    public static native void nativeClearCursors(long handle);

    public static native int nativeMoveCursors(long handle, int move, boolean extend);

    public static native int nativeMultiInsert(long handle, String text);

    public static native int nativeMultiErase(long handle, boolean backward);

    public static native int nativeSetLanguage(long handle, int languageId);

    public static native void nativeSetChangeListener(long handle, Object listener);

    public static native long nativeLspCreate();

    public static native void nativeLspDestroy(long handle);

    public static native void nativeLspInitialize(long handle, String rootUri);

    public static native void nativeLspFeed(long handle, String data);

    public static native String nativeLspTakeOutgoing(long handle);

    public static native String[] nativeLspTakeResult(long handle);

    public static native String[] nativeLspTakeDiagnostics(long handle);

    public static native boolean nativeLspIsInitialized(long handle);

    public static native String nativeLspServerCapabilities(long handle);

    public static native String nativeLspLastError(long handle);

    public static native int nativeLspDidOpen(
            long handle, String uri, String languageId, int version, String text);

    public static native int nativeLspDidChange(long handle, String uri, int version, String text);

    public static native int nativeLspDidClose(long handle, String uri);

    public static native int nativeLspDidSave(
            long handle, String uri, String text, boolean includeText);

    public static native long nativeLspRequest(long handle, String method, String params);

    public static native int nativeLspNotify(long handle, String method, String params);

    public static native void nativeLspCancel(long handle, long id);

    public static native void nativeLspShutdown(long handle);

    public static native void nativeLspExit(long handle);

    public static native boolean nativeExecutionSupported();

    public static native long nativeExecutionStart(
            String command,
            String[] args,
            String workingDirectory,
            String[] envKeys,
            String[] envValues,
            int timeoutMs,
            int maxQueuedBytes,
            boolean shell);

    public static native String nativeExecutionLastError();

    public static native String[] nativeExecutionPoll(long handle);

    public static native int nativeExecutionWrite(long handle, String data);

    public static native void nativeExecutionCloseStdin(long handle);

    public static native void nativeExecutionCancel(long handle);

    public static native boolean nativeExecutionWait(long handle, int timeoutMs);

    public static native boolean nativeExecutionIsRunning(long handle);

    public static native int nativeExecutionExitCode(long handle);

    public static native void nativeExecutionDestroy(long handle);
}
