/*
 * CodeCore — embeddable code editor engine.
 *
 * Public C ABI. This is the only header that platform SDKs may depend on.
 * C++ classes and the C++ ABI are never exposed across this boundary.
 *
 * Ownership rules
 * ---------------
 * - Opaque handles (cc_engine, cc_document) are owned by the caller and must be
 *   released with the matching destroy function.
 * - Functions returning an array through an out-parameter allocate the array
 *   and every string inside it. Release them with the matching *_free function
 *   (cc_string_free for single strings).
 * - cc_language_info pointers returned by cc_engine_language_at stay valid for
 *   the lifetime of the engine and must not be freed.
 * - Input strings are borrowed for the duration of the call and are copied
 *   before the call returns.
 *
 * Threading
 * ---------
 * The engine's language registry is immutable after creation and may be read
 * from multiple threads. A document is internally serialized; calls on the same
 * document from different threads are safe, but a single document should not be
 * the subject of concurrent long-running batch work. Change listeners are
 * invoked synchronously on the mutating thread.
 *
 * Snapshots (cc_document_create_snapshot) are immutable, may be queried from
 * any thread, and are the recommended way to move analysis off the document
 * lock. They remain valid after the source document changes or is destroyed.
 *
 * Text offsets
 * ------------
 * All offsets are byte offsets into the document's UTF-8 encoding. Offsets
 * passed to edit functions must fall on UTF-8 code point boundaries. Line and
 * column positions use a zero-based line index and a column measured in Unicode
 * code points from the start of the line.
 *
 * Language capabilities
 * ---------------------
 * The lexical layer remains the fallback for every language: comments, strings,
 * numbers, keywords, types and brackets. Structural parsing (Tree-sitter) is
 * available only for languages with a bundled grammar; currently Python and
 * JavaScript. It provides a concrete syntax tree, structural highlights and
 * syntax errors. It is not semantic analysis. See docs/status.md.
 *
 * Analysis caching
 * ---------------
 * Document language services reuse one immutable analysis per text version;
 * repeated tokenize/diagnose/folding calls do not re-lex unchanged text.
 * Snapshots hold their own analysis caches.
 *
 * Preview state
 * -------------
 * Proposed text and ghost text are preview state. They do not mutate the
 * document until cc_document_accept_proposal or cc_document_accept_ghost_text
 * succeeds. Normal edits clear both. Read-only documents may display previews
 * but cannot commit them.
 */

#ifndef CODECORE_CODECORE_H
#define CODECORE_CODECORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CODECORE_VERSION_MAJOR 0
#define CODECORE_VERSION_MINOR 5
#define CODECORE_VERSION_PATCH 0
#define CODECORE_ABI_VERSION 6u

#if defined(_WIN32) && defined(CODECORE_BUILD_SHARED)
#define CC_API __declspec(dllexport)
#elif defined(__GNUC__) && defined(CODECORE_BUILD_SHARED)
#define CC_API __attribute__((visibility("default")))
#else
#define CC_API
#endif

typedef struct cc_engine cc_engine;
typedef struct cc_document cc_document;
typedef struct cc_snapshot cc_snapshot;
typedef struct cc_analyzer cc_analyzer;
typedef struct cc_syntax_tree cc_syntax_tree;
typedef struct cc_lsp_client cc_lsp_client;
typedef struct cc_execution cc_execution;

typedef enum cc_status {
  CC_STATUS_OK = 0,
  CC_STATUS_INVALID_ARGUMENT = 1,
  CC_STATUS_OUT_OF_RANGE = 2,
  CC_STATUS_READ_ONLY = 3,
  CC_STATUS_UNKNOWN_LANGUAGE = 4,
  CC_STATUS_OUT_OF_MEMORY = 5,
  CC_STATUS_UNSUPPORTED = 6,
  CC_STATUS_INTERNAL_ERROR = 7,
  CC_STATUS_NOT_RUNNING = 8,
  CC_STATUS_IO_ERROR = 9
} cc_status;

