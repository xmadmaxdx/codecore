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
 * the subject of concurrent long-running batch work in v0.1. Change listeners
 * are invoked synchronously on the mutating thread.
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
 * v0.1 ships a lexical (Level 1) language layer: comments, strings, numbers,
 * keywords, types and brackets. It is *not* Tree-sitter and makes no semantic
 * claims. See docs/status.md.
 */

#ifndef CODECORE_CODECORE_H
#define CODECORE_CODECORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define CODECORE_VERSION_MAJOR 0
#define CODECORE_VERSION_MINOR 1
#define CODECORE_VERSION_PATCH 0
#define CODECORE_ABI_VERSION 1u

#if defined(_WIN32) && defined(CODECORE_BUILD_SHARED)
#define CC_API __declspec(dllexport)
#elif defined(__GNUC__) && defined(CODECORE_BUILD_SHARED)
#define CC_API __attribute__((visibility("default")))
#else
#define CC_API
#endif

typedef struct cc_engine cc_engine;
typedef struct cc_document cc_document;

typedef enum cc_status {
  CC_STATUS_OK = 0,
  CC_STATUS_INVALID_ARGUMENT = 1,
  CC_STATUS_OUT_OF_RANGE = 2,
  CC_STATUS_READ_ONLY = 3,
  CC_STATUS_UNKNOWN_LANGUAGE = 4,
  CC_STATUS_OUT_OF_MEMORY = 5,
  CC_STATUS_UNSUPPORTED = 6,
  CC_STATUS_INTERNAL_ERROR = 7
} cc_status;

typedef enum cc_change_kind {
  CC_CHANGE_SET_TEXT = 0,
  CC_CHANGE_INSERT = 1,
  CC_CHANGE_ERASE = 2,
  CC_CHANGE_REPLACE = 3,
  CC_CHANGE_UNDO = 4,
  CC_CHANGE_REDO = 5
} cc_change_kind;

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
} cc_change_event;

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
CC_API size_t cc_document_find(const cc_document* document, const char* query, size_t query_length,
                               const cc_search_options* options, cc_range** out_ranges);
CC_API void cc_search_ranges_free(cc_range* ranges);

/* Change notifications. */
CC_API void cc_document_set_change_listener(cc_document* document, cc_change_listener listener, void* user_data);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* CODECORE_CODECORE_H */
