#include "core/runtime/runtime.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <chrono>
#include <thread>

namespace codecore {
namespace {

using Paths = std::vector<std::string>;

std::string replaceAll(std::string text, const std::string& needle,
                       const std::string& replacement) {
  if (needle.empty()) {
    return text;
  }
  size_t position = 0;
  while ((position = text.find(needle, position)) != std::string::npos) {
    text.replace(position, needle.size(), replacement);
    position += replacement.size();
  }
  return text;
}

std::vector<std::string> splitList(const char* value, char separator) {
  std::vector<std::string> parts;
  if (!value) {
    return parts;
  }
  std::string current;
  for (const char* cursor = value; *cursor; ++cursor) {
    if (*cursor == separator) {
      if (!current.empty()) {
        parts.push_back(current);
        current.clear();
      }
      continue;
    }
    current.push_back(*cursor);
  }
  if (!current.empty()) {
    parts.push_back(current);
  }
  return parts;
}

#ifdef _WIN32
constexpr char kPathSeparator = ';';
constexpr const char* kExecutableSuffix = ".exe";
#else
constexpr char kPathSeparator = ':';
constexpr const char* kExecutableSuffix = "";
#endif

RuntimeDescriptor makeDescriptor(std::string id, std::string displayName,
                                 std::vector<std::string> languages,
                                 std::vector<std::string> candidateNames,
                                 std::vector<std::string> runArguments,
                                 std::string outputExtension = "") {
  RuntimeDescriptor descriptor;
  descriptor.id = std::move(id);
  descriptor.displayName = std::move(displayName);
  descriptor.languages = std::move(languages);
  descriptor.runArguments = std::move(runArguments);
  descriptor.outputExtension = std::move(outputExtension);
  for (const std::string& candidate : candidateNames) {
    const std::string resolved = RuntimeRegistry::resolveExecutable(candidate);
    if (!resolved.empty()) {
      descriptor.executable = resolved;
      descriptor.available = true;
      break;
    }
  }
  return descriptor;
}

RuntimeDescriptor makeRuntime(std::string id, std::string displayName,
                              std::vector<std::string> languages,
                              std::vector<std::string> candidateNames,
                              std::vector<std::string> runArguments,
                              std::vector<std::string> compileCandidates,
                              std::vector<std::string> compileArguments,
                              bool runCompiledArtifact, std::string outputExtension) {
  RuntimeDescriptor descriptor =
      makeDescriptor(std::move(id), std::move(displayName), std::move(languages),
                     std::move(candidateNames), std::move(runArguments),
                     std::move(outputExtension));
  if (!compileCandidates.empty()) {
    for (const std::string& candidate : compileCandidates) {
      const std::string resolved = RuntimeRegistry::resolveExecutable(candidate);
      if (!resolved.empty()) {
        descriptor.compileExecutable = resolved;
        descriptor.compileArguments = std::move(compileArguments);
        descriptor.runCompiledArtifact = runCompiledArtifact;
        break;
      }
    }
  }
  return descriptor;
}

std::vector<RuntimeDescriptor> builtinRuntimes() {
  std::vector<RuntimeDescriptor> runtimes;

  runtimes.push_back(makeDescriptor("python", "Python", {"python", "py", "python3"},
                                    {"python3", "python"}, {"{source}"}));
  runtimes.push_back(makeDescriptor("node", "Node.js", {"javascript", "js", "jsx", "mjs", "cjs"},
                                    {"node"}, {"{source}"}));
  runtimes.push_back(makeDescriptor("deno", "Deno", {"typescript", "ts", "tsx"}, {"deno"},
                                    {"run", "--allow-all", "{source}"}));
  runtimes.push_back(makeDescriptor("ruby", "Ruby", {"ruby", "rb"}, {"ruby"}, {"{source}"}));
  runtimes.push_back(makeDescriptor("php", "PHP", {"php"}, {"php"}, {"{source}"}));
  runtimes.push_back(makeDescriptor("perl", "Perl", {"perl", "pl"}, {"perl"}, {"{source}"}));
  runtimes.push_back(makeDescriptor("lua", "Lua", {"lua"}, {"lua"}, {"{source}"}));
  runtimes.push_back(makeDescriptor("rscript", "R", {"r"}, {"Rscript"}, {"{source}"}));
  runtimes.push_back(makeDescriptor("julia", "Julia", {"julia"}, {"julia"}, {"{source}"}));
  runtimes.push_back(makeDescriptor("bash", "Bash", {"bash", "sh", "zsh"}, {"bash", "sh"},
                                    {"{source}"}));
  runtimes.push_back(makeDescriptor("powershell", "PowerShell", {"powershell", "ps1"},
                                    {"pwsh", "powershell"},
                                    {"-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
                                     "{source}"}));
  runtimes.push_back(makeDescriptor("elixir", "Elixir", {"elixir"}, {"elixir"}, {"{source}"}));
  runtimes.push_back(makeDescriptor("runghc", "Haskell", {"haskell", "hs"}, {"runghc"},
                                    {"{source}"}));
  runtimes.push_back(makeDescriptor("dart", "Dart", {"dart"}, {"dart"}, {"run", "{source}"}));
  runtimes.push_back(makeDescriptor("go", "Go", {"go", "golang"}, {"go"},
                                    {"run", "{source}"}));
  runtimes.push_back(makeRuntime("java", "Java", {"java"}, {"java"}, {"{source}"}, {"javac"},
                                 {"-d", "{sourceDir}", "{source}"}, false, ".class"));
  runtimes.push_back(makeRuntime("kotlin", "Kotlin", {"kotlin", "kt"}, {"kotlin"},
                                 {"-classpath", "{output}"}, {"kotlinc"},
                                 {"-include-runtime", "-d", "{output}", "{source}"}, true,
                                 ".jar"));
  runtimes.push_back(makeRuntime("rust", "Rust", {"rust", "rs"}, {"rustc"}, {"{output}"},
                                 {"rustc"}, {"-O", "-o", "{output}", "{source}"}, true,
                                 kExecutableSuffix));
  runtimes.push_back(makeRuntime("gcc", "GCC", {"c"}, {"gcc", "clang", "cc"}, {"{output}"},
                                 {"gcc", "clang", "cc"}, {"{source}", "-o", "{output}"}, true,
                                 kExecutableSuffix));
  runtimes.push_back(makeRuntime("g++", "G++", {"cpp", "c++", "cc"},
                                 {"g++", "clang++", "c++"}, {"{output}"},
                                 {"g++", "clang++", "c++"},
                                 {"-std=c++17", "{source}", "-o", "{output}"}, true,
                                 kExecutableSuffix));
  runtimes.push_back(makeRuntime("gfortran", "Fortran", {"fortran"}, {"gfortran"}, {"{output}"},
                                 {"gfortran"}, {"{source}", "-o", "{output}"}, true,
                                 kExecutableSuffix));
  runtimes.push_back(makeDescriptor("dotnet", "C#", {"csharp", "cs"}, {"dotnet"},
                                    {"run", "--project", "{sourceDir}"}));
  return runtimes;
}

std::string firstLine(const std::string& text) {
  const size_t newline = text.find('\n');
  std::string line = newline == std::string::npos ? text : text.substr(0, newline);
  while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) {
    line.pop_back();
  }
  size_t start = 0;
  while (start < line.size() && (line[start] == ' ' || line[start] == '\t')) {
    ++start;
  }
  return line.substr(start);
}

std::vector<std::string> applyPlaceholders(const std::vector<std::string>& arguments,
                                           const std::string& source,
                                           const std::string& output,
                                           const std::string& sourceDir) {
  std::vector<std::string> result;
  result.reserve(arguments.size());
  for (const std::string& argument : arguments) {
    std::string value = replaceAll(argument, "{source}", source);
    value = replaceAll(value, "{output}", output);
    value = replaceAll(value, "{sourceDir}", sourceDir);
    result.push_back(std::move(value));
  }
  return result;
}

}  // namespace

