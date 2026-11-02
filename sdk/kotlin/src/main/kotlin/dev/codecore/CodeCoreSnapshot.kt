package dev.codecore

import dev.codecore.nativebridge.NativeCodeCore

/** Immutable native snapshot. It remains valid after its document is closed. */
class CodeCoreSnapshot internal constructor(initialHandle: Long) : AutoCloseable {
    private var handle: Long = initialHandle
    private val lock = Any()

    val isClosed: Boolean
        get() = synchronized(lock) { handle == 0L }

    val version: Long
        get() = synchronized(lock) { NativeCodeCore.nativeSnapshotVersion(nativeHandle()) }

    val length: Long
        get() = synchronized(lock) { NativeCodeCore.nativeSnapshotLength(nativeHandle()) }

    val languageId: Int
        get() = synchronized(lock) { NativeCodeCore.nativeSnapshotLanguage(nativeHandle()) }

    val text: String
        get() = synchronized(lock) { NativeCodeCore.nativeSnapshotText(nativeHandle()) }

    override fun close() {
        synchronized(lock) {
            if (handle == 0L) {
                return
            }
            NativeCodeCore.nativeSnapshotDestroy(handle)
            handle = 0
        }
    }

    internal fun nativeHandle(): Long {
        check(handle != 0L) { "CodeCoreSnapshot has been closed" }
        return handle
    }
}
