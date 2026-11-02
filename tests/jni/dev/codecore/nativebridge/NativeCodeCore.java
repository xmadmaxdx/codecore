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

    public static native String[] nativeComplete(long handle, int offset);

    public static native String[] nativeDiagnose(long handle);

    public static native int[] nativeFind(
            long handle, String query, boolean caseSensitive, boolean wholeWord);

    public static native int[] nativeFoldingRanges(long handle);

    public static native int nativeMatchBracket(long handle, int offset);
}
