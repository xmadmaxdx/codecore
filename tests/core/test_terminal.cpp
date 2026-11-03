#include "core/term/terminal.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <thread>

namespace {

int checks = 0;
int failures = 0;

void check(bool condition, const char* expression, int line) {
  ++checks;
  if (!condition) {
    ++failures;
    std::printf("FAIL line %d: %s\n", line, expression);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

#ifdef _WIN32
const char* kShell = "cmd.exe";
#else
const char* kShell = "/bin/sh";
#endif

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

void testInteractiveShell() {
  codecore::TerminalOptions options;
  options.command = kShell;
#ifdef _WIN32
  options.arguments = {"/Q"};
#endif
  options.columns = 100;
  options.rows = 30;

  std::string error;
  auto session = codecore::TerminalSession::start(options, &error);
  CHECK(session != nullptr);
  if (!session) {
    std::printf("  start failed: %s\n", error.c_str());
    return;
  }

  CHECK(session->columns() == 100);
  CHECK(session->rows() == 30);
  CHECK(session->resize(120, 40) == session->usesPty());
  CHECK(session->columns() == 120);
#ifdef _WIN32
  CHECK(!session->usesPty());
#else
  CHECK(session->usesPty());
#endif

  const std::string marker = "codecore-terminal-ok";
  std::string output;
  codecore::TerminalEvent event;

  const auto readFor = [&](std::chrono::milliseconds window) {
    const auto until = std::chrono::steady_clock::now() + window;
    while (std::chrono::steady_clock::now() < until) {
      while (session->poll(&event)) {
        if (event.kind == codecore::TerminalEvent::Kind::Output) {
          output += event.data;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
  };

  // Terminals echo input asynchronously; give the shell a moment to start.
  readFor(std::chrono::milliseconds(300));

#ifdef _WIN32
  CHECK(session->write("echo " + marker + "\r\n"));
#else
  CHECK(session->write("echo " + marker + "\n"));
#endif

  bool seen = false;
  for (int attempt = 0; attempt < 200 && !seen; ++attempt) {
    readFor(std::chrono::milliseconds(20));
    seen = contains(output, marker);
  }

#ifdef _WIN32
  CHECK(session->write("exit\r\n"));
#else
  CHECK(session->write("exit\n"));
#endif

  bool exited = false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(20);
  while (std::chrono::steady_clock::now() < deadline && !exited) {
    while (session->poll(&event)) {
      if (event.kind == codecore::TerminalEvent::Kind::Output) {
        output += event.data;
      } else {
        exited = true;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
  }

  CHECK(exited);
  CHECK(contains(output, marker));
  CHECK(session->exitCode() == 0);
  CHECK(session->wait(0));

  session->close();
}

void testCloseWhileRunning() {
  codecore::TerminalOptions options;
  options.command = kShell;
#ifdef _WIN32
  options.arguments = {"/Q"};
#endif

  std::string error;
  auto session = codecore::TerminalSession::start(options, &error);
  CHECK(session != nullptr);
  if (!session) {
    return;
  }
  CHECK(session->running());
  session->close();
  CHECK(!session->running());
}

void testInvalidStart() {
  codecore::TerminalOptions options;
  options.command = "/definitely/not/a/real/shell/binary";
  std::string error;
  auto session = codecore::TerminalSession::start(options, &error);
#ifdef _WIN32
  // CreateProcess fails synchronously for a missing executable.
  CHECK(session == nullptr);
  CHECK(!error.empty());
#else
  // fork() succeeds and the child exits with 127 after a failed exec.
  CHECK(session != nullptr);
  if (session) {
    CHECK(session->wait(5000));
    CHECK(session->exitCode() == 127);
    session->close();
  }
#endif
}

}  // namespace

int main() {
  testInteractiveShell();
  testCloseWhileRunning();
  testInvalidStart();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
