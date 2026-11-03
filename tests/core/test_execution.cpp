#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "codecore/codecore.h"
#include "core/exec/execution.h"

namespace {

int checks = 0;
int failures = 0;
std::string helperPath;

#define CHECK(condition)                                                       \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(condition)) {                                                        \
      ++failures;                                                              \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);         \
    }                                                                          \
  } while (0)

using codecore::ExecutionEvent;
using codecore::ExecutionOptions;
using codecore::ExecutionSession;
using codecore::ExecutionState;

struct Capture {
  std::string out;
  std::string err;
  std::vector<uint64_t> sequences;
  bool exited = false;
  int32_t code = -1;
  bool cancelled = false;
  bool timedOut = false;
};

ExecutionOptions baseOptions(const std::string& mode) {
  ExecutionOptions options;
  options.command = helperPath;
  options.arguments = {mode};
  options.timeoutMs = 5000;
  return options;
}

bool drain(ExecutionSession& session, Capture& capture, int timeoutMs = 5000) {
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  while (std::chrono::steady_clock::now() < deadline) {
    ExecutionEvent event;
    while (session.poll(&event)) {
      capture.sequences.push_back(event.sequence);
      if (event.kind == ExecutionEvent::Kind::Output) {
        if (event.stream == codecore::ExecutionOutputStream::Stdout) {
          capture.out += event.data;
        } else {
          capture.err += event.data;
        }
      } else {
        capture.exited = true;
        capture.code = event.exitCode;
        capture.cancelled = event.cancelled;
        capture.timedOut = event.timedOut;
        return true;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
  return false;
}

void checkSequences(const Capture& capture) {
  for (size_t index = 1; index < capture.sequences.size(); ++index) {
    CHECK(capture.sequences[index - 1] < capture.sequences[index]);
  }
}

void testEcho() {
  std::string error;
  auto session = ExecutionSession::start(baseOptions("echo"), &error);
  CHECK(session != nullptr);
  if (!session) {
    std::printf("start error: %s\n", error.c_str());
    return;
  }

  Capture capture;
  CHECK(drain(*session, capture));
  CHECK(capture.exited);
  CHECK(capture.code == 0);
  CHECK(capture.out == "out:hello\n");
  CHECK(capture.err == "err:hello\n");
  CHECK(!capture.cancelled);
  CHECK(!capture.timedOut);
  CHECK(!session->running());
  CHECK(session->state() == ExecutionState::Exited);
  CHECK(session->exitCode() == 0);
  CHECK(session->wait(0));
  CHECK(session->wait(100));
  checkSequences(capture);

  ExecutionEvent extra;
  CHECK(!session->poll(&extra));
}

void testExitCode() {
  std::string error;
  auto session = ExecutionSession::start(baseOptions("exit"), &error);
  CHECK(session != nullptr);
  if (!session) {
    return;
  }
  const std::string argument = "7";
  // Rebuild with the code argument; baseOptions only carries the mode.
  ExecutionOptions options = baseOptions("exit");
  options.arguments.push_back(argument);
  auto coded = ExecutionSession::start(options, &error);
  CHECK(coded != nullptr);
  if (coded) {
    Capture capture;
    CHECK(drain(*coded, capture));
    CHECK(capture.code == 7);
    CHECK(coded->exitCode() == 7);
  }
  session->cancel();
  (void)session->wait(1000);
}

void testStdin() {
  std::string error;
  auto session = ExecutionSession::start(baseOptions("stdin"), &error);
  CHECK(session != nullptr);
  if (!session) {
    return;
  }

  CHECK(session->write("hello\n"));
  CHECK(session->write("world\n"));
  session->closeStdin();

  Capture capture;
  CHECK(drain(*session, capture));
  CHECK(capture.code == 0);
  CHECK(capture.out.find("got:hello\n") != std::string::npos);
  CHECK(capture.out.find("got:world\n") != std::string::npos);
}

std::string directoryOf(const std::string& file) {
  const size_t separator = file.find_last_of("/\\");
  return separator == std::string::npos ? std::string(".")
                                        : file.substr(0, separator);
}

void testWorkingDirectory() {
  const std::string directory = directoryOf(helperPath);
  ExecutionOptions options = baseOptions("cwd");
  options.workingDirectory = directory;
  std::string error;
  auto session = ExecutionSession::start(options, &error);
  CHECK(session != nullptr);
  if (session) {
    Capture capture;
    CHECK(drain(*session, capture));
    CHECK(capture.code == 0);
    CHECK(capture.out.find("native") != std::string::npos);
  }
}

void testEnvironment() {
  ExecutionOptions options = baseOptions("env");
  options.arguments.push_back("CODECORE_EXEC_TEST");
  options.environment = {{"CODECORE_EXEC_TEST", "value-42"}};
  std::string error;
  auto session = ExecutionSession::start(options, &error);
  CHECK(session != nullptr);
  if (session) {
    Capture capture;
    CHECK(drain(*session, capture));
    CHECK(capture.code == 0);
    CHECK(capture.out == "CODECORE_EXEC_TEST=value-42\n");
  }
}

void testCancel() {
  ExecutionOptions options = baseOptions("sleep");
  options.arguments.push_back("10000");
  options.timeoutMs = 0;
  std::string error;
  auto session = ExecutionSession::start(options, &error);
  CHECK(session != nullptr);
  if (!session) {
    return;
  }
  const auto started = std::chrono::steady_clock::now();
  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  session->cancel();
  CHECK(session->wait(3000));
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - started);
  CHECK(elapsed.count() < 3000);

  Capture capture;
  CHECK(drain(*session, capture));
  CHECK(capture.exited);
  CHECK(capture.cancelled);
  CHECK(session->state() == ExecutionState::Cancelled);
}

void testTimeout() {
  ExecutionOptions options = baseOptions("sleep");
  options.arguments.push_back("10000");
  options.timeoutMs = 120;
  std::string error;
  auto session = ExecutionSession::start(options, &error);
  CHECK(session != nullptr);
  if (!session) {
    return;
  }
  const auto started = std::chrono::steady_clock::now();
  Capture capture;
  CHECK(drain(*session, capture, 4000));
  const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - started);
  CHECK(capture.exited);
  CHECK(capture.timedOut);
  CHECK(session->state() == ExecutionState::TimedOut);
  CHECK(elapsed.count() < 3000);
}

void testShell() {
  ExecutionOptions options;
  options.command = "echo shell-ok";
  options.shell = true;
  options.timeoutMs = 5000;
  std::string error;
  auto session = ExecutionSession::start(options, &error);
  CHECK(session != nullptr);
  if (!session) {
    return;
  }
  Capture capture;
  CHECK(drain(*session, capture));
  CHECK(capture.code == 0);
  CHECK(capture.out.find("shell-ok") != std::string::npos);
}

void testInvalidCommand() {
  ExecutionOptions options;
  options.command = "codecore-command-does-not-exist-xyz";
  std::string error;
  auto session = ExecutionSession::start(options, &error);
  CHECK(session == nullptr);
  CHECK(!error.empty());

  ExecutionOptions shellOptions;
  shellOptions.command = "echo hi";
  shellOptions.shell = true;
  shellOptions.arguments = {"not-allowed"};
  auto invalid = ExecutionSession::start(shellOptions, &error);
  CHECK(invalid == nullptr);
}

void testConcurrentSessions() {
  std::vector<std::unique_ptr<ExecutionSession>> sessions;
  for (int index = 0; index < 3; ++index) {
    std::string error;
    auto session = ExecutionSession::start(baseOptions("echo"), &error);
    CHECK(session != nullptr);
    if (session) {
      sessions.push_back(std::move(session));
    }
  }
  for (auto& session : sessions) {
    Capture capture;
    CHECK(drain(*session, capture));
    CHECK(capture.code == 0);
    CHECK(capture.out == "out:hello\n");
  }
}

void testCApi() {
  CHECK(cc_execution_supported() == 1);

  const char* args[] = {"echo"};
  cc_execution_options options{};
  options.struct_size = sizeof(options);
  options.command = helperPath.c_str();
  options.args = args;
  options.arg_count = 1;
  options.timeout_ms = 5000;

  char* error = nullptr;
  cc_execution* execution = cc_execution_start(&options, &error);
  CHECK(execution != nullptr);
  if (!execution) {
    if (error) {
      std::printf("C ABI start error: %s\n", error);
      cc_string_free(error);
    }
    return;
  }

  std::string output;
  bool exited = false;
  int32_t code = -1;
  for (int attempt = 0; attempt < 5000 && !exited; ++attempt) {
    cc_execution_event event{};
    while (cc_execution_poll(execution, &event)) {
      if (event.kind == CC_EXECUTION_OUTPUT) {
        if (event.stream == CC_EXECUTION_STDOUT) {
          output.append(event.data, event.length);
        }
        cc_string_free(event.data);
      } else {
        exited = true;
        code = event.exit_code;
        break;
      }
    }
    if (!exited) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
  }
  CHECK(exited);
  CHECK(code == 0);
  CHECK(output == "out:hello\n");
  CHECK(cc_execution_is_running(execution) == 0);
  CHECK(cc_execution_exit_code(execution) == 0);
  CHECK(cc_execution_wait(execution, 100) == 1);
  CHECK(cc_execution_write(execution, "x", 1) == CC_STATUS_NOT_RUNNING);
  cc_execution_destroy(execution);

  cc_execution* missing = cc_execution_start(nullptr, &error);
  CHECK(missing == nullptr);
  CHECK(error != nullptr);
  if (error) {
    cc_string_free(error);
  }
}

}  // namespace

int main(int argc, char** argv) {
  (void)argc;
  helperPath = directoryOf(argv[0] ? argv[0] : ".") + "/codecore_exec_helper";
#ifdef _WIN32
  helperPath += ".exe";
#endif

  std::ifstream helper(helperPath.c_str());
  if (!helper.good()) {
    std::printf("missing helper: %s\n", helperPath.c_str());
    return 1;
  }

  testEcho();
  testExitCode();
  testStdin();
  testWorkingDirectory();
  testEnvironment();
  testCancel();
  testTimeout();
  testShell();
  testInvalidCommand();
  testConcurrentSessions();
  testCApi();

  std::printf("Execution checks: %d, failures: %d\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
