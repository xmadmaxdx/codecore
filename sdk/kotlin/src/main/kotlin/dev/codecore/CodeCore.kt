package dev.codecore

import dev.codecore.nativebridge.NativeCodeCore

/** Entry point for the CodeCore Kotlin SDK. */
object CodeCore {
    val version: String
        get() = NativeCodeCore.nativeVersion()

    /**
     * Creates a document for one of CodeCore's registered languages.
     *
     * @throws CodeCoreException when the language is unknown.
     */
    fun createDocument(
        language: String = "python",
        tabSize: Int = 4,
        insertSpaces: Boolean = true,
    ): CodeCoreDocument {
        val handle = NativeCodeCore.nativeCreate(language, tabSize, insertSpaces)
        if (handle == 0L) {
            throw CodeCoreException(4, "Unknown CodeCore language '$language'")
        }
        return CodeCoreDocument(handle)
    }
}
