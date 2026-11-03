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

    /**
     * True when a Tree-sitter grammar is bundled for [language].
     *
     * The lexical tokenizer works for every registered language; structural
     * syntax is limited to the grammars compiled into the native library.
     */
    fun supportsStructuralSyntax(language: String): Boolean =
        NativeCodeCore.nativeSyntaxLanguageSupported(language)

    /** Opens an LSP client. The host owns the language-server transport. */
    fun createLsp(): CodeCoreLsp = CodeCoreLsp.open()
}
