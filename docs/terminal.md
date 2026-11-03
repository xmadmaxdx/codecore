# Terminal sessions

`core/term` runs an interactive shell and streams its bytes. The terminal widget,
key bindings, scrollback and themes belong to the host application; CodeCore owns
the process, the terminal device and window-size changes.

## Platform behaviour

| Platform | Backend | `usesPty()` | `resize()` |
|---|---|---|---|
| Android / Linux / macOS | `posix_openpt` + `fork`, child gets `TIOCSCTTY` | `true` | `TIOCSWINSZ` |
| Windows | `CreateProcessW` with piped stdin/stdout | `false` | no-op |

The Windows path is deliberately a pipe, not ConPTY. `CreatePseudoConsole` was
implemented and tested here; on Windows 11 the pseudoconsole returned EOF on the
child's stdin immediately, so the shell printed its banner and exited. Shipping a
pipe and reporting `usesPty() == false` is honest; a host that needs ConPTY can
attach its own console and use `cc_execution_*` instead.

Because POSIX uses a real PTY, the child is a session leader with a controlling
terminal: line editing, `SIGWINCH`, job control and `isatty()` behave normally.
Output therefore contains ANSI escapes and echoed input; the host's widget must
render them.

## C API

```c
cc_terminal_options options = {0};
options.columns = 100;
options.rows = 30;

char* error = NULL;
cc_terminal* term = cc_terminal_start(&options, &error);
if (!term) { fprintf(stderr, "%s\n", error); cc_string_free(error); }

cc_terminal_write(term, "ls\r", 3);

cc_terminal_event event;
while (cc_terminal_poll(term, &event)) {
  if (event.kind == 0) {
    fwrite(event.data, 1, event.length, stdout);
    cc_string_free(event.data);
  } else {
    printf("exit %d\n", event.exit_code);
  }
}
cc_terminal_destroy(term);
```

- `cc_terminal_poll` is non-blocking; `cc_terminal_wait` blocks up to a timeout.
- `kind` is `0` for output and `1` for exit. Output data is allocated and must be
  released with `cc_string_free`.
- `cc_terminal_write` returns `CC_STATUS_NOT_RUNNING` once the shell has exited.
- A bounded output backlog (1 MiB by default) pushes back on the reader thread,
  so a host that stops polling cannot grow memory without limit.
- `cc_terminal_destroy` terminates the process tree, closes the terminal and
  joins the reader threads.

## C++ API

```cpp
codecore::TerminalOptions options;
options.columns = 120;
options.rows = 32;
options.workingDirectory = "/project";

std::string error;
auto session = codecore::TerminalSession::start(options, &error);
session->write("cargo build\n");
session->resize(140, 40);
```

Each session owns one reader thread and one waiter thread. `poll()` never blocks
and events carry a strictly increasing sequence number.

## What it does not do

- No screen buffer, no escape-sequence parser, no scrollback.
- No shell integration (prompt detection, command boundaries).
- No Windows ConPTY (see above).
- No automatic shell discovery beyond `$SHELL` / `%COMSPEC%`; pass `command`
  explicitly to use `bash`, `zsh`, `pwsh` or any other shell.
