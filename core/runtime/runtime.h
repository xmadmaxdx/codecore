#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/document/types.h"
#include "core/exec/execution.h"

namespace codecore {

/*
 * One language runtime or toolchain that CodeCore can invoke.
 *
 * A descriptor never contains the runtime itself. `executable` is resolved
 * through PATH (or set explicitly by the host); `available` reports whether it
 * was found. Nothing is downloaded and no compiler is bundled with the engine.
 */
struct RuntimeDescriptor {
  /* Stable id, e.g. "python", "node", "gcc". */
  std::string id;
  std::string displayName;

  /* Language ids this runtime can execute, e.g. {"python", "py"}. */
  std::vector<std::string> languages;

  /* Resolved absolute path; empty when the runtime is not installed. */
  std::string executable;
  bool available = false;

  /* First line of `<executable> --version`, empty when it could not be read. */
  std::string version;

  /* Arguments before the source path for the run step; "{source}" is the path. */
  std::vector<std::string> runArguments;
  /* Extension used when the host lets CodeCore choose an output file name. */
  std::string outputExtension;

  /* Optional compile step. Empty executable means "run directly". */
  std::string compileExecutable;
  std::vector<std::string> compileArguments;
  /* Set when the compiled artifact is executed after a successful compile. */
  bool runCompiledArtifact = false;

  /* Arguments appended after the source path, e.g. cue flags. */
  std::vector<std::string> trailingArguments;
};

struct RuntimeRequest {
  /* Language id. The registry maps it to a runtime. */
  std::string language;
  std::string sourcePath;
  std::vector<std::string> arguments;
  std::string workingDirectory;
  std::vector<std::pair<std::string, std::string>> environment;
  uint32_t timeoutMs = 0;

  /* Skip the compile step for compiled languages when false. */
  bool compile = true;

  /* Output path for compiled languages. Empty derives one next to the source. */
  std::string outputPath;
};

/* The concrete steps a request resolves to. */
struct RuntimePlan {
  bool needsCompile = false;
  ExecutionOptions compile;
  ExecutionOptions run;
};

/*
 * Registry of installed language runtimes.
 *
 * Discovery scans PATH once per registry; version detection spawns the runtime
 * with `--version` and is therefore explicit (`probeVersions`), not automatic.
 * Hosts may register extra runtimes with add().
 */
class RuntimeRegistry {
 public:
  RuntimeRegistry();

  /* Adds the built-in descriptor table with PATH resolution. */
  static RuntimeRegistry discover();

  size_t count() const { return descriptors_.size(); }
  const RuntimeDescriptor* at(size_t index) const;
  const RuntimeDescriptor* byId(const std::string& id) const;
  /* Mutable lookup for hosts that adjust a discovered descriptor in place. */
  RuntimeDescriptor* mutableById(const std::string& id);
  const RuntimeDescriptor* forLanguage(const std::string& languageId) const;
  std::vector<std::string> availableLanguages() const;

  /* Registers or replaces a descriptor by id. */
  void add(RuntimeDescriptor descriptor);

  /* Fills `version` for every available runtime; returns how many were probed. */
  size_t probeVersions(uint32_t timeoutMs = 3000);

  /*
   * Resolves a request into compile/run commands. Fails with Status::NotFound
   * when no runtime handles the language and Status::Unavailable when the
   * runtime is not installed.
   */
  Status plan(const RuntimeRequest& request, RuntimePlan* out, std::string* error) const;

  /*
   * Compiles when needed, waits for the compiler, then starts the program.
   * Returns a running session or nullptr with `error` describing the failure
   * (compiler output is included for compile failures).
   */
  std::unique_ptr<ExecutionSession> run(const RuntimeRequest& request,
                                        std::string* error) const;

  /* Resolves an executable name through PATH. */
  static std::string resolveExecutable(const std::string& name);

 private:
  std::vector<RuntimeDescriptor> descriptors_;
};

}  // namespace codecore
