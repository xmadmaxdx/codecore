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
    external fun nativeComplete(handle: Long, offset: Int): Array<String>

    @JvmStatic
    external fun nativeDiagnose(handle: Long): Array<String>

    @JvmStatic
    external fun nativeFind(
        handle: Long,
        query: String,
        caseSensitive: Boolean,
        wholeWord: Boolean,
    ): IntArray

    @JvmStatic
    external fun nativeFoldingRanges(handle: Long): IntArray

    @JvmStatic
    external fun nativeMatchBracket(handle: Long, offset: Int): Int
}