RuntimeRegistry::RuntimeRegistry() = default;

std::string RuntimeRegistry::resolveExecutable(const std::string& name) {
  namespace fs = std::filesystem;
  if (name.empty()) {
    return {};
  }

  std::error_code error;
  const fs::path direct(name);
  if (direct.has_parent_path()) {
    if (fs::exists(direct, error) && !fs::is_directory(direct, error)) {
      return fs::absolute(direct, error).string();
    }
    return {};
  }

  std::vector<std::string> names{name};
#ifdef _WIN32
  const char* pathExtValue = std::getenv("PATHEXT");
  const std::vector<std::string> extensions =
      pathExtValue ? splitList(pathExtValue, ';')
                   : std::vector<std::string>{".EXE", ".CMD", ".BAT", ".COM"};
  for (const std::string& extension : extensions) {
    names.push_back(name + extension);
  }
#endif

  for (const std::string& directory :
       splitList(std::getenv("PATH"), kPathSeparator)) {
#ifdef _WIN32
    // Skip the Microsoft Store execution-alias directory: it contains stubs
    // that print an install prompt and exit 9009 instead of running anything.
    std::string lower = directory;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
      return static_cast<char>(std::tolower(c));
    });
    if (lower.find("\\windowsapps") != std::string::npos) {
      continue;
    }
