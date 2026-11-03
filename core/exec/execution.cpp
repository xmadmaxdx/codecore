#include "core/exec/execution.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <cstring>
#include <cwctype>
#include <deque>
#include <mutex>
#include <thread>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern char** environ;
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
  MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()),
                      result.data(), count);
  return result;
}

std::string windowsError(const char* prefix) {
  return std::string(prefix) + " failed (error " +
         std::to_string(static_cast<unsigned long>(GetLastError())) + ")";
}

/*
 * Quotes one argument using the CommandLineToArgvW / MSVCRT rules so the child
 * receives exactly the bytes the host passed.
 */
std::wstring quoteWindowsArgument(const std::wstring& argument) {
  const bool needsQuotes =
      argument.empty() ||
      argument.find_first_of(L" \t\n\v\"") != std::wstring::npos;
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

std::wstring buildCommandLine(const ExecutionOptions& options) {
  if (options.shell) {
    return L"cmd.exe /d /s /c \"" + widen(options.command) + L"\"";
  }
  std::vector<std::wstring> tokens;
  tokens.push_back(widen(options.command));
  for (const std::string& argument : options.arguments) {
    tokens.push_back(widen(argument));
  }
  std::wstring line;
  for (const std::wstring& token : tokens) {
    if (!line.empty()) {
      line.push_back(L' ');
    }
    line += quoteWindowsArgument(token);
  }
  return line;
}

bool equalNoCase(const std::wstring& left, const std::wstring& right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (size_t index = 0; index < left.size(); ++index) {
    if (std::towlower(left[index]) != std::towlower(right[index])) {
      return false;
    }
  }
  return true;
}

std::wstring buildEnvironmentBlock(const ExecutionOptions& options) {
  if (options.environment.empty()) {
    return std::wstring();
  }

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
      entries.emplace_back(entry.substr(0, separator),
                           entry.substr(separator + 1));
    }
    FreeEnvironmentStringsW(block);
  }

  for (const auto& [key, value] : options.environment) {
    const std::wstring wideKey = widen(key);
    const std::wstring wideValue = widen(value);
    bool replaced = false;
    for (auto& entry : entries) {
      if (equalNoCase(entry.first, wideKey)) {
        entry.second = wideValue;
        replaced = true;
        break;
      }
    }
    if (!replaced) {
      entries.emplace_back(wideKey, wideValue);
    }
  }

  std::sort(entries.begin(), entries.end(),
            [](const auto& left, const auto& right) {
              return left.first < right.first;
            });

  std::wstring result;
  for (const auto& entry : entries) {
    result += entry.first;
    result.push_back(L'=');
    result += entry.second;
    result.push_back(L'\0');
  }
  result.push_back(L'\0');
  return result;
}

#else

std::string shellQuote(std::string_view value) {
  std::string result("'");
  for (char character : value) {
    if (character == '\'') {
      result += "'\\''";
    } else {
      result.push_back(character);
    }
  }
  result.push_back('\'');
  return result;
}

std::string resolveExecutable(const std::string& command) {
  if (command.empty()) {
    return std::string();
  }
  if (command.find('/') != std::string::npos) {
    return access(command.c_str(), X_OK) == 0 ? command : std::string();
  }
  const char* path = std::getenv("PATH");
  const std::string search = path ? path : "/usr/local/bin:/usr/bin:/bin";
  size_t offset = 0;
  while (offset <= search.size()) {
    const size_t separator = search.find(':', offset);
    const size_t end = separator == std::string::npos ? search.size() : separator;
    const std::string directory = search.substr(offset, end - offset);
    const std::string candidate =
        (directory.empty() ? std::string(".") : directory) + "/" + command;
    if (access(candidate.c_str(), X_OK) == 0) {
      return candidate;
    }
    if (separator == std::string::npos) {
      break;
    }
    offset = separator + 1;
  }
  return std::string();
}

#endif

}  // namespace

struct ExecutionSession::Impl {
  ExecutionOptions options;

  mutable std::mutex mutex;
  std::condition_variable changed;
  std::condition_variable spaceAvailable;
  std::deque<ExecutionEvent> events;
  size_t queuedBytes = 0;
  uint64_t nextSequence = 1;
  ExecutionState state = ExecutionState::Running;
  int32_t exitCodeValue = -1;
  bool exitRecorded = false;
  bool exitEmitted = false;
  bool readersDone = false;
  int readersFinished = 0;
  std::atomic<bool> stopping{false};
  std::atomic<bool> processGone{false};
  bool cancelRequested = false;
  bool timedOut = false;
  std::string lastError;

