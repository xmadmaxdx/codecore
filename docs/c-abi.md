# C ABI guide

Header: `native/include/codecore/codecore.h`

The ABI is versioned with `cc_abi_version()` (currently `7`) and
`cc_version_string()` (`0.6.0`). ABI version and library version are separate:
the ABI changes only when the binary contract changes; the library version also
moves for behavior additions.

## Lifecycle

```c
cc_engine* engine = cc_engine_create(NULL);          /* default: 64 MB documents */
cc_document* doc = cc_document_create(engine, NULL); /* defaults to Python */

/* ... */

cc_document_destroy(doc);
cc_engine_destroy(engine);
```

A document holds its own shared reference to its language, so it remains usable
after the engine is destroyed. Documents are not destroyed with the engine.

## Ownership

| Returned value | Release with |
|---|---|
| `char*` from `cc_document_get_text` | `cc_string_free` |
| `char*` from `cc_document_get_proposed_text` | `cc_string_free` |
| `char*` from `cc_document_get_ghost_text` | `cc_string_free` |
| `char*` from `cc_snapshot_get_text` | `cc_string_free` |
| `cc_token*` from tokenization | `cc_tokens_free` |
| `cc_completion_item*` | `cc_completions_free(items, count)` |
| `cc_diagnostic*` | `cc_diagnostics_free(items, count)` |
| `cc_folding_range*` | `cc_folding_ranges_free` |
| `cc_range*` from find | `cc_search_ranges_free` |
| `cc_diff_line*` from proposal diff | `cc_diff_lines_free(lines, count)` |
| `cc_language_info*` | Not freed; engine-owned |
| `cc_snapshot*` | `cc_snapshot_destroy` |
| `cc_syntax_tree*` | `cc_syntax_tree_destroy` |

Empty result sets return `0` and set the out-pointer to `NULL`. Inputs are
copied before a function returns.

## Status codes

- `CC_STATUS_OK`
- `CC_STATUS_INVALID_ARGUMENT` — null pointer, invalid UTF-8, code-point split,
  unknown origin, or an operation that requires missing preview state.
- `CC_STATUS_OUT_OF_RANGE` — offset/range outside the document or size limit.
- `CC_STATUS_READ_ONLY` — mutation attempted on a read-only document.
- `CC_STATUS_UNKNOWN_LANGUAGE` — unknown language id.
- `CC_STATUS_UNSUPPORTED` — no matching bracket, for example.
- `CC_STATUS_OUT_OF_MEMORY`, `CC_STATUS_INTERNAL_ERROR`.

On failure, `cc_document_last_error` returns a document-owned message.

## Text and coordinates

- All offsets are UTF-8 byte offsets.
- Edit offsets must fall on code-point boundaries; invalid UTF-8 is rejected.
- Line indexes are zero-based.
- `cc_position.column` counts Unicode code points.
- `cc_position_to_offset` clamps past the end of line content and does not
  include the line ending.
- Caret positions are defined on line content, not inside a CR/LF terminator.

## Editing and permissions

```c
cc_document_set_read_only(doc, 1);   /* locked, still selectable/queryable */
cc_document_set_editable(doc, 1);    /* inverse convenience alias */
cc_document_set_read_only(doc, 0);
```

`cc_document_edit_origin(doc)` and `cc_document_set_edit_origin(doc, origin)`
classify subsequent edits. Values are `CC_EDIT_ORIGIN_USER`, `AI`, `HOST`,
`FORMATTER` and `EXTERNAL_TOOL`. The origin is stored in undo records and in
change events.

## Change events

```c
static void on_change(void* user, const cc_change_event* event) {
  printf("v%llu kind=%u origin=%u start=%u -%u +%u\n",
         (unsigned long long)event->version, event->kind, event->origin,
         event->start, event->removed_length, event->inserted_length);
}

cc_document_set_change_listener(doc, on_change, my_state);
```

Kinds: set text, insert, erase, replace, undo, redo, proposal accepted, proposal
rejected, snapshot restored, ghost text accepted, ghost text dismissed.

Listeners run synchronously on the mutating thread while the document is locked.
They must not destroy the document or block. Query methods may be reentered from
the same thread because the mutex is recursive, but that is not a general
invitation to call arbitrary APIs from the callback.

Proposal rejection and ghost dismissal do not change document text, so their
event version equals the current text version. Treat the kind as the signal.

## Proposed changes

```c
cc_document_set_proposed_text(doc, proposed, proposed_length, CC_EDIT_ORIGIN_AI);
if (cc_document_has_proposal(doc)) {
  cc_diff_line* lines = NULL;
  size_t count = cc_document_proposal_diff(doc, &lines);
  for (size_t i = 0; i < count; ++i) {
    /* kind: 0 equal, 1 insert, 2 delete; old_line/new_line: 0-based or UINT32_MAX */
  }
  cc_diff_lines_free(lines, count);
}

cc_document_accept_proposal(doc);  /* one undoable replacement */
cc_document_reject_proposal(doc);  /* preview discarded */
```

