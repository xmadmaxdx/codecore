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

    /**
     * Starts a child process. CodeCore ships no runtime; [command] is the
     * executable to run and the host owns its distribution.
     *
     * @throws CodeCoreException when the process cannot be started.
     */
    fun startExecution(
        command: String,
        arguments: List<String> = emptyList(),
        workingDirectory: String? = null,
        environment: Map<String, String> = emptyMap(),
        timeoutMs: Long = 0L,
        shell: Boolean = false,
        maxQueuedBytes: Int = 0,
    ): CodeCoreExecution {
        require(command.isNotEmpty()) { "command must not be empty" }
        require(!(shell && arguments.isNotEmpty())) {
            "arguments must be empty in shell mode"
        }
        val handle = NativeCodeCore.nativeExecutionStart(
            command = command,
            args = arguments.toTypedArray(),
            workingDirectory = workingDirectory,
            envKeys = environment.keys.toTypedArray(),
            envValues = environment.values.toTypedArray(),
            timeoutMs = timeoutMs.coerceIn(0L, Int.MAX_VALUE.toLong()).toInt(),
            maxQueuedBytes = maxQueuedBytes.coerceAtLeast(0),
            shell = shell,
        )
        if (handle == 0L) {
            throw CodeCoreException(7, NativeCodeCore.nativeExecutionLastError())
        }
        return CodeCoreExecution(handle)
    }

    /** Opens an LSP client. The host owns the language-server transport. */
    fun createLsp(): CodeCoreLsp = CodeCoreLsp.open()
}