typedef enum cc_change_kind {
  CC_CHANGE_SET_TEXT = 0,
  CC_CHANGE_INSERT = 1,
  CC_CHANGE_ERASE = 2,
  CC_CHANGE_REPLACE = 3,
  CC_CHANGE_UNDO = 4,
  CC_CHANGE_REDO = 5,
  CC_CHANGE_PROPOSAL_ACCEPTED = 6,
  CC_CHANGE_PROPOSAL_REJECTED = 7,
  CC_CHANGE_SNAPSHOT_RESTORED = 8,
  CC_CHANGE_GHOST_TEXT_ACCEPTED = 9,
  CC_CHANGE_GHOST_TEXT_DISMISSED = 10
} cc_change_kind;

typedef enum cc_edit_origin {
  CC_EDIT_ORIGIN_USER = 0,
  CC_EDIT_ORIGIN_AI = 1,
  CC_EDIT_ORIGIN_HOST = 2,
  CC_EDIT_ORIGIN_FORMATTER = 3,
  CC_EDIT_ORIGIN_EXTERNAL_TOOL = 4
} cc_edit_origin;

typedef enum cc_token_kind {
  CC_TOKEN_PLAIN = 0,
  CC_TOKEN_COMMENT = 1,
  CC_TOKEN_KEYWORD = 2,
  CC_TOKEN_TYPE = 3,
  CC_TOKEN_BUILTIN = 4,
  CC_TOKEN_STRING = 5,
  CC_TOKEN_NUMBER = 6,
  CC_TOKEN_OPERATOR = 7,
  CC_TOKEN_PUNCTUATION = 8,
  CC_TOKEN_IDENTIFIER = 9,
  CC_TOKEN_FUNCTION = 10,
  CC_TOKEN_ANNOTATION = 11,
  CC_TOKEN_CONSTANT = 12,
  CC_TOKEN_TAG = 13,
  CC_TOKEN_ATTRIBUTE = 14,
  CC_TOKEN_PROPERTY = 15,
  CC_TOKEN_INVALID = 16
} cc_token_kind;

typedef enum cc_completion_kind {
  CC_COMPLETION_KEYWORD = 0,
  CC_COMPLETION_SYMBOL = 1,
  CC_COMPLETION_FUNCTION = 2,
  CC_COMPLETION_CLASS = 3,
  CC_COMPLETION_TYPE = 4,
  CC_COMPLETION_BUILTIN = 5,
  CC_COMPLETION_PROPERTY = 6
} cc_completion_kind;

typedef enum cc_severity {
  CC_SEVERITY_ERROR = 1,
  CC_SEVERITY_WARNING = 2,
  CC_SEVERITY_INFO = 3,
  CC_SEVERITY_HINT = 4
} cc_severity;

typedef enum cc_cursor_move {
  CC_MOVE_LEFT = 0,
  CC_MOVE_RIGHT = 1,
  CC_MOVE_WORD_LEFT = 2,
  CC_MOVE_WORD_RIGHT = 3,
  CC_MOVE_LINE_START = 4,
  CC_MOVE_LINE_END = 5,
  CC_MOVE_UP = 6,
  CC_MOVE_DOWN = 7,
  CC_MOVE_DOC_START = 8,
  CC_MOVE_DOC_END = 9
} cc_cursor_move;

typedef enum cc_folding_kind {
  CC_FOLDING_BRACE = 0,
  CC_FOLDING_COMMENT = 1
} cc_folding_kind;

#define CC_LANGUAGE_CAP_SYNTAX 0x1u
#define CC_LANGUAGE_CAP_COMPLETION 0x2u
#define CC_LANGUAGE_CAP_DIAGNOSTICS 0x4u
#define CC_LANGUAGE_CAP_FOLDING 0x8u

typedef struct cc_range {
  uint32_t start;
  uint32_t end;
} cc_range;

typedef struct cc_position {
  uint32_t line;
  uint32_t column;
} cc_position;

typedef struct cc_token {
  uint32_t start;
  uint32_t end;
  uint32_t kind;
} cc_token;

typedef struct cc_line_info {
  uint32_t start;
  uint32_t content_end;
  uint32_t end;
} cc_line_info;

typedef struct cc_completion_item {
  const char* label;
  const char* insert_text;
  const char* detail;
  uint32_t kind;
  int32_t score;
} cc_completion_item;

typedef struct cc_diagnostic {
  uint32_t start;
  uint32_t end;
  uint32_t severity;
  const char* message;
  const char* source;
} cc_diagnostic;

typedef struct cc_folding_range {
  uint32_t start_line;
  uint32_t end_line;
  uint32_t kind;
} cc_folding_range;

