# C ABI guide

Header: `native/include/codecore/codecore.h`

The ABI is versioned with `cc_abi_version()` (currently `1`) and
`cc_version_string()` (`0.1.0`).

## Lifecycle

```c
cc_engine* engine = cc_engine_create(NULL);       /* default: 64 MB documents */
cc_document* doc = cc_document_create(engine, NULL); /* defaults to Python */

/* ... */

cc_document_destroy(doc);
cc_engine_destroy(engine);
```

A document holds its own shared reference to its language, so it remains usable
after the engine is destroyed. Documents are not automatically destroyed with
the engine; the caller must still free them.

## Ownership

| Returned value | Release with |
|---|---|
| `char*` from `cc_document_get_text` | `cc_string_free` |
| `cc_token*` from `cc_document_tokenize` | `cc_tokens_free` |
| `cc_completion_item*` from `cc_document_complete` | `cc_completions_free(items, count)` |
| `cc_diagnostic*` from `cc_document_diagnose` | `cc_diagnostics_free(items, count)` |
| `cc_folding_range*` | `cc_folding_ranges_free` |
| `cc_range*` from `cc_document_find` | `cc_search_ranges_free` |
| `cc_language_info*` from `cc_engine_language_at` | Not freed; engine-owned |

Empty result sets return `0` and set the out-pointer to `NULL`.

Inputs are copied before a function returns. No input pointer is retained.

## Status codes

`cc_status` values:

- `CC_STATUS_OK`
- `CC_STATUS_INVALID_ARGUMENT` — null pointer, invalid UTF-8, code-point split.
- `CC_STATUS_OUT_OF_RANGE` — offset/range outside the document or size limit.
- `CC_STATUS_READ_ONLY` — mutation attempted on a read-only document.
- `CC_STATUS_UNKNOWN_LANGUAGE` — unknown language id.
- `CC_STATUS_UNSUPPORTED` — no matching bracket, for example.
- `CC_STATUS_OUT_OF_MEMORY`, `CC_STATUS_INTERNAL_ERROR`.

On failure, `cc_document_last_error` returns a message owned by the document;
it stays valid until the next call that changes the error state.

## Text and coordinates

- All offsets are UTF-8 byte offsets.
- Offsets on edit calls must be at code-point boundaries; insertion of invalid
  UTF-8 is rejected.
- Line indexes are zero-based.
- `cc_position.column` counts Unicode code points, not bytes or display cells.
- `cc_position_to_offset` clamps a column past the end of a line to that line's
  content end. It does not include the line ending.

## Example: tokenize and complete

```c
size_t token_count = 0;
cc_token* tokens = NULL;
token_count = cc_document_tokenize(doc, 0, cc_document_length(doc), &tokens);
for (size_t i = 0; i < token_count; ++i) {
  printf("%u..%u kind=%u\n", tokens[i].start, tokens[i].end, tokens[i].kind);
}
cc_tokens_free(tokens);

cc_completion_item* items = NULL;
size_t item_count = cc_document_complete(doc, cc_document_length(doc), &items);
cc_completions_free(items, item_count);
```

## Language capabilities

`cc_language_info.capabilities` is a bit mask:

- `CC_LANGUAGE_CAP_SYNTAX` — lexical tokens.
- `CC_LANGUAGE_CAP_COMPLETION` — local completion.
- `CC_LANGUAGE_CAP_DIAGNOSTICS` — bracket/lexical diagnostics.
- `CC_LANGUAGE_CAP_FOLDING` — brace/comment folding.

There is no semantic-LSP flag in v0.1. Do not infer LSP support from these bits.

## Change events

```c
static void on_change(void* user, const cc_change_event* event) {
  printf("v%llu kind=%u start=%u -%u +%u\n",
         (unsigned long long)event->version, event->kind, event->start,
         event->removed_length, event->inserted_length);
}

cc_document_set_change_listener(doc, on_change, my_state);
```

The listener runs synchronously on the mutating thread while the document is
locked. It must not destroy the document or block. Query methods may be called
reentrantly from the same thread because the core uses a recursive mutex, but
this is not a general invitation to call arbitrary APIs from the callback.

## Threading

- The engine's language metadata is immutable and can be read concurrently.
- Calls on one document are serialized internally.
- The C ABI does not start threads in v0.1.

## Limits

- Default maximum document size: 64 MB, configurable through
  `cc_engine_config.max_document_bytes`.
- Default history depth: 200 edits; 0 disables undo.
- Completion is capped at 100 entries.
- Find is capped at 10,000 matches.
- Diagnostics are capped at 200 entries.
- Folding ranges are capped at 500 entries.
