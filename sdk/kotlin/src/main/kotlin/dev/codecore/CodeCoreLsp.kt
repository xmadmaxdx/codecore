package dev.codecore

import dev.codecore.nativebridge.NativeCodeCore

/** One completed LSP response. [payload] is the raw JSON result or error object. */
data class LspResult(
    val id: Long,
    val method: String,
    val payload: String,
    val isError: Boolean,
)

/** Diagnostics published by the language server for one document. */
data class LspDiagnostics(val uri: String, val payload: String)

/**
 * Transport-agnostic LSP client.
 *
 * The host owns the language-server process, socket or pipe. Write every frame
 * returned by [takeOutgoing] to the server, and pass received bytes to [feed].
 */
class CodeCoreLsp private constructor(private var handle: Long) : AutoCloseable {

    companion object {
        internal fun open(): CodeCoreLsp {
            val handle = NativeCodeCore.nativeLspCreate()
            if (handle == 0L) {
                throw CodeCoreException(5, "LSP client allocation failed")
            }
            return CodeCoreLsp(handle)
        }
    }

    val isInitialized: Boolean
        get() = NativeCodeCore.nativeLspIsInitialized(requireHandle())

    val serverCapabilities: String
        get() = NativeCodeCore.nativeLspServerCapabilities(requireHandle())

    val lastError: String
        get() = NativeCodeCore.nativeLspLastError(requireHandle())

    fun initialize(rootUri: String) {
        NativeCodeCore.nativeLspInitialize(requireHandle(), rootUri)
    }

    /** Feeds raw bytes received from the language server. */
    fun feed(chunk: String) {
        NativeCodeCore.nativeLspFeed(requireHandle(), chunk)
    }

    /** Returns the next complete outgoing frame, or null when the queue is empty. */
    fun takeOutgoing(): String? = NativeCodeCore.nativeLspTakeOutgoing(requireHandle())

    fun takeResult(): LspResult? {
        val fields = NativeCodeCore.nativeLspTakeResult(requireHandle()) ?: return null
        if (fields.size < 4) {
            return null
        }
        return LspResult(
            id = fields[0].toLongOrNull() ?: 0L,
            method = fields[1],
            payload = fields[2],
            isError = fields[3] == "1",
        )
    }

    fun takeDiagnostics(): LspDiagnostics? {
        val fields = NativeCodeCore.nativeLspTakeDiagnostics(requireHandle()) ?: return null
        if (fields.size < 2) {
            return null
        }
        return LspDiagnostics(uri = fields[0], payload = fields[1])
    }

    fun didOpen(uri: String, languageId: String, version: Int, text: String) {
        check(
            NativeCodeCore.nativeLspDidOpen(
                requireHandle(), uri, languageId, version, text,
            ),
        )
    }

    fun didChange(uri: String, version: Int, text: String) {
        check(NativeCodeCore.nativeLspDidChange(requireHandle(), uri, version, text))
    }

    fun didClose(uri: String) {
        check(NativeCodeCore.nativeLspDidClose(requireHandle(), uri))
    }

    fun didSave(uri: String, text: String, includeText: Boolean = true) {
        check(NativeCodeCore.nativeLspDidSave(requireHandle(), uri, text, includeText))
    }

    fun completion(uri: String, line: Int, character: Int): Long =
        positionRequest("textDocument/completion", uri, line, character)

    fun hover(uri: String, line: Int, character: Int): Long =
        positionRequest("textDocument/hover", uri, line, character)

    fun definition(uri: String, line: Int, character: Int): Long =
        positionRequest("textDocument/definition", uri, line, character)

    fun signatureHelp(uri: String, line: Int, character: Int): Long =
        positionRequest("textDocument/signatureHelp", uri, line, character)

    fun references(
        uri: String,
        line: Int,
        character: Int,
        includeDeclaration: Boolean = true,
    ): Long = request(
        "textDocument/references",
        positionParams(uri, line, character).dropLast(1) +
            ",\"context\":{\"includeDeclaration\":$includeDeclaration}}",
    )