  std::thread stdoutThread;
  std::thread stderrThread;
  std::thread waiterThread;

  std::mutex stdinMutex;
  bool stdinClosed = false;

#ifdef _WIN32
  HANDLE process = nullptr;
  HANDLE job = nullptr;
  HANDLE stdinWrite = nullptr;
  HANDLE stdoutRead = nullptr;
  HANDLE stderrRead = nullptr;
#else
  pid_t pid = -1;
  int stdinFd = -1;
  int stdoutFd = -1;
  int stderrFd = -1;
#endif

  bool spawn(std::string* error);
  void readLoop(ExecutionOutputStream stream);
  void waitLoop();
  void pushOutput(ExecutionOutputStream stream, std::string data);
  void readerFinished();
  void finish(int32_t code, ExecutionState finalState);
  void terminate();
  bool processExited(int32_t* code);
  void sleepSlice();
  void closeHandles();
};

namespace {

constexpr size_t kDefaultQueueBytes = 1024u * 1024u;
constexpr size_t kReadBufferBytes = 8192;

}  // namespace

#ifdef _WIN32

bool ExecutionSession::Impl::spawn(std::string* error) {
  HANDLE stdinRead = nullptr;
  HANDLE stdinWriteLocal = nullptr;
  HANDLE stdoutReadLocal = nullptr;
  HANDLE stdoutWrite = nullptr;
  HANDLE stderrReadLocal = nullptr;
  HANDLE stderrWrite = nullptr;

  auto cleanup = [&]() {
    for (HANDLE* handle : {&stdinRead, &stdinWriteLocal, &stdoutReadLocal,
                           &stdoutWrite, &stderrReadLocal, &stderrWrite}) {
      if (*handle) {
        CloseHandle(*handle);
        *handle = nullptr;
      }
    }
  };

  SECURITY_ATTRIBUTES attributes{};
  attributes.nLength = sizeof(attributes);
  attributes.bInheritHandle = TRUE;

  if (!CreatePipe(&stdinRead, &stdinWriteLocal, &attributes, 0)) {
    *error = windowsError("CreatePipe(stdin)");
    cleanup();
    return false;
  }
  if (!CreatePipe(&stdoutReadLocal, &stdoutWrite, &attributes, 0)) {
    *error = windowsError("CreatePipe(stdout)");
    cleanup();
    return false;
  }
  if (!CreatePipe(&stderrReadLocal, &stderrWrite, &attributes, 0)) {
    *error = windowsError("CreatePipe(stderr)");
    cleanup();
    return false;
  }

  SetHandleInformation(stdinWriteLocal, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(stdoutReadLocal, HANDLE_FLAG_INHERIT, 0);
  SetHandleInformation(stderrReadLocal, HANDLE_FLAG_INHERIT, 0);

  std::wstring commandLine = buildCommandLine(options);
  std::wstring workingDirectory = widen(options.workingDirectory);
  std::wstring environment = buildEnvironmentBlock(options);
  if (!commandLine.empty()) {
    commandLine.reserve(commandLine.size() + 1);
  }

  SIZE_T attributeSize = 0;
  InitializeProcThreadAttributeList(nullptr, 1, 0, &attributeSize);
  std::vector<unsigned char> attributeBuffer(attributeSize);
  auto* attributesList =
      reinterpret_cast<LPPROC_THREAD_ATTRIBUTE_LIST>(attributeBuffer.data());
  if (!InitializeProcThreadAttributeList(attributesList, 1, 0,
                                         &attributeSize)) {
    *error = windowsError("InitializeProcThreadAttributeList");
    cleanup();
    return false;
  }

  HANDLE childHandles[3] = {stdinRead, stdoutWrite, stderrWrite};
  if (!UpdateProcThreadAttribute(attributesList, 0,
                                 PROC_THREAD_ATTRIBUTE_HANDLE_LIST,
                                 childHandles, sizeof(childHandles), nullptr,
                                 nullptr)) {
    *error = windowsError("UpdateProcThreadAttribute");
    DeleteProcThreadAttributeList(attributesList);
    cleanup();
    return false;
  }

  STARTUPINFOEXW startup{};
  startup.StartupInfo.cb = sizeof(startup);
  startup.StartupInfo.dwFlags = STARTF_USESTDHANDLES;
  startup.StartupInfo.hStdInput = stdinRead;
  startup.StartupInfo.hStdOutput = stdoutWrite;
  startup.StartupInfo.hStdError = stderrWrite;
  startup.lpAttributeList = attributesList;

  PROCESS_INFORMATION processInfo{};
  const DWORD flags = EXTENDED_STARTUPINFO_PRESENT | CREATE_UNICODE_ENVIRONMENT |
                      CREATE_NO_WINDOW;
  const BOOL created = CreateProcessW(
      nullptr, commandLine.data(), nullptr, nullptr, TRUE, flags,
      environment.empty() ? nullptr : environment.data(),
      workingDirectory.empty() ? nullptr : workingDirectory.c_str(),
      &startup.StartupInfo, &processInfo);
  DeleteProcThreadAttributeList(attributesList);
  CloseHandle(stdinRead);
  CloseHandle(stdoutWrite);
  CloseHandle(stderrWrite);
  stdinRead = stdoutWrite = stderrWrite = nullptr;

  if (!created) {
    *error = windowsError("CreateProcess");
    cleanup();
    return false;
  }

  CloseHandle(processInfo.hThread);
  process = processInfo.hProcess;
  stdinWrite = stdinWriteLocal;
  stdinWriteLocal = nullptr;
  stdoutRead = stdoutReadLocal;
  stdoutReadLocal = nullptr;
  stderrRead = stderrReadLocal;
  stderrReadLocal = nullptr;

  job = CreateJobObjectW(nullptr, nullptr);
  if (job) {
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
    limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    SetInformationJobObject(job, JobObjectExtendedLimitInformation, &limits,
                            sizeof(limits));
    AssignProcessToJobObject(job, process);
  }

  cleanup();
  return true;
}

void ExecutionSession::Impl::terminate() {
  if (processGone) {
    return;
  }
  if (job) {
    TerminateJobObject(job, 1);
  }
  if (process) {
    TerminateProcess(process, 1);
  }
}

bool ExecutionSession::Impl::processExited(int32_t* code) {
  if (!process) {
    *code = -1;
    return true;
  }
  if (WaitForSingleObject(process, 0) != WAIT_OBJECT_0) {
    return false;
  }
  DWORD value = 1;
  GetExitCodeProcess(process, &value);
  *code = static_cast<int32_t>(value);
  processGone = true;
  return true;
}

void ExecutionSession::Impl::sleepSlice() {
  if (process) {
    WaitForSingleObject(process, 5);
  } else {
    Sleep(5);
  }
}

void ExecutionSession::Impl::readLoop(ExecutionOutputStream stream) {
  HANDLE handle = stream == ExecutionOutputStream::Stdout ? stdoutRead : stderrRead;
  char buffer[kReadBufferBytes];
  while (!stopping.load()) {
    DWORD read = 0;
    if (!ReadFile(handle, buffer, static_cast<DWORD>(sizeof(buffer)), &read,
                  nullptr) ||
        read == 0) {
      break;
    }
    pushOutput(stream, std::string(buffer, read));
  }
  readerFinished();
}

void ExecutionSession::Impl::closeHandles() {
  for (HANDLE* handle : {&stdinWrite, &stdoutRead, &stderrRead, &process, &job}) {
    if (*handle) {
      CloseHandle(*handle);
      *handle = nullptr;
    }
  }
}

#else

bool ExecutionSession::Impl::spawn(std::string* error) {
  std::vector<std::string> argumentStorage;
  std::string executable;

  if (options.shell) {
    if (!options.arguments.empty()) {
      *error = "arguments must be empty in shell mode";
      return false;
    }
    executable = "/bin/sh";
    argumentStorage = {"/bin/sh", "-c", options.command};
  } else {
    executable = resolveExecutable(options.command);
    if (executable.empty()) {
      *error = "executable not found: " + options.command;
      return false;
    }
    argumentStorage.push_back(executable);
    for (const std::string& argument : options.arguments) {
      argumentStorage.push_back(argument);
    }
  }

  if (!options.workingDirectory.empty()) {
    struct stat status{};
    if (stat(options.workingDirectory.c_str(), &status) != 0 ||
        !S_ISDIR(status.st_mode)) {
      *error = "working directory not found: " + options.workingDirectory;
      return false;
    }
  }

  std::vector<std::string> environmentStorage;
  if (!options.environment.empty()) {
    for (char** entry = environ; entry && *entry; ++entry) {
      environmentStorage.emplace_back(*entry);
    }
    for (const auto& [key, value] : options.environment) {
      if (key.empty() || key.find('=') != std::string::npos) {
        continue;
      }
      const std::string prefix = key + "=";
      bool replaced = false;
      for (std::string& entry : environmentStorage) {
        if (entry.compare(0, prefix.size(), prefix) == 0) {
          entry = prefix + value;
          replaced = true;
          break;
        }
      }
      if (!replaced) {
        environmentStorage.push_back(prefix + value);
      }
    }
  }

  std::vector<char*> argv;
  argv.reserve(argumentStorage.size() + 1);
  for (std::string& argument : argumentStorage) {
    argv.push_back(argument.data());
  }
  argv.push_back(nullptr);

  std::vector<char*> envp;
  envp.reserve(environmentStorage.size() + 1);
  for (std::string& entry : environmentStorage) {
    envp.push_back(entry.data());
  }
  if (!environmentStorage.empty()) {
    envp.push_back(nullptr);
  }

  int stdinPipe[2] = {-1, -1};
  int stdoutPipe[2] = {-1, -1};
  int stderrPipe[2] = {-1, -1};
  if (pipe(stdinPipe) != 0 || pipe(stdoutPipe) != 0 || pipe(stderrPipe) != 0) {
    *error = std::string("pipe failed: ") + std::strerror(errno);
    for (int descriptor : {stdinPipe[0], stdinPipe[1], stdoutPipe[0],
                           stdoutPipe[1], stderrPipe[0], stderrPipe[1]}) {
      if (descriptor >= 0) {
        close(descriptor);
      }
    }
    return false;
  }

  pid = fork();
  if (pid < 0) {
    *error = std::string("fork failed: ") + std::strerror(errno);
    for (int descriptor : {stdinPipe[0], stdinPipe[1], stdoutPipe[0],
                           stdoutPipe[1], stderrPipe[0], stderrPipe[1]}) {
      close(descriptor);
    }
    return false;
  }

  if (pid == 0) {
    setpgid(0, 0);
    if (!options.workingDirectory.empty() &&
        chdir(options.workingDirectory.c_str()) != 0) {
      _exit(127);
    }
    dup2(stdinPipe[0], 0);
    dup2(stdoutPipe[1], 1);
    dup2(stderrPipe[1], 2);
    close(stdinPipe[0]);
    close(stdinPipe[1]);
    close(stdoutPipe[0]);
    close(stdoutPipe[1]);
    close(stderrPipe[0]);
    close(stderrPipe[1]);
    if (envp.empty()) {
      execv(executable.c_str(), argv.data());
    } else {
      execve(executable.c_str(), argv.data(), envp.data());
    }
    _exit(127);
  }

  close(stdinPipe[0]);
  close(stdoutPipe[1]);
  close(stderrPipe[1]);
  stdinFd = stdinPipe[1];
  stdoutFd = stdoutPipe[0];
  stderrFd = stderrPipe[0];
  return true;
}

void ExecutionSession::Impl::terminate() {
  if (processGone || pid <= 0) {
    return;
  }
  if (kill(-pid, SIGKILL) != 0) {
    kill(pid, SIGKILL);
  }
}

bool ExecutionSession::Impl::processExited(int32_t* code) {
  int status = 0;
  const pid_t result = waitpid(pid, &status, WNOHANG);
  if (result == 0 || (result < 0 && errno == EINTR)) {
    return false;
  }
  if (result < 0) {
    *code = 127;
    return true;
  }
  if (WIFEXITED(status)) {
    *code = WEXITSTATUS(status);
  } else if (WIFSIGNALED(status)) {
    *code = 128 + WTERMSIG(status);
  } else {
    *code = 127;
  }
  processGone = true;
  return true;
}

void ExecutionSession::Impl::sleepSlice() {
  usleep(5000);
}

void ExecutionSession::Impl::readLoop(ExecutionOutputStream stream) {
  const int descriptor =
      stream == ExecutionOutputStream::Stdout ? stdoutFd : stderrFd;
  char buffer[kReadBufferBytes];
  while (!stopping.load()) {
    const ssize_t read = ::read(descriptor, buffer, sizeof(buffer));
    if (read < 0 && errno == EINTR) {
      continue;
    }
    if (read <= 0) {
      break;
    }
    pushOutput(stream, std::string(buffer, static_cast<size_t>(read)));
  }
  readerFinished();
}

void ExecutionSession::Impl::closeHandles() {
  for (int* descriptor : {&stdinFd, &stdoutFd, &stderrFd}) {
    if (*descriptor >= 0) {
      close(*descriptor);
      *descriptor = -1;
    }
  }
}

#endif

ExecutionSession::ExecutionSession() : impl_(std::make_unique<Impl>()) {}

ExecutionSession::~ExecutionSession() {
  {
    std::lock_guard<std::mutex> lock(impl_->stdinMutex);
    if (!impl_->stdinClosed) {
#ifdef _WIN32
      if (impl_->stdinWrite) {
        CloseHandle(impl_->stdinWrite);
        impl_->stdinWrite = nullptr;
      }
#else
      if (impl_->stdinFd >= 0) {
        close(impl_->stdinFd);
        impl_->stdinFd = -1;
      }
#endif
      impl_->stdinClosed = true;
    }
  }

  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->stopping = true;
    impl_->spaceAvailable.notify_all();
    impl_->changed.notify_all();
  }

  impl_->terminate();

  if (impl_->waiterThread.joinable()) {
    impl_->waiterThread.join();
  }
  if (impl_->stdoutThread.joinable()) {
    impl_->stdoutThread.join();
  }
  if (impl_->stderrThread.joinable()) {
    impl_->stderrThread.join();
  }
  impl_->closeHandles();
}