typedef struct cc_search_options {
  uint32_t struct_size;
  uint8_t case_sensitive;
  uint8_t whole_word;
  /* Zero-initialize the struct before setting fields. */
  uint8_t regex;
} cc_search_options;

typedef struct cc_selection {
  uint32_t anchor;
  uint32_t focus;
} cc_selection;

typedef struct cc_document_options {
  uint32_t struct_size;
  int32_t language_id;
  uint32_t tab_size;
  uint8_t insert_spaces;
  uint8_t read_only;
} cc_document_options;

typedef struct cc_engine_config {
  uint32_t struct_size;
  uint32_t max_document_bytes;
} cc_engine_config;

typedef struct cc_language_info {
  int32_t id;
  const char* id_string;
  const char* display_name;
  const char* extensions;
  uint32_t capabilities;
} cc_language_info;

typedef struct cc_change_event {
  uint64_t version;
  uint32_t kind;
  uint32_t start;
  uint32_t removed_length;
  uint32_t inserted_length;
  uint32_t origin;
} cc_change_event;

typedef enum cc_execution_event_kind {
  CC_EXECUTION_OUTPUT = 0,
  CC_EXECUTION_EXIT = 1
} cc_execution_event_kind;

typedef enum cc_execution_stream {
  CC_EXECUTION_STDOUT = 0,
  CC_EXECUTION_STDERR = 1
} cc_execution_stream;

typedef struct cc_execution_options {
  uint32_t struct_size;
  /* Executable name or path; resolved through PATH when possible. */
  const char* command;
  /* argv[1..]; no program name. Must be NULL when arg_count is zero. */
  const char* const* args;
  size_t arg_count;
  /* NULL or empty inherits the current working directory. */
  const char* working_directory;
  /* Additions/overrides merged into the inherited environment. */
  const char* const* env_keys;
  const char* const* env_values;
  size_t env_count;
  /* 0 disables the timeout. */
  uint32_t timeout_ms;
  /* Soft output backlog bound; 0 uses the default (1 MiB). */
  uint32_t max_queued_bytes;
  /* Runs command verbatim through cmd.exe / sh -c; args must be empty. */
  uint8_t shell;
} cc_execution_options;

typedef struct cc_execution_event {
  uint32_t kind; /* cc_execution_event_kind */
  uint32_t stream; /* cc_execution_stream, output events only */
  uint64_t sequence;
  /* UTF-8 bytes, allocated with cc_alloc. Free with cc_string_free. */
  char* data;
  size_t length;
  int32_t exit_code; /* exit events only; -1 for a signal-free unknown */
  uint8_t cancelled;
  uint8_t timed_out;
} cc_execution_event;

typedef struct cc_diff_line {
  uint32_t kind;
  uint32_t old_line;
  uint32_t new_line;
  const char* text;
} cc_diff_line;

typedef void (*cc_change_listener)(void* user_data, const cc_change_event* event);

/* Library information and memory allocation. */
CC_API uint32_t cc_abi_version(void);
CC_API const char* cc_version_string(void);
CC_API void* cc_alloc(size_t size);
CC_API void cc_free(void* pointer);
CC_API void cc_string_free(char* string);

/* Engine and language registry. */
CC_API cc_engine* cc_engine_create(const cc_engine_config* config);
CC_API void cc_engine_destroy(cc_engine* engine);
CC_API size_t cc_engine_language_count(const cc_engine* engine);
CC_API const cc_language_info* cc_engine_language_at(const cc_engine* engine, size_t index);
CC_API int32_t cc_engine_find_language(const cc_engine* engine, const char* language_id);

/* Document lifecycle and plain text. */
CC_API cc_document* cc_document_create(cc_engine* engine, const cc_document_options* options);
CC_API void cc_document_destroy(cc_document* document);
CC_API cc_status cc_document_set_text(cc_document* document, const char* utf8, size_t length);
CC_API char* cc_document_get_text(const cc_document* document, size_t* out_length);
CC_API size_t cc_document_length(const cc_document* document);
CC_API uint64_t cc_document_version(const cc_document* document);
CC_API int32_t cc_document_language(const cc_document* document);
CC_API cc_status cc_document_set_language(cc_document* document, int32_t language_id);
CC_API uint8_t cc_document_is_read_only(const cc_document* document);
CC_API cc_status cc_document_set_read_only(cc_document* document, uint8_t read_only);
CC_API cc_status cc_document_set_editable(cc_document* document, uint8_t editable);
CC_API uint32_t cc_document_edit_origin(const cc_document* document);
CC_API cc_status cc_document_set_edit_origin(cc_document* document, uint32_t origin);
CC_API uint32_t cc_document_tab_size(const cc_document* document);
CC_API uint8_t cc_document_insert_spaces(const cc_document* document);
CC_API const char* cc_document_last_error(const cc_document* document);

