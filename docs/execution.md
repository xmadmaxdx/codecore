# Local execution

CodeCore runs child processes. It does not ship or choose a runtime, compiler or
package manager. The host passes an executable and arguments; CodeCore owns the
pipes, streaming, timeout, cancellation and exit reporting.

This is the first layer of the offline-coding roadmap. Runtimes, PTY/terminal,
package management and distribution are separate phases; see
[UPDATED_PLAN.md](../UPDATED_PLAN.md) locally.

## Model

Each `cc_execution*` handle (C ABI) or `ExecutionSession` (C++) is one child
process with three pipes:

```text
host write ──> stdin  ──> child
host poll  <── stdout <── child
host poll  <── stderr <── child
```

- `start` validates the executable and working directory before spawning.
- Output is queued as chunks with a per-session sequence number. The host polls;
  there are no callbacks and no UI-thread coupling.
- The exit event is delivered only after all queued output. Per-stream byte order
  is exact; stdout/stderr interleaving is reader enqueue order.
- Exit codes are the process status. On POSIX a signal death is `128 + signal`.
- `cancel` terminates the process tree. `wait` is optional and can be replaced by
  polling the exit event.
- `destroy` cancels a running child, joins internal threads and releases handles.

The queue is bounded (default 1 MiB). If the host stops polling, reader threads
block and the OS pipe applies backpressure to the child. A host that destroys a
session while output is queued loses that queued output.

## C example

```c
cc_execution_options options = {0};
options.struct_size = sizeof(options);
options.command = "python3";
options.args = (const char* const[]){"-c", "print('hello')"};
options.arg_count = 2;
options.timeout_ms = 10000;
options.environment = ...; /* see cc_execution_options */
```

Start it, then poll:

```c
char* error = NULL;
cc_execution* run = cc_execution_start(&options, &error);
if (!run) {
  fprintf(stderr, "start failed: %s\n", error);
  cc_string_free(error);
  return;
}

cc_execution_event event;
while (cc_execution_poll(run, &event)) {
  if (event.kind == CC_EXECUTION_OUTPUT) {
    fwrite(event.data, 1, event.length, event.stream == CC_EXECUTION_STDOUT
                                             ? stdout
                                             : stderr);
    cc_string_free(event.data);
  } else {
    printf("exit=%d cancelled=%u timeout=%u\n", event.exit_code,
           event.cancelled, event.timed_out);
  }
}
cc_execution_destroy(run);
```

For interactive input, call `cc_execution_write` with UTF-8 bytes and
`cc_execution_close_stdin` to send EOF. `cc_execution_wait` blocks with a
millisecond timeout; zero means "check now", not "wait forever".

## SDK wrappers

Dart:

```dart
final run = core.startExecution(
  command: 'python3',
  arguments: ['-c', 'print("hello")'],
  timeout: const Duration(seconds: 10),
);
while (true) {
  final event = run.poll();
  if (event == null) { sleep(const Duration(milliseconds: 5)); continue; }
  if (event.kind == ExecutionEventKind.output) {
    stdout.write(event.data);
  } else {
    print(event.exitCode);
    break;
  }
}
run.dispose();
```

Kotlin:

```kotlin
val run = CodeCore.startExecution(command = "python3", timeoutMs = 10_000)
while (true) {
    val event = run.poll() ?: continue
    if (event.kind == ExecutionEventKind.OUTPUT) print(event.data) else break
}
run.close()
```

## Options

| Field | Meaning |
|---|---|
| `command` | Executable name or path. On POSIX it is resolved through `PATH`; on Windows `CreateProcessW` resolves it. |
| `args`, `arg_count` | argv[1..]. No program name. Must be NULL when count is 0. |
| `working_directory` | Validated before spawn; NULL inherits the host's cwd. |
| `env_keys`, `env_values`, `env_count` | Additions/overrides merged into the inherited environment. |
| `timeout_ms` | 0 disables the timeout. On expiry the child is terminated and the exit event has `timed_out=1`. |
| `max_queued_bytes` | Soft output backlog bound. 0 uses 1 MiB. |
| `shell` | Runs `command` verbatim through `cmd.exe /d /s /c` or `/bin/sh -c`. Arguments must be empty. Off by default. |

## Threading

The API is caller-threaded: `poll`, `write`, `wait`, `cancel` and `destroy` may
be called from one host thread while internal reader and waiter threads run.
`cancel` and `poll` are safe to call from different threads. Do not call
`destroy` concurrently with another method on the same handle.

## Platform notes

- **Windows:** `CreateProcessW` with an explicit handle-inheritance list, three
  pipes, and a job object with `JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE`. Cancellation
  terminates the job, covering descendants. `CREATE_NO_WINDOW` avoids a console
  flash. UTF-8 is converted at the boundary.
- **POSIX / Android:** `fork` + `execve` with argv/envp built in the parent, a
  process group for tree termination, and `waitpid(WNOHANG)` polling. Only
  async-signal-safe calls run between `fork` and `execve`. Working-directory
  failure in the child exits 127; the parent validates the directory first.
- **Android policy:** executing files from app data is blocked on modern
  Android. Package runtimes as native libraries inside the APK or provide an
  interpreter the platform permits. CodeCore does not bypass this.
- **No runtime:** `libcodecore` does not link Python, Node, a JVM, a compiler or
  a shell. The host is responsible for distribution and licensing.

## Verification

`tests/core/test_execution.cpp` uses `codecore_exec_helper.exe`, a dependency-free
child, and covers:

- stdout/stderr separation and exact bytes
- exit codes
- stdin writes and EOF
- working directory and environment overrides
- cancellation and timeout termination
- three concurrent sessions
- invalid command / shell-argument rejection
- the C ABI poll, wait, status and ownership path

Run it with:

```bash
cmake --build build/native --target codecore_execution_tests
./build/native/codecore_execution_tests.exe
```

The suite reports `Execution checks: 79, failures: 0` on Windows and is part of
CTest. The Dart smoke test covers shell start, output and exit (111 checks); the
JNI harness covers the same through JNI (126 checks). The POSIX backend is
compiled by the Android NDK build; it was not run on a device in this
repository's verification.