std::unique_ptr<ExecutionSession> ExecutionSession::start(
    const ExecutionOptions& options, std::string* errorMessage) {
  if (errorMessage) {
    errorMessage->clear();
  }
  auto session = std::unique_ptr<ExecutionSession>(new ExecutionSession());
  session->impl_->options = options;
  if (session->impl_->options.maxQueuedOutputBytes == 0) {
    session->impl_->options.maxQueuedOutputBytes = kDefaultQueueBytes;
  }
  if (session->impl_->options.command.empty()) {
    if (errorMessage) {
      *errorMessage = "command is empty";
    }
    return nullptr;
  }
  if (session->impl_->options.shell && !session->impl_->options.arguments.empty()) {
    if (errorMessage) {
      *errorMessage = "arguments must be empty in shell mode";
    }
    return nullptr;
  }

  std::string error;
  if (!session->impl_->spawn(&error)) {
    if (errorMessage) {
      *errorMessage = error;
    }
    return nullptr;
  }

  Impl* impl = session->impl_.get();
  try {
    impl->stdoutThread =
        std::thread([impl]() { impl->readLoop(ExecutionOutputStream::Stdout); });
    impl->stderrThread =
        std::thread([impl]() { impl->readLoop(ExecutionOutputStream::Stderr); });
    impl->waiterThread = std::thread([impl]() { impl->waitLoop(); });
  } catch (...) {
    {
      std::lock_guard<std::mutex> lock(impl->mutex);
      impl->stopping = true;
      impl->spaceAvailable.notify_all();
    }
    impl->terminate();
    if (impl->stdoutThread.joinable()) {
      impl->stdoutThread.join();
    }
    if (impl->stderrThread.joinable()) {
      impl->stderrThread.join();
    }
    impl->closeHandles();
    if (errorMessage) {
      *errorMessage = "failed to start execution worker threads";
    }
    return nullptr;
  }
  return session;
}