/* Editing. Offsets are UTF-8 byte offsets. */
CC_API cc_status cc_document_insert(cc_document* document, size_t offset, const char* utf8, size_t length);
CC_API cc_status cc_document_erase(cc_document* document, size_t start, size_t end);
CC_API cc_status cc_document_replace(cc_document* document, size_t start, size_t end, const char* utf8, size_t length);

/* Undo and redo. */
CC_API uint8_t cc_document_can_undo(const cc_document* document);
CC_API uint8_t cc_document_can_redo(const cc_document* document);
CC_API cc_status cc_document_undo(cc_document* document);
CC_API cc_status cc_document_redo(cc_document* document);
CC_API void cc_document_clear_history(cc_document* document);
CC_API void cc_document_set_history_limit(cc_document* document, size_t limit);

/* Lines and positions. Lines are 0-based; columns are code points. */
CC_API size_t cc_document_line_count(const cc_document* document);
CC_API cc_status cc_document_line_at(const cc_document* document, size_t line_index, cc_line_info* out_info);
CC_API cc_status cc_document_offset_to_position(const cc_document* document, size_t offset, cc_position* out_position);
CC_API cc_status cc_document_position_to_offset(const cc_document* document, uint32_t line, uint32_t column, size_t* out_offset);

/* Selection and cursor. */
CC_API cc_selection cc_document_get_selection(const cc_document* document);
CC_API cc_status cc_document_set_selection(cc_document* document, uint32_t anchor, uint32_t focus);
CC_API cc_status cc_document_move_cursor(cc_document* document, uint32_t move, uint8_t extend_selection);

/* Multi-cursor. Index 0 is the primary selection. */
CC_API size_t cc_document_cursor_count(const cc_document* document);
CC_API cc_status cc_document_cursor_at(const cc_document* document, size_t index,
                                       cc_selection* out_selection);
CC_API cc_status cc_document_add_cursor(cc_document* document, uint32_t anchor, uint32_t focus);
CC_API void cc_document_clear_cursors(cc_document* document);
CC_API cc_status cc_document_move_cursors(cc_document* document, uint32_t move,
                                          uint8_t extend_selection);
/* Replaces every selection with utf8 as one undoable transaction. */
CC_API cc_status cc_document_multi_insert(cc_document* document, const char* utf8, size_t length);
/* Erases every selection, or one code point per collapsed caret. */
CC_API cc_status cc_document_multi_erase(cc_document* document, uint8_t backward);
CC_API cc_status cc_document_match_bracket(const cc_document* document, size_t offset, size_t* out_match);

/* Language services. Returned arrays must be freed by the caller. */
CC_API size_t cc_document_tokenize(const cc_document* document, size_t start, size_t end, cc_token** out_tokens);
CC_API void cc_tokens_free(cc_token* tokens);
CC_API size_t cc_document_complete(const cc_document* document, size_t offset, cc_completion_item** out_items);
CC_API void cc_completions_free(cc_completion_item* items, size_t count);
CC_API size_t cc_document_diagnose(const cc_document* document, cc_diagnostic** out_diagnostics);
CC_API void cc_diagnostics_free(cc_diagnostic* diagnostics, size_t count);
CC_API size_t cc_document_folding_ranges(const cc_document* document, cc_folding_range** out_ranges);
CC_API void cc_folding_ranges_free(cc_folding_range* ranges);
/* Literal find. Kept for compatibility; prefer cc_document_search. */
CC_API size_t cc_document_find(const cc_document* document, const char* query, size_t query_length,
                               const cc_search_options* options, cc_range** out_ranges);
/* Search with status reporting. options->regex selects ECMAScript syntax.
 * An invalid pattern returns CC_STATUS_INVALID_ARGUMENT and no ranges. */
