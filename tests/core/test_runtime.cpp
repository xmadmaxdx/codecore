#include "core/runtime/runtime.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
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
const char* kShellName = "cmd.exe";
#else
const char* kShellName = "sh";
#endif

std::string drainUntilExit(codecore::ExecutionSession& session, int32_t* exitCode,
                           int timeoutMs = 20000) {
  std::string output;
  codecore::ExecutionEvent event;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
  *exitCode = -1;
  while (std::chrono::steady_clock::now() < deadline) {
    while (session.poll(&event)) {
      if (event.kind == codecore::ExecutionEvent::Kind::Output) {
        output += event.data;
      } else {
        *exitCode = event.exitCode;
      }
    }
    if (!session.running()) {
      break;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  while (session.poll(&event)) {
    if (event.kind == codecore::ExecutionEvent::Kind::Output) {
      output += event.data;
    } else {
      *exitCode = event.exitCode;
    }
  }
  return output;
}

bool contains(const std::string& haystack, const std::string& needle) {
  return haystack.find(needle) != std::string::npos;
}

codecore::RuntimeDescriptor shellRuntime(std::vector<std::string> runArguments) {
  codecore::RuntimeDescriptor descriptor;
  descriptor.id = "test-shell";
  descriptor.displayName = "Test shell";
  descriptor.languages = {"testshell"};
  descriptor.executable = codecore::RuntimeRegistry::resolveExecutable(kShellName);
  descriptor.available = !descriptor.executable.empty();
  descriptor.runArguments = std::move(runArguments);
  return descriptor;
}

void testPaths() {
  CHECK(!codecore::RuntimeRegistry::resolveExecutable(kShellName).empty());
  CHECK(codecore::RuntimeRegistry::resolveExecutable("definitely-not-a-real-binary-xyz").empty());
}

void testDiscovery() {
  auto registry = codecore::RuntimeRegistry::discover();
  CHECK(registry.count() >= 20);
  CHECK(registry.byId("python") != nullptr);
  CHECK(registry.byId("g++") != nullptr);
  CHECK(registry.forLanguage("python") != nullptr);
  CHECK(registry.forLanguage("rust") != nullptr);
  CHECK(registry.forLanguage("no-such-language") == nullptr);

  const codecore::RuntimeDescriptor* cpp = registry.byId("g++");
  CHECK(cpp != nullptr);
  if (cpp) {
    CHECK(!cpp->compileArguments.empty());
    CHECK(cpp->runCompiledArtifact);
  }
}

void testPlanErrors() {
  auto registry = codecore::RuntimeRegistry::discover();
  codecore::RuntimeRequest request;
  codecore::RuntimePlan plan;
  std::string error;

  CHECK(registry.plan(request, &plan, &error) == codecore::Status::InvalidArgument);
  CHECK(!error.empty());

  request.language = "no-such-language";
  request.sourcePath = "main.txt";
  error.clear();
  CHECK(registry.plan(request, &plan, &error) == codecore::Status::InvalidArgument);
  CHECK(contains(error, "no runtime"));

  request.language = "python";
  request.sourcePath = "";
  CHECK(registry.plan(request, &plan, &error) == codecore::Status::InvalidArgument);
}

void testShellRuntimeRuns() {
  auto registry = codecore::RuntimeRegistry::discover();
#ifdef _WIN32
  registry.add(shellRuntime({"/c", "echo codecore-runtime-ok"}));
#else
  registry.add(shellRuntime({"-c", "echo codecore-runtime-ok"}));
#endif

  codecore::RuntimeRequest request;
  request.language = "testshell";
  request.sourcePath = "unused.txt";

  codecore::RuntimePlan plan;
  std::string error;
  CHECK(registry.plan(request, &plan, &error) == codecore::Status::Ok);
  CHECK(!plan.needsCompile);
  CHECK(!plan.run.command.empty());

  std::string runError;
  auto session = registry.run(request, &runError);
  CHECK(session != nullptr);
  if (session) {
    int32_t exitCode = -1;
    const std::string output = drainUntilExit(*session, &exitCode);
    CHECK(contains(output, "codecore-runtime-ok"));
    CHECK(exitCode == 0);
  } else {
    std::printf("  run error: %s\n", runError.c_str());
  }
}

void testCompileStep() {
  auto registry = codecore::RuntimeRegistry::discover();
#ifdef _WIN32
  registry.add(shellRuntime({"/c", "echo program-ran"}));
  codecore::RuntimeDescriptor* descriptor = registry.mutableById("test-shell");
  CHECK(descriptor != nullptr);
  descriptor->compileExecutable = descriptor->executable;
  descriptor->compileArguments = {"/c", "echo compiling"};
  descriptor->runCompiledArtifact = false;
#else
  registry.add(shellRuntime({"-c", "echo program-ran"}));
  codecore::RuntimeDescriptor* descriptor = registry.mutableById("test-shell");
  CHECK(descriptor != nullptr);
  descriptor->compileExecutable = descriptor->executable;
  descriptor->compileArguments = {"-c", "echo compiling"};
  descriptor->runCompiledArtifact = false;
#endif

  codecore::RuntimeRequest request;
  request.language = "testshell";
  request.sourcePath = "unused.txt";

  codecore::RuntimePlan plan;
  std::string error;
  CHECK(registry.plan(request, &plan, &error) == codecore::Status::Ok);
  CHECK(plan.needsCompile);

  std::string runError;
  auto session = registry.run(request, &runError);
  CHECK(session != nullptr);
  if (session) {
    int32_t exitCode = -1;
    const std::string output = drainUntilExit(*session, &exitCode);
    CHECK(contains(output, "program-ran"));
    CHECK(exitCode == 0);
  }

  request.compile = false;
  CHECK(registry.plan(request, &plan, &error) == codecore::Status::Ok);
  CHECK(!plan.needsCompile);
}

void testCompileFailure() {
  auto registry = codecore::RuntimeRegistry::discover();
#ifdef _WIN32
  registry.add(shellRuntime({"/c", "echo should-not-run"}));
  codecore::RuntimeDescriptor* descriptor = registry.mutableById("test-shell");
  CHECK(descriptor != nullptr);
  descriptor->compileExecutable = descriptor->executable;
  descriptor->compileArguments = {"/c", "echo broken-compile & exit /b 3"};
#else
  registry.add(shellRuntime({"-c", "echo should-not-run"}));
  codecore::RuntimeDescriptor* descriptor = registry.mutableById("test-shell");
  CHECK(descriptor != nullptr);
  descriptor->compileExecutable = descriptor->executable;
  descriptor->compileArguments = {"-c", "echo broken-compile; exit 3"};
#endif

  codecore::RuntimeRequest request;
  request.language = "testshell";
  request.sourcePath = "unused.txt";

  std::string error;
  auto session = registry.run(request, &error);
  CHECK(session == nullptr);
  CHECK(contains(error, "compile failed"));
  CHECK(contains(error, "broken-compile"));
}

void testRealPythonIfInstalled() {
  auto registry = codecore::RuntimeRegistry::discover();
  const codecore::RuntimeDescriptor* python = registry.byId("python");
  if (!python || !python->available) {
    std::printf("  (python not installed; skipping real execution test)\n");
    return;
  }

  registry.probeVersions(3000);
  python = registry.byId("python");
  CHECK(python != nullptr);
  if (python) {
    CHECK(!python->version.empty());
  }

  const auto directory =
      std::filesystem::temp_directory_path() / "codecore-runtime-test";
  std::filesystem::create_directories(directory);
  const auto script = directory / "hello.py";
  {
    std::ofstream out(script);
    out << "print('hello-from-python')\n";
  }

  codecore::RuntimeRequest request;
  request.language = "python";
  request.sourcePath = script.string();

  std::string error;
  auto session = registry.run(request, &error);
  CHECK(session != nullptr);
  if (session) {
    int32_t exitCode = -1;
    const std::string output = drainUntilExit(*session, &exitCode);
    CHECK(contains(output, "hello-from-python"));
    CHECK(exitCode == 0);
  } else {
    std::printf("  python run error: %s\n", error.c_str());
  }
  std::error_code ignore;
  std::filesystem::remove_all(directory, ignore);
}

}  // namespace

int main() {
  testPaths();
  testDiscovery();
  testPlanErrors();
  testShellRuntimeRuns();
  testCompileStep();
  testCompileFailure();
  testRealPythonIfInstalled();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
