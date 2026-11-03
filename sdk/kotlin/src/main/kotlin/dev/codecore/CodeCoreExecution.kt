package dev.codecore

import dev.codecore.nativebridge.NativeCodeCore

/**
 * One child process started by [CodeCore.startExecution].
 *
 * Poll [poll] to drain output; the exit event is delivered after all queued
 * output. [close] cancels a running child and releases the native session.
 */
class CodeCoreExecution internal constructor(private var handle: Long) : AutoCloseable {

    /** Non-blocking. Returns null when no output or exit event is ready. */
    fun poll(): ExecutionEvent? {
        val fields = NativeCodeCore.nativeExecutionPoll(requireHandle()) ?: return null
        if (fields.size < 7) {
            return null
        }
        return ExecutionEvent(
            kind = if (fields[0] == "0") ExecutionEventKind.OUTPUT else ExecutionEventKind.EXIT,
            stream = if (fields[1] == "0") ExecutionStream.STDOUT else ExecutionStream.STDERR,
            sequence = fields[2].toLongOrNull() ?: 0L,
            data = fields[3],
            exitCode = fields[4].toIntOrNull() ?: -1,
            cancelled = fields[5] == "1",
            timedOut = fields[6] == "1",
        )
    }

    /**
     * Writes UTF-8 bytes to the child's stdin.
     *
     * @throws CodeCoreException when the process has exited or the pipe failed.
     */
    fun write(data: String) {
        val status = NativeCodeCore.nativeExecutionWrite(requireHandle(), data)
        if (status != 0) {
            throw CodeCoreException(status, "execution write failed")
        }
    }

    /** Closes stdin, signalling EOF. */
    fun closeStdin() {
        NativeCodeCore.nativeExecutionCloseStdin(requireHandle())
    }

    /** Requests termination of the process tree. Does not block. */
    fun cancel() {
        NativeCodeCore.nativeExecutionCancel(requireHandle())
    }

    /**
     * Waits for the process to exit. A zero timeout only checks the current
     * state. Returns true when the process is no longer running.
     */
    fun waitFor(timeoutMs: Long = 0L): Boolean {
        val clamped = timeoutMs.coerceIn(0L, Int.MAX_VALUE.toLong()).toInt()
        return NativeCodeCore.nativeExecutionWait(requireHandle(), clamped)
    }

    val isRunning: Boolean
        get() = NativeCodeCore.nativeExecutionIsRunning(requireHandle())

    /** -1 until exit. Signal deaths report 128 + signal. */
    val exitCode: Int
        get() = NativeCodeCore.nativeExecutionExitCode(requireHandle())

    override fun close() {
        if (handle != 0L) {
            NativeCodeCore.nativeExecutionDestroy(handle)
            handle = 0L
        }
    }

    private fun requireHandle(): Long {
        check(handle != 0L) { "CodeCoreExecution is closed" }
        return handle
    }
}