CC_API cc_status cc_document_search(const cc_document* document, const char* query,
                                    size_t query_length, const cc_search_options* options,
                                    cc_range** out_ranges, size_t* out_count);
CC_API void cc_search_ranges_free(cc_range* ranges);

/* Proposed changes. Preview text never mutates the document. */
CC_API cc_status cc_document_set_proposed_text(cc_document* document, const char* utf8,
                                               size_t length, uint32_t origin);
CC_API uint8_t cc_document_has_proposal(const cc_document* document);
CC_API char* cc_document_get_proposed_text(const cc_document* document, size_t* out_length);
CC_API size_t cc_document_proposal_diff(const cc_document* document, cc_diff_line** out_lines);
CC_API void cc_diff_lines_free(cc_diff_line* lines, size_t count);
CC_API cc_status cc_document_accept_proposal(cc_document* document);
CC_API cc_status cc_document_reject_proposal(cc_document* document);

/* Ghost text. The suggestion is not inserted until accepted. */
CC_API cc_status cc_document_set_ghost_text(cc_document* document, size_t offset,
                                            const char* utf8, size_t length, uint32_t origin);
CC_API char* cc_document_get_ghost_text(const cc_document* document, size_t* out_offset,
                                        size_t* out_length, uint32_t* out_origin);
CC_API cc_status cc_document_accept_ghost_text(cc_document* document, size_t max_code_points);
CC_API cc_status cc_document_dismiss_ghost_text(cc_document* document);

/* Immutable snapshots. Queries are thread-safe for a given snapshot. */
CC_API cc_snapshot* cc_document_create_snapshot(const cc_document* document);
CC_API void cc_snapshot_destroy(cc_snapshot* snapshot);
CC_API uint64_t cc_snapshot_version(const cc_snapshot* snapshot);
CC_API size_t cc_snapshot_length(const cc_snapshot* snapshot);
CC_API int32_t cc_snapshot_language(const cc_snapshot* snapshot);
CC_API char* cc_snapshot_get_text(const cc_snapshot* snapshot, size_t* out_length);
CC_API size_t cc_snapshot_tokenize(const cc_snapshot* snapshot, size_t start, size_t end,
                                  cc_token** out_tokens);
CC_API size_t cc_snapshot_complete(const cc_snapshot* snapshot, size_t offset,
                                   cc_completion_item** out_items);
CC_API size_t cc_snapshot_diagnose(const cc_snapshot* snapshot,
                                   cc_diagnostic** out_diagnostics);
CC_API size_t cc_snapshot_folding_ranges(const cc_snapshot* snapshot,
                                         cc_folding_range** out_ranges);
CC_API size_t cc_snapshot_find(const cc_snapshot* snapshot, const char* query,
                               size_t query_length, const cc_search_options* options,
                               cc_range** out_ranges);
CC_API cc_status cc_snapshot_search(const cc_snapshot* snapshot, const char* query,
                                    size_t query_length, const cc_search_options* options,
                                    cc_range** out_ranges, size_t* out_count);
CC_API cc_status cc_document_restore_snapshot(cc_document* document,
                                             const cc_snapshot* snapshot);

/*
 * Background analysis.
 *
 * submit() captures an immutable snapshot and queues it on one worker thread.
 * The newest submission wins; a stale result is discarded. wait() is optional
 * and intended for tests/shutdown, not the UI thread. take_result() returns a
 * snapshot owned by the caller (destroy with cc_snapshot_destroy) or NULL when
 * no fresh result is available.
 */
CC_API cc_analyzer* cc_analyzer_create(void);
CC_API void cc_analyzer_destroy(cc_analyzer* analyzer);
CC_API cc_status cc_analyzer_submit(cc_analyzer* analyzer, const cc_document* document);
CC_API cc_snapshot* cc_analyzer_take_result(cc_analyzer* analyzer);
CC_API void cc_analyzer_wait(cc_analyzer* analyzer);
CC_API void cc_analyzer_cancel(cc_analyzer* analyzer);
CC_API size_t cc_analyzer_dropped(const cc_analyzer* analyzer);
CC_API size_t cc_analyzer_completed(const cc_analyzer* analyzer);

