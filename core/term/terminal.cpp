#include "core/term/terminal.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace codecore {
namespace {

#ifdef _WIN32

std::wstring widen(std::string_view text) {
  if (text.empty()) {
    return std::wstring();
  }
  const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(),
                                        static_cast<int>(text.size()), nullptr, 0);
  if (count <= 0) {
    return std::wstring();
  }
  std::wstring result(static_cast<size_t>(count), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(),
                      count);
  return result;
}

std::string windowsError(const char* prefix) {
  return std::string(prefix) + " failed (error " +
         std::to_string(static_cast<unsigned long>(GetLastError())) + ")";
}

std::wstring quoteWindowsArgument(const std::wstring& argument) {
  const bool needsQuotes =
      argument.empty() || argument.find_first_of(L" \t\n\v\"") != std::wstring::npos;
  std::wstring result;
  if (needsQuotes) {
    result.push_back(L'"');
  }
  size_t backslashes = 0;
  for (wchar_t character : argument) {
    if (character == L'\\') {
      ++backslashes;
      continue;
    }
    if (character == L'"') {
      result.append(backslashes * 2 + 1, L'\\');
      result.push_back(L'"');
      backslashes = 0;
      continue;
    }
    result.append(backslashes, L'\\');
    backslashes = 0;
    result.push_back(character);
  }
  if (needsQuotes) {
    result.append(backslashes * 2, L'\\');
    result.push_back(L'"');
  } else {
    result.append(backslashes, L'\\');
  }
  return result;
}

std::wstring buildEnvironmentBlock(
    const std::vector<std::pair<std::string, std::string>>& overrides) {
  std::vector<std::pair<std::wstring, std::wstring>> entries;
  LPWCH block = GetEnvironmentStringsW();
  if (block) {
    for (LPWCH cursor = block; *cursor; cursor += wcslen(cursor) + 1) {
      std::wstring entry(cursor);
      if (entry.empty() || entry.front() == L'=') {
        continue;
      }
      const size_t separator = entry.find(L'=');
      if (separator == std::wstring::npos || separator == 0) {
        continue;
      }
      entries.emplace_back(entry.substr(0, separator), entry.substr(separator + 1));
    }
    FreeEnvironmentStringsW(block);
  }
  for (const auto& overrideEntry : overrides) {
    const std::wstring key = widen(overrideEntry.first);
    const std::wstring value = widen(overrideEntry.second);
    bool replaced = false;
    for (auto& entry : entries) {
      if (CompareStringOrdinal(entry.first.c_str(), -1, key.c_str(), -1, TRUE) == CSTR_EQUAL) {
        entry.second = value;
        replaced = true;
        break;
      }
    }
    if (!replaced) {
      entries.emplace_back(key, value);
    }
  }
  std::vector<wchar_t> buffer;
  for (const auto& entry : entries) {
    buffer.insert(buffer.end(), entry.first.begin(), entry.first.end());
    buffer.push_back(L'=');
    buffer.insert(buffer.end(), entry.second.begin(), entry.second.end());
    buffer.push_back(L'\0');
  }
  buffer.push_back(L'\0');
  return std::wstring(buffer.begin(), buffer.end());
}

#else

std::string posixError(const char* prefix) {
  return std::string(prefix) + " failed: " + std::strerror(errno);
}

#endif

}  // namespace

struct TerminalSession::Impl {
  mutable std::mutex mutex;
  std::condition_variable cv;
  std::deque<TerminalEvent> events;
  size_t queuedBytes = 0;
  size_t maxQueuedBytes = 1024u * 1024u;
  uint64_t sequence = 0;
  bool running = true;
  bool exitReported = false;
  int32_t exitCode = -1;
  std::string lastError;
  uint32_t columns = 80;
  uint32_t rows = 24;
  bool pty = false;
  std::thread readerThread;
  std::thread waiterThread;

#ifdef _WIN32
  HANDLE process = nullptr;
  HANDLE thread = nullptr;
  HANDLE inputWrite = nullptr;
  HANDLE outputRead = nullptr;
#else
  int master = -1;
  long pid = -1;
#endif