void ExecutionSession::Impl::pushOutput(ExecutionOutputStream stream,
                                        std::string data) {
  std::unique_lock<std::mutex> lock(mutex);
  while (!stopping && !events.empty() &&
         queuedBytes >= options.maxQueuedOutputBytes) {
    spaceAvailable.wait(lock);
  }
  if (stopping) {
    return;
  }
  ExecutionEvent event;
  event.kind = ExecutionEvent::Kind::Output;
  event.stream = stream;
  event.sequence = nextSequence++;
  queuedBytes += data.size();
  event.data = std::move(data);
  events.push_back(std::move(event));
  changed.notify_all();
}

void ExecutionSession::Impl::readerFinished() {
  std::lock_guard<std::mutex> lock(mutex);
  ++readersFinished;
  if (readersFinished >= 2) {
    readersDone = true;
  }
  changed.notify_all();
}

void ExecutionSession::Impl::finish(int32_t code, ExecutionState finalState) {
  std::lock_guard<std::mutex> lock(mutex);
  if (state != ExecutionState::Running) {
    return;
  }
  state = finalState;
  exitCodeValue = code;
  exitRecorded = true;
  changed.notify_all();
}

void ExecutionSession::Impl::waitLoop() {
  const auto started = std::chrono::steady_clock::now();
  bool sawTimeout = false;
  int32_t code = -1;

  for (;;) {
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (options.timeoutMs > 0 && !timedOut) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started);
        if (elapsed.count() >= static_cast<long long>(options.timeoutMs)) {
          timedOut = true;
          sawTimeout = true;
        }
      }
      if (timedOut) {
        sawTimeout = true;
      }
    }

    const bool cancel = [&]() {
      std::lock_guard<std::mutex> lock(mutex);
      return cancelRequested;
    }();
    if (cancel || sawTimeout) {
      terminate();
    }

    if (processExited(&code)) {
      break;
    }
    sleepSlice();
  }

  ExecutionState finalState = ExecutionState::Exited;
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (timedOut) {
      finalState = ExecutionState::TimedOut;
    } else if (cancelRequested) {
      finalState = ExecutionState::Cancelled;
    }
  }
  finish(code, finalState);
}