Setting a proposal never mutates document text. Accepting computes a minimal
replacement range and records it as one history entry. Rejecting clears it.

## Ghost text

```c
cc_document_set_ghost_text(doc, offset, suggestion, length,
                           CC_EDIT_ORIGIN_AI);

size_t offset_out = 0, length_out = 0;
uint32_t origin_out = 0;
char* suggestion = cc_document_get_ghost_text(doc, &offset_out, &length_out,
                                              &origin_out);
cc_string_free(suggestion);

cc_document_accept_ghost_text(doc, 6); /* first 6 code points; 0 = all */
cc_document_dismiss_ghost_text(doc);
```

Ghost text is display state until accepted. It is cleared automatically by any
document edit, undo or redo. Read-only documents may hold ghost text, but
acceptance returns `CC_STATUS_READ_ONLY`.

## Snapshots

```c
cc_snapshot* snap = cc_document_create_snapshot(doc);
size_t length = 0;
char* text = cc_snapshot_get_text(snap, &length);
cc_string_free(text);

cc_document_restore_snapshot(doc, snap); /* one undoable edit */
cc_snapshot_destroy(snap);
```

Snapshots are immutable and remain valid after the source document changes or is
destroyed. They are independently queryable for text, version, language, tokens,
completion, diagnostics, folding and find. Restoration is rejected on read-only
documents and does not reset the undo history.

## Multi-cursor

Selections are a list; index 0 is the primary caret used by
`cc_document_get_selection`.

```c
cc_document_set_selection(doc, 0, 0);
cc_document_add_cursor(doc, 4, 4);

cc_selection second = {0};
cc_document_cursor_at(doc, 1, &second);   /* anchor=4, focus=4 */

cc_document_multi_insert(doc, "# ", 2);   /* one undo entry */
cc_document_undo(doc);                    /* restores both lines */

cc_document_move_cursors(doc, CC_MOVE_WORD_RIGHT, 0);
cc_document_multi_erase(doc, 1);          /* backspace at every caret */
cc_document_clear_cursors(doc);           /* keep only the primary */
```

Overlapping selections merge before a multi edit. Duplicate collapsed carets
collapse to one insertion. Multi-cursor operations emit one change event per
affected range, all under the same document version; undo and redo apply the
whole group as one history entry.

## Search

`cc_document_find` is the original literal API and remains available. It returns
the match count and cannot distinguish "no matches" from an invalid pattern.

For status reporting and regular expressions use `cc_document_search` or
`cc_snapshot_search`:

```c
cc_search_options options = {0};
options.struct_size = sizeof(options);
options.case_sensitive = 1;
options.regex = 1;

const char* pattern = "\\bdef\\s+\\w+";
cc_range* ranges = NULL;
size_t count = 0;
cc_status status = cc_document_search(doc, pattern, strlen(pattern), &options,
                                      &ranges, &count);
if (status == CC_STATUS_INVALID_ARGUMENT) {
  /* malformed pattern; cc_document_last_error has the reason */
}
cc_search_ranges_free(ranges);
```

`options` must be zero-initialized before fields are set. Regex syntax is
ECMAScript with multiline `^` and `$`; matching is case-insensitive unless
`case_sensitive` is set. `whole_word` adds word-boundary checks around each
match. Results are capped at 10,000 ranges. A malformed pattern returns
`CC_STATUS_INVALID_ARGUMENT` with no ranges and sets the document's last error.

## Language switching

`cc_document_set_language(doc, language_id)` uses an id from
`cc_engine_find_language`. The document wrapper owns the shared language
references. Switching language invalidates analysis but does not edit text.

## Language capabilities

`cc_language_info.capabilities` is a bit mask:

- `CC_LANGUAGE_CAP_SYNTAX` — lexical tokens.
- `CC_LANGUAGE_CAP_COMPLETION` — local completion.
- `CC_LANGUAGE_CAP_DIAGNOSTICS` — bracket/lexical diagnostics.
- `CC_LANGUAGE_CAP_FOLDING` — brace/comment folding.

There is no semantic-LSP flag. Do not infer LSP support from these bits.

## LSP client

The shared LSP client is transport-agnostic: the host owns the language-server
process or socket.