#endif
    for (const std::string& candidate : names) {
      const fs::path full = fs::path(directory) / candidate;
      if (fs::exists(full, error) && !fs::is_directory(full, error)) {
        return full.string();
      }
    }
  }
  return {};
}

RuntimeRegistry RuntimeRegistry::discover() {
  RuntimeRegistry registry;
  registry.descriptors_ = builtinRuntimes();
  return registry;
}

const RuntimeDescriptor* RuntimeRegistry::at(size_t index) const {
  if (index >= descriptors_.size()) {
    return nullptr;
  }
  return &descriptors_[index];
}

const RuntimeDescriptor* RuntimeRegistry::byId(const std::string& id) const {
  for (const RuntimeDescriptor& descriptor : descriptors_) {
    if (descriptor.id == id) {
      return &descriptor;
    }
  }
  return nullptr;
}

RuntimeDescriptor* RuntimeRegistry::mutableById(const std::string& id) {
  for (RuntimeDescriptor& descriptor : descriptors_) {
    if (descriptor.id == id) {
      return &descriptor;
    }
  }
  return nullptr;
}

const RuntimeDescriptor* RuntimeRegistry::forLanguage(const std::string& languageId) const {
  for (const RuntimeDescriptor& descriptor : descriptors_) {
    for (const std::string& language : descriptor.languages) {
      if (language == languageId) {
        return &descriptor;
      }
    }
  }
  return nullptr;
}

std::vector<std::string> RuntimeRegistry::availableLanguages() const {
  std::vector<std::string> languages;
  for (const RuntimeDescriptor& descriptor : descriptors_) {
    if (!descriptor.available) {
      continue;
    }
    for (const std::string& language : descriptor.languages) {
      languages.push_back(language);
    }
  }
  std::sort(languages.begin(), languages.end());
  languages.erase(std::unique(languages.begin(), languages.end()), languages.end());
  return languages;
}

void RuntimeRegistry::add(RuntimeDescriptor descriptor) {
  for (RuntimeDescriptor& existing : descriptors_) {
    if (existing.id == descriptor.id) {
      existing = std::move(descriptor);
      return;
    }
  }
  descriptors_.push_back(std::move(descriptor));
}