/*
 * Local process execution.
 *
 * CodeCore ships no runtime; the host supplies the executable and arguments.
 * Each handle owns one child process, its pipes, and internal reader/waiter
 * threads. Calls are safe from one host thread. Output is polled, not pushed
 * through callbacks. stdout/stderr byte order is preserved per stream; the
 * cross-stream interleaving is enqueue order.
 *
 * start() returns NULL on failure and allocates *out_error (free with
 * cc_string_free). Output event data is allocated with cc_alloc and must be
 * freed with cc_string_free. The final EXIT event is delivered only after all
 * queued output.
 *
 * On Android, executing files from app data is blocked by platform policy;
 * runtimes must be packaged as native libraries. CodeCore does not bundle one.
 */
CC_API uint8_t cc_execution_supported(void);
CC_API cc_execution* cc_execution_start(const cc_execution_options* options,
                                         char** out_error);
CC_API uint8_t cc_execution_poll(cc_execution* execution,
                                  cc_execution_event* out_event);
CC_API cc_status cc_execution_write(cc_execution* execution, const char* data,
                                     size_t length);
CC_API void cc_execution_close_stdin(cc_execution* execution);
CC_API void cc_execution_cancel(cc_execution* execution);
/* timeout_ms == 0 is a non-blocking state check. */
CC_API uint8_t cc_execution_wait(cc_execution* execution, uint32_t timeout_ms);
CC_API uint8_t cc_execution_is_running(const cc_execution* execution);
CC_API int32_t cc_execution_exit_code(const cc_execution* execution);
CC_API void cc_execution_destroy(cc_execution* execution);

/*
 * Structural syntax (Tree-sitter).
 *
 * cc_document_syntax_tree parses the current version incrementally from the
 * previous tree and returns an owned tree handle. The handle is immutable;
 * highlights and errors are snapshots of that version. Strings inside the
 * returned structs are owned by the tree and stay valid until
 * cc_syntax_tree_destroy.
 *
 * Only languages with a bundled grammar are supported; check
 * cc_syntax_language_supported before relying on the result. The lexical
 * tokenizer remains available for every other language.
 */
typedef struct cc_syntax_highlight {
  uint32_t start;
  uint32_t end;
  uint32_t kind; /* cc_token_kind */
  const char* capture; /* e.g. "keyword", "function", "string" */
} cc_syntax_highlight;

typedef struct cc_syntax_error {
  uint32_t start;
  uint32_t end;
  uint32_t start_row;
  uint32_t start_column; /* bytes, Tree-sitter convention */
  uint32_t end_row;
  uint32_t end_column;
  uint8_t missing;
  const char* node_type;
} cc_syntax_error;

CC_API uint8_t cc_syntax_language_supported(const char* language_id);
CC_API cc_syntax_tree* cc_document_syntax_tree(cc_document* document);
CC_API void cc_syntax_tree_destroy(cc_syntax_tree* tree);
CC_API const char* cc_syntax_tree_language(const cc_syntax_tree* tree);
CC_API const char* cc_syntax_tree_root_type(const cc_syntax_tree* tree);
CC_API size_t cc_syntax_tree_node_count(const cc_syntax_tree* tree);
CC_API uint8_t cc_syntax_tree_has_errors(const cc_syntax_tree* tree);
CC_API size_t cc_syntax_tree_highlight_count(const cc_syntax_tree* tree);
CC_API uint8_t cc_syntax_tree_highlight_at(const cc_syntax_tree* tree, size_t index,
                                           cc_syntax_highlight* out_highlight);
CC_API size_t cc_syntax_tree_error_count(const cc_syntax_tree* tree);
CC_API uint8_t cc_syntax_tree_error_at(const cc_syntax_tree* tree, size_t index,
                                       cc_syntax_error* out_error);

/* Visible-line geometry for viewport renderers (end_line is exclusive). */
typedef struct cc_viewport_range {
  uint32_t first_line;
  uint32_t end_line;
} cc_viewport_range;

CC_API cc_status cc_viewport_visible_range(uint32_t line_count, uint32_t scroll_y,
                                           uint32_t viewport_height, uint32_t line_height,
                                           uint32_t overscan_lines,
                                           cc_viewport_range* out_range);

/* Change notifications. */
CC_API void cc_document_set_change_listener(cc_document* document, cc_change_listener listener, void* user_data);

