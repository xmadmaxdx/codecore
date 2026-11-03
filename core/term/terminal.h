#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace codecore {

struct TerminalOptions {
  /* Empty selects the platform default: $SHELL, /bin/sh, or %COMSPEC%. */
  std::string command;
  std::vector<std::string> arguments;
  std::string workingDirectory;
  std::vector<std::pair<std::string, std::string>> environment;

  uint32_t columns = 80;
  uint32_t rows = 24;

  /* Bounded output queue; readers block once the host falls behind. */
  size_t maxQueuedOutputBytes = 1024u * 1024u;
};

struct TerminalEvent {
  enum class Kind {
    Output = 0,
    Exit = 1,
  };

  Kind kind = Kind::Output;
  uint64_t sequence = 0;
  std::string data;
  int32_t exitCode = -1;
};

/*
 * An interactive shell session attached to a pseudo-terminal.
 *
 * On POSIX and Android the child runs on a real PTY (`posix_openpt`), so it is
 * a controlling terminal: job control, line editing, `TIOCSWINSZ`-based resize
 * and terminal-size signals behave as in a normal shell.
 *
 * Windows currently uses piped stdin/stdout with a plain `cmd.exe`; `usesPty()`
 * reports false there and `resize()` is a documented no-op. ConPTY support is
 * not shipped because the pseudoconsole detached its input on the Windows 11
 * systems tested; a piped shell is honest about what it is instead of
 * pretending to be a terminal.
 *
 * The terminal UI, key handling and theming belong to the host. This class owns
 * the process, the shell's byte stream and window-size changes only.
 *
 * Output is a raw terminal byte stream, not a screen buffer. Output is polled;
 * there are no callbacks, so the host keeps thread affinity.
 */
class TerminalSession {
 public:
  ~TerminalSession();

  TerminalSession(const TerminalSession&) = delete;
  TerminalSession& operator=(const TerminalSession&) = delete;

  static std::unique_ptr<TerminalSession> start(const TerminalOptions& options,
                                                std::string* errorMessage);

  /* Writes keystrokes or pasted text to the shell. */
  bool write(std::string_view data);

  /* Updates the PTY window size; a no-op on the piped fallback. */
  bool resize(uint32_t columns, uint32_t rows);

  /* Non-blocking. Returns true for each output chunk or the final exit event. */
  bool poll(TerminalEvent* out);

  /* Waits for exit or timeout; zero is a non-blocking state check. */
  bool wait(uint32_t timeoutMs);

  bool running() const;
  int32_t exitCode() const;

  /* True when the child is attached to a real pseudo-terminal. */
  bool usesPty() const;

  uint32_t columns() const;
  uint32_t rows() const;

  /* Terminates the process tree and releases the terminal. */
  void close();

  const std::string& lastError() const;

 private:
  TerminalSession();

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace codecore