bool ExecutionSession::write(std::string_view data) {
  std::lock_guard<std::mutex> stdinLock(impl_->stdinMutex);
  if (impl_->stdinClosed) {
    return false;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->state != ExecutionState::Running) {
      return false;
    }
  }

#ifdef _WIN32
  if (!impl_->stdinWrite) {
    return false;
  }
  size_t offset = 0;
  while (offset < data.size()) {
    const size_t remaining = data.size() - offset;
    const DWORD chunk = static_cast<DWORD>(
        remaining > 0x7FFFFFFFu ? 0x7FFFFFFFu : remaining);
    DWORD written = 0;
    if (!WriteFile(impl_->stdinWrite, data.data() + offset, chunk, &written,
                   nullptr) ||
        written == 0) {
      impl_->lastError = windowsError("WriteFile(stdin)");
      return false;
    }
    offset += written;
  }
#else
  if (impl_->stdinFd < 0) {
    return false;
  }
  size_t offset = 0;
  while (offset < data.size()) {
    const ssize_t written =
        ::write(impl_->stdinFd, data.data() + offset, data.size() - offset);
    if (written < 0 && errno == EINTR) {
      continue;
    }
    if (written <= 0) {
      impl_->lastError = std::string("write(stdin) failed: ") + std::strerror(errno);
      return false;
    }
    offset += static_cast<size_t>(written);
  }