/*
 * LSP client
 * ----------
 * The host owns the transport. Drain cc_lsp_take_outgoing and write those bytes
 * to the language server, then pass received bytes to cc_lsp_feed. The client
 * speaks the LSP base protocol (Content-Length framing) and tracks request ids.
 *
 * Positions use LSP's UTF-16 line/character coordinates, not CodeCore byte
 * offsets. The host converts between the two.
 *
 * Strings returned by take_result and take_diagnostics are allocated with
 * cc_alloc and must be released with cc_string_free. The capability and error
 * pointers are owned by the client and stay valid until the next receive.
 */
CC_API cc_lsp_client* cc_lsp_create(void);
CC_API void cc_lsp_destroy(cc_lsp_client* client);
CC_API void cc_lsp_initialize(cc_lsp_client* client, const char* root_uri);
CC_API void cc_lsp_feed(cc_lsp_client* client, const char* data, size_t length);
CC_API char* cc_lsp_take_outgoing(cc_lsp_client* client, size_t* out_length);
CC_API uint8_t cc_lsp_take_result(cc_lsp_client* client, int64_t* out_id, char** out_method,
                                  char** out_payload, uint8_t* out_is_error);
CC_API uint8_t cc_lsp_take_diagnostics(cc_lsp_client* client, char** out_uri,
                                       char** out_payload);
CC_API uint8_t cc_lsp_is_initialized(const cc_lsp_client* client);
CC_API const char* cc_lsp_server_capabilities(const cc_lsp_client* client);
CC_API cc_status cc_lsp_did_open(cc_lsp_client* client, const char* uri,
                                 const char* language_id, int32_t version, const char* text,
                                 size_t text_length);
CC_API cc_status cc_lsp_did_change(cc_lsp_client* client, const char* uri, int32_t version,
                                   const char* text, size_t text_length);
CC_API cc_status cc_lsp_did_close(cc_lsp_client* client, const char* uri);
CC_API cc_status cc_lsp_did_save(cc_lsp_client* client, const char* uri, const char* text,
                                 size_t text_length, uint8_t include_text);
CC_API int64_t cc_lsp_completion(cc_lsp_client* client, const char* uri, uint32_t line,
                                 uint32_t character);
CC_API int64_t cc_lsp_hover(cc_lsp_client* client, const char* uri, uint32_t line,
                            uint32_t character);
CC_API int64_t cc_lsp_definition(cc_lsp_client* client, const char* uri, uint32_t line,
                                 uint32_t character);
CC_API int64_t cc_lsp_signature_help(cc_lsp_client* client, const char* uri, uint32_t line,
                                     uint32_t character);
CC_API int64_t cc_lsp_references(cc_lsp_client* client, const char* uri, uint32_t line,
                                 uint32_t character, uint8_t include_declaration);
CC_API int64_t cc_lsp_document_symbols(cc_lsp_client* client, const char* uri);
CC_API int64_t cc_lsp_workspace_symbols(cc_lsp_client* client, const char* query);
CC_API int64_t cc_lsp_rename(cc_lsp_client* client, const char* uri, uint32_t line,
                             uint32_t character, const char* new_name);
CC_API int64_t cc_lsp_formatting(cc_lsp_client* client, const char* uri, uint32_t tab_size,
                                 uint8_t insert_spaces);
CC_API int64_t cc_lsp_code_action(cc_lsp_client* client, const char* uri, uint32_t start_line,
                                  uint32_t start_character, uint32_t end_line,
                                  uint32_t end_character);
CC_API int64_t cc_lsp_semantic_tokens(cc_lsp_client* client, const char* uri);
/* Generic request. params_json must contain a JSON value; returns the request id. */
CC_API int64_t cc_lsp_request(cc_lsp_client* client, const char* method,
                              const char* params_json, size_t params_length);
/* Generic notification. params_json must contain a JSON value. */
CC_API cc_status cc_lsp_notify(cc_lsp_client* client, const char* method,
                               const char* params_json, size_t params_length);
CC_API void cc_lsp_cancel(cc_lsp_client* client, int64_t id);
CC_API void cc_lsp_shutdown(cc_lsp_client* client);
CC_API void cc_lsp_exit(cc_lsp_client* client);
CC_API const char* cc_lsp_last_error(const cc_lsp_client* client);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CODECORE_CODECORE_H */