size_t RuntimeRegistry::probeVersions(uint32_t timeoutMs) {
  size_t probed = 0;
  for (RuntimeDescriptor& descriptor : descriptors_) {
    if (!descriptor.available || descriptor.executable.empty()) {
      continue;
    }
    ExecutionOptions options;
    options.command = descriptor.executable;
    options.arguments = {"--version"};
    options.timeoutMs = timeoutMs == 0 ? 3000 : timeoutMs;

    std::string startError;
    auto session = ExecutionSession::start(options, &startError);
    if (!session) {
      continue;
    }
    std::string collected;
    ExecutionEvent event;
    const auto deadline =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(options.timeoutMs == 0 ? 3000 : options.timeoutMs + 1000);
    while (std::chrono::steady_clock::now() < deadline) {
      while (session->poll(&event)) {
        if (event.kind == ExecutionEvent::Kind::Output &&
            event.stream == ExecutionOutputStream::Stdout) {
          collected += event.data;
        }
      }
      if (!session->running()) {
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    while (session->poll(&event)) {
      if (event.kind == ExecutionEvent::Kind::Output &&
          event.stream == ExecutionOutputStream::Stdout) {
        collected += event.data;
      }
    }
    const std::string line = firstLine(collected);
    if (!line.empty()) {
      descriptor.version = line;
      ++probed;
    }
  }
  return probed;
}

Status RuntimeRegistry::plan(const RuntimeRequest& request, RuntimePlan* out,
                             std::string* error) const {
  if (!out) {
    return Status::InvalidArgument;
  }
  if (request.language.empty()) {
    if (error) *error = "language is required";
    return Status::InvalidArgument;
  }
  if (request.sourcePath.empty()) {
    if (error) *error = "sourcePath is required";
    return Status::InvalidArgument;
  }

  const RuntimeDescriptor* descriptor = forLanguage(request.language);
  if (!descriptor) {
    if (error) {
      *error = "no runtime is registered for language '" + request.language + "'";
    }
    return Status::InvalidArgument;
  }

  const bool hasCompileStep = !descriptor->compileExecutable.empty();
  if (!descriptor->available && !hasCompileStep) {
    if (error) {
      *error = "runtime '" + descriptor->id + "' for language '" + request.language +
               "' is not installed or not on PATH";
    }
    return Status::Unsupported;
  }
  if (!hasCompileStep && descriptor->executable.empty()) {
    if (error) *error = "runtime '" + descriptor->id + "' has no executable";
    return Status::Unsupported;
  }

  namespace fs = std::filesystem;
  const fs::path sourcePath(request.sourcePath);
  const fs::path sourceAbsolute = sourcePath.is_absolute()
                                      ? sourcePath
                                      : fs::current_path() / sourcePath;
  const fs::path parent = sourceAbsolute.parent_path().empty()
                              ? fs::current_path()
                              : sourceAbsolute.parent_path();
  const std::string source = sourceAbsolute.string();
  const std::string sourceDir = parent.string();

  fs::path output = request.outputPath.empty()
                        ? parent / (sourceAbsolute.stem().string() + ".out" +
                                    descriptor->outputExtension)
                        : fs::path(request.outputPath);
  if (!output.is_absolute()) {
    output = fs::current_path() / output;
  }
  const std::string outputPath = output.string();

  out->needsCompile = hasCompileStep && request.compile;
  out->compile = ExecutionOptions{};
  out->run = ExecutionOptions{};

  if (out->needsCompile) {
    out->compile.command = descriptor->compileExecutable;
    out->compile.arguments =
        applyPlaceholders(descriptor->compileArguments, source, outputPath, sourceDir);
    out->compile.workingDirectory =
        request.workingDirectory.empty() ? sourceDir : request.workingDirectory;
    out->compile.environment = request.environment;
    out->compile.timeoutMs = request.timeoutMs;
  }

  out->run.command = descriptor->runCompiledArtifact && hasCompileStep
                         ? outputPath
                         : descriptor->executable;
  std::vector<std::string> runArguments =
      applyPlaceholders(descriptor->runArguments, source, outputPath, sourceDir);
  for (const std::string& extra : request.arguments) {
    runArguments.push_back(extra);
  }
  for (const std::string& trailing : descriptor->trailingArguments) {
    runArguments.push_back(trailing);
  }
  out->run.arguments = std::move(runArguments);
  out->run.workingDirectory =
      request.workingDirectory.empty() ? sourceDir : request.workingDirectory;
  out->run.environment = request.environment;
  out->run.timeoutMs = request.timeoutMs;
  return Status::Ok;
}

std::unique_ptr<ExecutionSession> RuntimeRegistry::run(const RuntimeRequest& request,
                                                       std::string* error) const {
  RuntimePlan plan;
  std::string planError;
  const Status status = this->plan(request, &plan, &planError);
  if (status != Status::Ok) {
    if (error) *error = planError;
    return nullptr;
  }

  if (plan.needsCompile) {
    std::string startError;
    auto compiler = ExecutionSession::start(plan.compile, &startError);
    if (!compiler) {
      if (error) *error = startError;
      return nullptr;
    }

    std::string output;
    ExecutionEvent event;
    int32_t exitCode = -1;
    while (true) {
      while (compiler->poll(&event)) {
        if (event.kind == ExecutionEvent::Kind::Output) {
          output += event.data;
        } else {
          exitCode = event.exitCode;
        }
      }
      if (!compiler->running()) {
        break;
      }
      compiler->wait(20);
    }
    while (compiler->poll(&event)) {
      if (event.kind == ExecutionEvent::Kind::Output) {
        output += event.data;
      } else {
        exitCode = event.exitCode;
      }
    }
    if (exitCode != 0) {
      if (error) {
        *error = "compile failed with exit code " + std::to_string(exitCode);
        if (!output.empty()) {
          *error += ":\n" + output;
        }
      }
      return nullptr;
    }
    if (output.size() > 64 * 1024) {
      output.resize(64 * 1024);
    }
  }

  std::string startError;
  auto session = ExecutionSession::start(plan.run, &startError);
  if (!session) {
    if (error) *error = startError;
    return nullptr;
  }
  return session;
}

}  // namespace codecore