```c
cc_lsp_client* lsp = cc_lsp_create();
cc_lsp_initialize(lsp, "file:///project");

size_t frame_length = 0;
char* frame = cc_lsp_take_outgoing(lsp, &frame_length);
/* write frame, frame_length bytes, to the language server */
cc_string_free(frame);

/* when bytes arrive from the server */
cc_lsp_feed(lsp, bytes, bytes_length);

int64_t result_id = 0;
char* method = NULL;
char* payload = NULL;
uint8_t is_error = 0;
if (cc_lsp_take_result(lsp, &result_id, &method, &payload, &is_error)) {
  /* payload is the result JSON; is_error marks an error response */
  cc_string_free(method);
  cc_string_free(payload);
}

cc_lsp_did_open(lsp, "file:///project/main.py", "python", 1, text, text_length);
int64_t request = cc_lsp_completion(lsp, "file:///project/main.py", 2, 4);
/* Also: hover, definition, signature_help, references, document_symbols,
 * workspace_symbols, rename, formatting, code_action, semantic_tokens. */
const char* raw_params = "{\"command\":\"codecore.noop\"}";
int64_t raw = cc_lsp_request(lsp, "workspace/executeCommand",
                             raw_params, strlen(raw_params));
cc_lsp_notify(lsp, "$/setTrace", "{\"value\":\"off\"}", 15);
cc_lsp_cancel(lsp, request);
cc_lsp_shutdown(lsp);
cc_lsp_exit(lsp);
cc_lsp_destroy(lsp);
```

- Outgoing frames already include `Content-Length` framing.
- Results and diagnostics are owned JSON strings; free them with
  `cc_string_free`. Take results/diagnostics in a loop; each call dequeues one.
- ABI 4 added the result id and error flag to `cc_lsp_take_result`.
- `cc_lsp_server_capabilities` and `cc_lsp_last_error` are client-owned and stay
  valid until the next `cc_lsp_feed`.
- Positions are LSP UTF-16 line/character values, not CodeCore byte offsets.
- Sync is full-text (`TextDocumentSyncKind.Full`).
- Frames larger than 16 MB or malformed JSON are rejected and reported through
  the last-error string.

## Local process execution

CodeCore does not bundle a runtime. The host supplies an executable and
arguments; CodeCore owns the child, the pipes and the reader threads.

```c
const char* args[] = {"-c", "print('hello')"};
cc_execution_options options = {0};
options.struct_size = sizeof(options);
options.command = "python3";
options.args = args;
options.arg_count = 2;
options.timeout_ms = 10000;

char* error = NULL;
cc_execution* run = cc_execution_start(&options, &error);
if (!run) {
  fprintf(stderr, "%s\n", error);
  cc_string_free(error);
  return;
}

cc_execution_event event;
while (cc_execution_poll(run, &event)) {
  if (event.kind == CC_EXECUTION_OUTPUT) {
    fwrite(event.data, 1, event.length, event.stream == CC_EXECUTION_STDOUT ? stdout : stderr);
    cc_string_free(event.data);
  } else {
    printf("exit=%d cancelled=%u timeout=%u\n", event.exit_code, event.cancelled,
           event.timed_out);
  }
}

cc_execution_destroy(run);
```

- `cc_execution_start` returns `NULL` on failure and allocates `*out_error` with
  `cc_alloc`; release it with `cc_string_free`.
- Output `data` is allocated with `cc_alloc` and released with `cc_string_free`.
  The exit event is delivered only after all queued output.
- `cc_execution_poll` is non-blocking. `cc_execution_wait` is optional;
  `timeout_ms == 0` is a state check, not an infinite wait.
- `cc_execution_write` returns `CC_STATUS_NOT_RUNNING` after exit and
  `CC_STATUS_IO_ERROR` when the pipe fails. `cc_execution_close_stdin` sends EOF.
- `cc_execution_destroy` cancels a running process and joins the internal
  threads. Call it from one thread; do not call it from a callback (there are
  no callbacks in this API).
- Exit codes are the process status. A POSIX signal death reports
  `128 + signal`. Windows uses the process exit code.
- `shell` runs `command` verbatim through `cmd.exe /d /s /c` or `/bin/sh -c`;
  arguments must be empty. Shell mode is opt-in.
- Backpressure: reader threads stop draining the OS pipe once
  `max_queued_bytes` output is waiting for the host (default 1 MiB). Keep
  polling; do not assume output is lossless if the session is destroyed early.
- The Windows backend uses a job object with kill-on-close, so cancellation
  reaches the child tree. The POSIX backend uses a process group.
- Android: the POSIX path is compiled, but app-data executables are blocked by
  platform policy. Package runtimes as native libraries or use a bundled
  interpreter; CodeCore does not supply one.

## Structural syntax (Tree-sitter)

Only languages with a bundled grammar produce a tree. Eight grammars are
compiled in:

| Language | Accepted ids | Root node |
|---|---|---|
| Python | `python`, `py`, `python3` | `module` |
| JavaScript | `javascript`, `js`, `jsx`, `mjs`, `cjs` | `program` |
| Java | `java` | `program` |
| Go | `go`, `golang` | `source_file` |
| JSON | `json` | `document` |
| C | `c`, `h` | `translation_unit` |
| C++ | `cpp`, `c++`, `cc`, `cxx`, `hpp` | `translation_unit` |
| Rust | `rust`, `rs` | `source_file` |