#endif
  return true;
}

void ExecutionSession::closeStdin() {
  std::lock_guard<std::mutex> lock(impl_->stdinMutex);
  if (impl_->stdinClosed) {
    return;
  }
#ifdef _WIN32
  if (impl_->stdinWrite) {
    CloseHandle(impl_->stdinWrite);
    impl_->stdinWrite = nullptr;
  }
#else
  if (impl_->stdinFd >= 0) {
    close(impl_->stdinFd);
    impl_->stdinFd = -1;
  }
#endif
  impl_->stdinClosed = true;
}

void ExecutionSession::cancel() {
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->state != ExecutionState::Running) {
      return;
    }
    impl_->cancelRequested = true;
  }
  impl_->terminate();
}

bool ExecutionSession::wait(uint32_t timeoutMs) {
  std::unique_lock<std::mutex> lock(impl_->mutex);
  if (impl_->state != ExecutionState::Running) {
    return true;
  }
  if (timeoutMs == 0) {
    return false;
  }
  impl_->changed.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                          [this]() {
                            return impl_->state != ExecutionState::Running;
                          });
  return impl_->state != ExecutionState::Running;
}

bool ExecutionSession::poll(ExecutionEvent* out) {
  if (!out) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!impl_->events.empty()) {
    *out = std::move(impl_->events.front());
    impl_->events.pop_front();
    impl_->queuedBytes -= out->data.size();
    impl_->spaceAvailable.notify_one();
    return true;
  }
  if (impl_->exitRecorded && impl_->readersDone && !impl_->exitEmitted) {
    out->kind = ExecutionEvent::Kind::Exit;
    out->sequence = impl_->nextSequence++;
    out->exitCode = impl_->exitCodeValue;
    out->cancelled = impl_->state == ExecutionState::Cancelled;
    out->timedOut = impl_->state == ExecutionState::TimedOut;
    out->data.clear();
    impl_->exitEmitted = true;
    return true;
  }
  return false;
}

bool ExecutionSession::running() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->state == ExecutionState::Running;
}

ExecutionState ExecutionSession::state() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->state;
}

int32_t ExecutionSession::exitCode() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->exitCodeValue;
}

const std::string& ExecutionSession::lastError() const {
  return impl_->lastError;
}

}  // namespace codecore