    fun documentSymbols(uri: String): Long =
        request("textDocument/documentSymbol", "{\"textDocument\":{\"uri\":${quote(uri)}}}")

    fun workspaceSymbols(query: String): Long =
        request("workspace/symbol", "{\"query\":${quote(query)}}")

    fun rename(uri: String, line: Int, character: Int, newName: String): Long =
        request(
            "textDocument/rename",
            positionParams(uri, line, character).dropLast(1) +
                ",\"newName\":${quote(newName)}}",
        )

    fun formatting(
        uri: String,
        tabSize: Int = 4,
        insertSpaces: Boolean = true,
    ): Long = request(
        "textDocument/formatting",
        "{\"textDocument\":{\"uri\":${quote(uri)}},\"options\":{" +
            "\"tabSize\":$tabSize,\"insertSpaces\":$insertSpaces}}",
    )

    fun codeAction(
        uri: String,
        startLine: Int,
        startCharacter: Int,
        endLine: Int,
        endCharacter: Int,
    ): Long = request(
        "textDocument/codeAction",
        "{\"textDocument\":{\"uri\":${quote(uri)}},\"range\":{" +
            "\"start\":{\"line\":$startLine,\"character\":$startCharacter}," +
            "\"end\":{\"line\":$endLine,\"character\":$endCharacter}}," +
            "\"context\":{\"diagnostics\":[]}}",
    )

    fun semanticTokens(uri: String): Long = request(
        "textDocument/semanticTokens/full",
        "{\"textDocument\":{\"uri\":${quote(uri)}}}",
    )

    /** Sends an arbitrary LSP request with an already encoded JSON object. */
    fun request(method: String, paramsJson: String): Long {
        val id = NativeCodeCore.nativeLspRequest(requireHandle(), method, paramsJson)
        if (id < 0L) {
            throw CodeCoreException(1, lastError)
        }
        return id
    }

    /** Sends an arbitrary LSP notification with an already encoded JSON object. */
    fun notify(method: String, paramsJson: String) {
        check(NativeCodeCore.nativeLspNotify(requireHandle(), method, paramsJson))
    }

    fun cancel(id: Long) {
        NativeCodeCore.nativeLspCancel(requireHandle(), id)
    }

    fun shutdown() {
        NativeCodeCore.nativeLspShutdown(requireHandle())
    }

    fun exit() {
        NativeCodeCore.nativeLspExit(requireHandle())
    }

    override fun close() {
        val current = handle
        if (current == 0L) {
            return
        }
        handle = 0L
        NativeCodeCore.nativeLspDestroy(current)
    }

    private fun positionRequest(method: String, uri: String, line: Int, character: Int): Long =
        request(method, positionParams(uri, line, character))

    private fun positionParams(uri: String, line: Int, character: Int): String =
        "{\"textDocument\":{\"uri\":${quote(uri)}}," +
            "\"position\":{\"line\":$line,\"character\":$character}}"

    private fun check(status: Int) {
        if (status != 0) {
            throw CodeCoreException(status, lastError)
        }
    }

    private fun requireHandle(): Long {
        val current = handle
        check(current != 0L) { "CodeCoreLsp has been closed" }
        return current
    }

    private fun quote(value: String): String {
        val out = StringBuilder(value.length + 2)
        out.append('"')
        for (character in value) {
            when (character) {
                '"' -> out.append("\\\"")
                '\\' -> out.append("\\\\")
                '\n' -> out.append("\\n")
                '\r' -> out.append("\\r")
                '\t' -> out.append("\\t")
                '\b' -> out.append("\\b")
                '\u000C' -> out.append("\\f")
                else -> {
                    if (character.code < 0x20) {
                        out.append("\\u")
                        out.append(character.code.toString(16).padStart(4, '0'))
                    } else {
                        out.append(character)
                    }
                }
            }
        }
        out.append('"')
        return out.toString()
    }
}