```c
cc_syntax_tree* tree = cc_document_syntax_tree(doc);
if (tree) {
  printf("%s errors=%u\n", cc_syntax_tree_root_type(tree),
         cc_syntax_tree_has_errors(tree));

  for (size_t i = 0; i < cc_syntax_tree_highlight_count(tree); ++i) {
    cc_syntax_highlight highlight = {0};
    cc_syntax_tree_highlight_at(tree, i, &highlight);
    /* highlight.start/end are UTF-8 bytes; kind is a cc_token_kind. */
  }

  for (size_t i = 0; i < cc_syntax_tree_error_count(tree); ++i) {
    cc_syntax_error error = {0};
    cc_syntax_tree_error_at(tree, i, &error);
  }
  cc_syntax_tree_destroy(tree);
}
```

- `cc_document_syntax_tree` parses the current version incrementally from the
  document's previous tree and returns an owned handle. Repeated calls on an
  unchanged version reuse the cached tree; building the handle still walks or
  queries the tree for counts and ranges.
- Returns `NULL` when the document's language has no bundled grammar. Check
  `cc_syntax_language_supported("java")`, `"go"`, `"json"`, `"c"`, `"cpp"`,
  `"rust"`, `"python"` or `"javascript"` when the grammar is required.
- `cc_syntax_tree_has_errors` and `cc_syntax_tree_error_at` report real
  Tree-sitter `ERROR` and `MISSING` nodes, including malformed Java, Go, JSON,
  C, C++ and Rust source.
- Highlight and error strings point into the tree handle and stay valid until
  `cc_syntax_tree_destroy`.
- `start`/`end` are UTF-8 byte offsets. Row/column values in `cc_syntax_error`
  follow Tree-sitter: rows are zero-based and columns are bytes within the row,
  not Unicode code points.
- Tree-sitter is syntax, not semantics: it does not resolve names or types.

## Threading

- The engine's language metadata is immutable and can be read concurrently.
- Calls on one document are serialized internally.
- Snapshots may be queried from any thread.
- The C ABI does not start threads.
- Change listeners run while the document is locked.

## Limits

- Default maximum document size: 64 MB, configurable through
  `cc_engine_config.max_document_bytes`.
- Default history depth: 200 edits; 0 disables undo.
- Completion is capped at 100 entries.
- Search is capped at 10,000 matches.
- Diagnostics are capped at 200 entries.
- Folding ranges are capped at 500 entries.
- Syntax highlights are capped by the tree itself; syntax errors are capped
  at 500 entries.
- Diff uses a bounded LCS table (4,000,000 cells / 20,000 lines); larger inputs
  fall back to a coarse delete-all/insert-all diff.

## Terminal sessions (ABI 7)

```c
cc_terminal_options options = {0};
options.columns = 120;
options.rows = 32;

char* error = NULL;
cc_terminal* terminal = cc_terminal_start(&options, &error);
if (!terminal) { /* error is allocated */ cc_string_free(error); }

cc_terminal_write(terminal, "make
", 5);

cc_terminal_event event;
while (cc_terminal_poll(terminal, &event)) {
  if (event.kind == 0) {
    /* render event.data[0..event.length) */
    cc_string_free(event.data);
  }
}
cc_terminal_destroy(terminal);
```

- `cc_terminal_uses_pty` is `0` on Windows, where the session is a piped shell.
- `cc_terminal_resize` returns `0` when there is no terminal device.
- `cc_terminal_destroy` terminates the process tree and joins its threads.

## Runtime registry (ABI 7)

```c
cc_runtime_registry* runtimes = cc_runtime_registry_create();
cc_runtime_probe_versions(runtimes, 3000);

cc_runtime_info info;
for (size_t i = 0; i < cc_runtime_count(runtimes); ++i) {
  if (cc_runtime_at(runtimes, i, &info)) {
    printf("%s available=%d version=%s
", info.id, info.available, info.version);
    cc_runtime_info_free(&info);
  }
}

cc_runtime_request request = {0};
request.language = "python";
request.source_path = "/project/main.py";
request.compile = 1;

cc_runtime_plan plan;
if (cc_runtime_plan_request(runtimes, &request, &plan, &error) == CC_STATUS_OK) {
  printf("run: %s
", plan.run_command);
  cc_runtime_plan_free(&plan);
}

cc_execution* run = cc_runtime_run(runtimes, &request, &error);
```

`cc_runtime_run` compiles first when the runtime needs it, then returns a running
`cc_execution`. On a compile failure it returns `NULL` and `out_error` contains the
compiler output. Strings returned by `cc_runtime_at` are allocated; free them with
`cc_runtime_info_free`.