  void pushOutput(std::string data, uint64_t sequenceNumber) {
    {
      std::unique_lock<std::mutex> lock(mutex);
      cv.wait(lock, [this] {
        return !running || queuedBytes < maxQueuedBytes || maxQueuedBytes == 0;
      });
      if (!running) {
        return;
      }
      TerminalEvent event;
      event.kind = TerminalEvent::Kind::Output;
      event.sequence = sequenceNumber;
      event.data = std::move(data);
      queuedBytes += event.data.size();
      events.push_back(std::move(event));
    }
    cv.notify_all();
  }

  void pushExit(int32_t code) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      running = false;
      exitCode = code;
      if (!exitReported) {
        TerminalEvent event;
        event.kind = TerminalEvent::Kind::Exit;
        event.sequence = sequence++;
        event.exitCode = code;
        events.push_back(std::move(event));
        exitReported = true;
      }
    }
    cv.notify_all();
  }
};

TerminalSession::TerminalSession() : impl_(std::make_unique<Impl>()) {}

TerminalSession::~TerminalSession() { close(); }

bool TerminalSession::usesPty() const { return impl_->pty; }

uint32_t TerminalSession::columns() const { return impl_->columns; }

uint32_t TerminalSession::rows() const { return impl_->rows; }

const std::string& TerminalSession::lastError() const { return impl_->lastError; }

bool TerminalSession::running() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->running;
}

int32_t TerminalSession::exitCode() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->exitCode;
}

bool TerminalSession::write(std::string_view data) {
  if (data.empty() || !impl_ || !running()) {
    return false;
  }
#ifdef _WIN32
  if (!impl_->inputWrite) {
    return false;
  }
  size_t offset = 0;
  while (offset < data.size()) {
    DWORD written = 0;
    const DWORD chunk = static_cast<DWORD>(std::min<size_t>(data.size() - offset, 16u * 1024u));
    if (!WriteFile(impl_->inputWrite, data.data() + offset, chunk, &written, nullptr) ||
        written == 0) {
      impl_->lastError = windowsError("WriteFile");
      return false;
    }
    offset += written;
  }
  return true;
#else
  if (impl_->master < 0) {
    return false;
  }
  size_t offset = 0;
  while (offset < data.size()) {
    const ssize_t written = ::write(impl_->master, data.data() + offset, data.size() - offset);
    if (written <= 0) {
      if (errno == EINTR) {
        continue;
      }
      return false;
    }
    offset += static_cast<size_t>(written);
  }
  return true;
#endif
}

bool TerminalSession::resize(uint32_t columns, uint32_t rows) {
  if (columns == 0 || rows == 0) {
    return false;
  }
  impl_->columns = columns;
  impl_->rows = rows;
#ifdef _WIN32
  // Piped shells have no window size to update.
  return false;
#else
  if (impl_->master < 0) {
    return false;
  }
  struct winsize window;
  std::memset(&window, 0, sizeof(window));
  window.ws_col = static_cast<unsigned short>(columns);
  window.ws_row = static_cast<unsigned short>(rows);
  return ::ioctl(impl_->master, TIOCSWINSZ, &window) == 0;
#endif
}

bool TerminalSession::wait(uint32_t timeoutMs) {
  std::unique_lock<std::mutex> lock(impl_->mutex);
  if (!impl_->running) {
    return true;
  }
  if (timeoutMs == 0) {
    return !impl_->running;
  }
  impl_->cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                     [this] { return !impl_->running; });
  return !impl_->running;
}

bool TerminalSession::poll(TerminalEvent* out) {
  if (!out) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->events.empty()) {
    return false;
  }
  TerminalEvent event = std::move(impl_->events.front());
  impl_->events.pop_front();
  if (event.kind == TerminalEvent::Kind::Output) {
    impl_->queuedBytes -= std::min(impl_->queuedBytes, event.data.size());
  }
  *out = std::move(event);
  return true;
}

