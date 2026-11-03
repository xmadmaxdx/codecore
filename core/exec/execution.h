#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace codecore {

enum class ExecutionOutputStream : uint32_t {
  Stdout = 0,
  Stderr = 1,
};

enum class ExecutionState : uint32_t {
  Running = 0,
  Exited = 1,
  Failed = 2,
  Cancelled = 3,
  TimedOut = 4,
};

struct ExecutionOptions {
  /* Executable name or path, resolved through PATH when it has no separator. */
  std::string command;
  std::vector<std::string> arguments;
  std::string workingDirectory;

  /* Additions/overrides merged into the inherited environment. */
  std::vector<std::pair<std::string, std::string>> environment;

  /* 0 disables the timeout. */
  uint32_t timeoutMs = 0;

  /*
   * Soft backpressure bound. Reader threads block once this many bytes are
   * waiting for the host to poll, which pushes back through the OS pipe.
   */
  size_t maxQueuedOutputBytes = 1024u * 1024u;

  /*
   * Runs `command` verbatim through the platform shell (cmd.exe / sh -c).
   * `arguments` must be empty in shell mode. Off by default.
   */
  bool shell = false;
};

struct ExecutionEvent {
  enum class Kind {
    Output = 0,
    Exit = 1,
  };

  Kind kind = Kind::Output;
  ExecutionOutputStream stream = ExecutionOutputStream::Stdout;

  /* Strictly increasing per session, across stdout and stderr. */
  uint64_t sequence = 0;
  std::string data;

  /* Exit events only. */
  int32_t exitCode = -1;
  bool cancelled = false;
  bool timedOut = false;
};

/*
 * One child process with piped stdin, stdout and stderr.
 *
 * All methods are safe to call from one host thread while internal reader and
 * waiter threads run. Output is queued and polled; there are no callbacks, so
 * the host controls thread affinity. A session is independent of Document and
 * Engine and does not affect the editor state.
 *
 * Per-stream byte order is guaranteed. stdout/stderr interleaving is the order
 * in which the reader threads enqueued chunks, not a merged OS stream.
 *
 * Execution is opt-in functionality: CodeCore ships no interpreter, compiler
 * or runtime. On Android, app-data executables are blocked by platform policy;
 * hosts must ship runnable binaries as packaged native libraries.
 */
class ExecutionSession {
 public:
  ~ExecutionSession();

  ExecutionSession(const ExecutionSession&) = delete;
  ExecutionSession& operator=(const ExecutionSession&) = delete;

  /*
   * Starts the process. Returns nullptr and fills `errorMessage` on failure.
   * The working directory and executable are validated before spawning.
   */
  static std::unique_ptr<ExecutionSession> start(const ExecutionOptions& options,
                                                 std::string* errorMessage);

  /* Writes to the child's stdin. Returns false once the process is gone. */
  bool write(std::string_view data);

  /* Closes stdin, signalling EOF to the child. */
  void closeStdin();

  /* Requests termination of the process tree. Does not block. */
  void cancel();

  /*
   * Waits until the process has exited or the timeout expires.
   * Returns true when the process is no longer running. A zero timeout is a
   * non-blocking state check.
   */
  bool wait(uint32_t timeoutMs);

  /*
   * Non-blocking. Returns true when an output chunk or the final exit event was
   * written to `out`. Output is always drained before the exit event.
   */
  bool poll(ExecutionEvent* out);

  bool running() const;
  ExecutionState state() const;

  /* -1 until the process has exited. Signal deaths report 128 + signal. */
  int32_t exitCode() const;

  const std::string& lastError() const;

 private:
  ExecutionSession();

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace codecore
