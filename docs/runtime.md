# Runtime registry

`core/runtime` is the bridge between a language id and a program that can run it.
CodeCore ships no interpreter, compiler or SDK. The registry finds what the device
already has, and builds the commands to compile and run a source file.

## Discovery

`cc_runtime_registry_create()` (C++) `RuntimeRegistry::discover()` creates a table
of 22 built-in runtimes and resolves each executable through `PATH`. On Windows
the lookup honours `PATHEXT` and skips `%LOCALAPPDATA%\Microsoft\WindowsApps`,
whose execution aliases are stubs that print an install prompt and exit 9009.

| Runtime id | Languages | Run step | Compile step |
|---|---|---|---|
| `python` | `python`, `py`, `python3` | `python3 {source}` | none |
| `node` | `javascript`, `js`, `jsx`, `mjs`, `cjs` | `node {source}` | none |
| `deno` | `typescript`, `ts`, `tsx` | `deno run --allow-all {source}` | none |
| `ruby` | `ruby`, `rb` | `ruby {source}` | none |
| `php` | `php` | `php {source}` | none |
| `perl` | `perl`, `pl` | `perl {source}` | none |
| `lua` | `lua` | `lua {source}` | none |
| `rscript` | `r` | `Rscript {source}` | none |
| `julia` | `julia` | `julia {source}` | none |
| `bash` | `bash`, `sh`, `zsh` | `bash {source}` | none |
| `powershell` | `powershell`, `ps1` | `pwsh -File {source}` | none |
| `elixir` | `elixir` | `elixir {source}` | none |
| `runghc` | `haskell`, `hs` | `runghc {source}` | none |
| `dart` | `dart` | `dart run {source}` | none |
| `go` | `go`, `golang` | `go run {source}` | internal |
| `dotnet` | `csharp`, `cs` | `dotnet run --project {sourceDir}` | internal |
| `java` | `java` | `java {source}` (single-file mode) | `javac` |
| `kotlin` | `kotlin`, `kt` | `kotlin -classpath {output}` | `kotlinc` |
| `rust` | `rust`, `rs` | `{output}` | `rustc -O -o {output} {source}` |
| `gcc` | `c` | `{output}` | `gcc {source} -o {output}` |
| `g++` | `cpp`, `c++`, `cc` | `{output}` | `g++ -std=c++17 {source} -o {output}` |
| `gfortran` | `fortran` | `{output}` | `gfortran {source} -o {output}` |

Placeholders are `{source}`, `{output}` and `{sourceDir}`. When `output` is not
supplied it becomes `<source stem>.out<extension>` next to the source.

`probeVersions()` runs `<executable> --version` once per available runtime and
keeps the first line. It is explicit because it spawns processes.

## Unified run

```cpp
auto registry = codecore::RuntimeRegistry::discover();
registry.probeVersions(3000);

codecore::RuntimeRequest request;
request.language = "python";
request.sourcePath = "/project/main.py";
request.arguments = {"--verbose"};

std::string error;
auto session = registry.run(request, &error);   // compile first, then run
```

`plan()` exposes the same decision without starting anything: it fills a
`RuntimePlan` with the compile and run `ExecutionOptions`, so a host can show a
"Run" preview or execute it itself.

Behaviour worth knowing:

- A language id maps to exactly one runtime; unknown ids return
  `Status::InvalidArgument`, missing runtimes `Status::Unsupported`, both with a
  readable message.
- For compiled languages the compiler runs to completion first. A non-zero exit
  returns `nullptr` and `error` contains the compiler's output, so a host can put
  it straight into an output panel.
- `request.compile = false` skips the compiler and runs the artifact path.
- Nothing is downloaded, cached or installed. Add custom runtimes with
  `RuntimeRegistry::add()` (C++); the C ABI exposes the built-in table only.
- Arguments are passed as an array to `CreateProcess`/`execvp`. There is no shell
  expansion, quoting surprise or injection through the source path.

## C API

`cc_runtime_registry_create`, `cc_runtime_count`, `cc_runtime_at`,
`cc_runtime_info_free`, `cc_runtime_find_for_language`,
`cc_runtime_probe_versions`, `cc_runtime_plan_request`, `cc_runtime_plan_free`
and `cc_runtime_run`. `cc_runtime_run` returns a `cc_execution*`, so the existing
poll/wait/cancel functions work unchanged.