std::unique_ptr<TerminalSession> TerminalSession::start(const TerminalOptions& options,
                                                        std::string* errorMessage) {
  auto session = std::unique_ptr<TerminalSession>(new TerminalSession());
  Impl& impl = *session->impl_;
  impl.columns = options.columns == 0 ? 80 : options.columns;
  impl.rows = options.rows == 0 ? 24 : options.rows;
  impl.maxQueuedBytes = options.maxQueuedOutputBytes;

#ifdef _WIN32
  std::wstring command = widen(options.command);
  if (command.empty()) {
    wchar_t buffer[MAX_PATH];
    const DWORD length = GetEnvironmentVariableW(L"COMSPEC", buffer, MAX_PATH);
    command = length > 0 && length < MAX_PATH ? std::wstring(buffer, length) : L"cmd.exe";
  }
  std::wstring commandLine = quoteWindowsArgument(command);
  if (options.command.empty()) {
    commandLine += L" /Q";
  }
  for (const std::string& argument : options.arguments) {
    commandLine += L" ";
    commandLine += quoteWindowsArgument(widen(argument));
  }

  SECURITY_ATTRIBUTES attributes;
  attributes.nLength = sizeof(attributes);
  attributes.lpSecurityDescriptor = nullptr;
  attributes.bInheritHandle = TRUE;

  HANDLE inputRead = nullptr;
  HANDLE inputWrite = nullptr;
  HANDLE outputRead = nullptr;
  HANDLE outputWrite = nullptr;
  if (!CreatePipe(&inputRead, &inputWrite, &attributes, 0) ||
      !CreatePipe(&outputRead, &outputWrite, &attributes, 0)) {
    impl.lastError = windowsError("CreatePipe");
    if (errorMessage) *errorMessage = impl.lastError;
    return nullptr;
  }
  SetHandleInformation(inputWrite, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(outputRead, HANDLE_FLAG_INHERIT, 0);

  std::wstring environment;
  const bool hasEnvironment = !options.environment.empty();
  if (hasEnvironment) {
    environment = buildEnvironmentBlock(options.environment);
  }

  STARTUPINFOW startup;
  std::memset(&startup, 0, sizeof(startup));
  startup.cb = sizeof(startup);
  startup.dwFlags = STARTF_USESTDHANDLES;
  startup.hStdInput = inputRead;
  startup.hStdOutput = outputWrite;
  startup.hStdError = outputWrite;

  std::vector<wchar_t> commandBuffer(commandLine.begin(), commandLine.end());
  commandBuffer.push_back(L'\0');
  const std::wstring workingDirectory = widen(options.workingDirectory);

  PROCESS_INFORMATION processInfo;
  std::memset(&processInfo, 0, sizeof(processInfo));
  const BOOL created = CreateProcessW(
      nullptr, commandBuffer.data(), nullptr, nullptr, TRUE, CREATE_UNICODE_ENVIRONMENT,
      hasEnvironment ? environment.data() : nullptr,
      workingDirectory.empty() ? nullptr : workingDirectory.c_str(), &startup, &processInfo);

  CloseHandle(inputRead);
  CloseHandle(outputWrite);

  if (!created) {
    impl.lastError = windowsError("CreateProcess");
    if (errorMessage) *errorMessage = impl.lastError;
    CloseHandle(inputWrite);
    CloseHandle(outputRead);
    return nullptr;
  }

  impl.process = processInfo.hProcess;
  impl.thread = processInfo.hThread;
  impl.inputWrite = inputWrite;
  impl.outputRead = outputRead;
  impl.pty = false;

  Impl* state = &impl;
  state->readerThread = std::thread([state] {
    char buffer[4096];
    while (true) {
      DWORD read = 0;
      if (!ReadFile(state->outputRead, buffer, sizeof(buffer), &read, nullptr) || read == 0) {
        break;
      }
      uint64_t sequence = 0;
      {
        std::lock_guard<std::mutex> lock(state->mutex);
        sequence = state->sequence++;
      }
      state->pushOutput(std::string(buffer, read), sequence);
    }
  });

  state->waiterThread = std::thread([state] {
    WaitForSingleObject(state->process, INFINITE);
    DWORD code = 0;
    if (!GetExitCodeProcess(state->process, &code) || code == STILL_ACTIVE) {
      code = 1;
    }
    state->pushExit(static_cast<int32_t>(code));
  });

  return session;
#else
  std::string command = options.command;
  if (command.empty()) {
    const char* shell = std::getenv("SHELL");
    command = shell && *shell ? shell : "/bin/sh";
  }

  const int master = ::posix_openpt(O_RDWR | O_NOCTTY);
  if (master < 0) {
    impl.lastError = posixError("posix_openpt");
    if (errorMessage) *errorMessage = impl.lastError;
    return nullptr;
  }
  if (::grantpt(master) != 0 || ::unlockpt(master) != 0) {
    impl.lastError = posixError("grantpt/unlockpt");
    if (errorMessage) *errorMessage = impl.lastError;
    ::close(master);
    return nullptr;
  }
  const char* slaveName = ::ptsname(master);
  if (!slaveName) {
    impl.lastError = posixError("ptsname");
    if (errorMessage) *errorMessage = impl.lastError;
    ::close(master);
    return nullptr;
  }
  const std::string slavePath = slaveName;

  struct winsize window;
  std::memset(&window, 0, sizeof(window));
  window.ws_col = static_cast<unsigned short>(impl.columns);
  window.ws_row = static_cast<unsigned short>(impl.rows);
  ::ioctl(master, TIOCSWINSZ, &window);

  std::vector<std::string> argvStorage;
  argvStorage.push_back(command);
  for (const std::string& argument : options.arguments) {
    argvStorage.push_back(argument);
  }
  std::vector<char*> argv;
  argv.reserve(argvStorage.size() + 1);
  for (std::string& token : argvStorage) {
    argv.push_back(const_cast<char*>(token.c_str()));
  }
  argv.push_back(nullptr);

  const pid_t pid = ::fork();
  if (pid < 0) {
    impl.lastError = posixError("fork");
    if (errorMessage) *errorMessage = impl.lastError;
    ::close(master);
    return nullptr;
  }

  if (pid == 0) {
    ::setsid();
    const int slave = ::open(slavePath.c_str(), O_RDWR);
    if (slave >= 0) {
      ::ioctl(slave, TIOCSCTTY, 0);
      ::dup2(slave, STDIN_FILENO);
      ::dup2(slave, STDOUT_FILENO);
      ::dup2(slave, STDERR_FILENO);
      if (slave > STDERR_FILENO) {
        ::close(slave);
      }
    }
    ::close(master);
    for (const auto& entry : options.environment) {
      ::setenv(entry.first.c_str(), entry.second.c_str(), 1);
    }
    if (!options.workingDirectory.empty() && ::chdir(options.workingDirectory.c_str()) != 0) {
      _exit(126);
    }
    ::execvp(argv[0], argv.data());
    _exit(127);
  }

  impl.master = master;
  impl.pid = static_cast<long>(pid);
  impl.pty = true;

  Impl* state = &impl;
  state->readerThread = std::thread([state] {
    char buffer[4096];
    while (true) {
      const ssize_t read = ::read(state->master, buffer, sizeof(buffer));
      if (read <= 0) {
        if (read < 0 && errno == EINTR) {
          continue;
        }
        break;
      }
      uint64_t sequence = 0;
      {
        std::lock_guard<std::mutex> lock(state->mutex);
        sequence = state->sequence++;
      }
      state->pushOutput(std::string(buffer, static_cast<size_t>(read)), sequence);
    }
  });

  state->waiterThread = std::thread([state] {
    int status = 0;
    while (::waitpid(static_cast<pid_t>(state->pid), &status, 0) < 0) {
      if (errno != EINTR) {
        state->pushExit(1);
        return;
      }
    }
    int32_t code = 1;
    if (WIFEXITED(status)) {
      code = WEXITSTATUS(status);
    } else if (WIFSIGNALED(status)) {
      code = 128 + WTERMSIG(status);
    }
    state->pushExit(code);
  });

  return session;
#endif
}

void TerminalSession::close() {
  if (!impl_) {
    return;
  }
  Impl& impl = *impl_;

#ifdef _WIN32
  if (impl.process) {
    DWORD code = 0;
    if (GetExitCodeProcess(impl.process, &code) && code == STILL_ACTIVE) {
      TerminateProcess(impl.process, 1);
    }
  }
  if (impl.inputWrite) {
    CloseHandle(impl.inputWrite);
    impl.inputWrite = nullptr;
  }
#else
  if (impl.pid > 0) {
    ::kill(static_cast<pid_t>(impl.pid), SIGTERM);
    ::kill(static_cast<pid_t>(impl.pid), SIGHUP);
  }
  if (impl.master >= 0) {
    ::close(impl.master);
    impl.master = -1;
  }
#endif

  if (impl.readerThread.joinable()) {
    impl.readerThread.join();
  }
  if (impl.waiterThread.joinable()) {
    impl.waiterThread.join();
  }

#ifdef _WIN32
  if (impl.outputRead) {
    CloseHandle(impl.outputRead);
    impl.outputRead = nullptr;
  }
  if (impl.thread) {
    CloseHandle(impl.thread);
    impl.thread = nullptr;
  }
  if (impl.process) {
    CloseHandle(impl.process);
    impl.process = nullptr;
  }
#endif
}

}  // namespace codecore
