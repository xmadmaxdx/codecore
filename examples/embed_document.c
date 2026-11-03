/*
 * Minimal host integration example.
 *
 * Build against a release package:
 *   cc embed_document.c -Iinclude -Llib -lcodecore -o embed_document
 *   ./embed_document
 *
 * Windows (MinGW):
 *   gcc embed_document.c -Iinclude -Llib -lcodecore -o embed_document.exe
 */
#include <codecore/codecore.h>

#include <stdio.h>
#include <string.h>

int main(void) {
  printf("CodeCore %s (ABI %u)\n", cc_version_string(), cc_abi_version());

  cc_engine* engine = cc_engine_create(NULL);
  cc_document* document = cc_document_create(engine, NULL); /* defaults to Python */

  const char* source = "def total(a, b):\n    return a + b\n\nprint(total(1, 2))\n";
  if (cc_document_set_text(document, source, strlen(source)) != CC_STATUS_OK) {
    printf("set_text failed: %s\n", cc_document_last_error(document));
    cc_document_destroy(document);
    cc_engine_destroy(engine);
    return 1;
  }

  cc_token* tokens = NULL;
  const size_t count = cc_document_tokenize(document, 0, cc_document_length(document), &tokens);
  printf("%zu tokens, %zu lines\n", count, cc_document_line_count(document));
  cc_tokens_free(tokens);

  cc_completion_item* items = NULL;
  const size_t completions = cc_document_complete(document, 12, &items);
  printf("%zu completions at offset 12\n", completions);
  cc_completions_free(items, completions);

  cc_diagnostics_free(NULL, 0);

  cc_document_destroy(document);
  cc_engine_destroy(engine);
  return 0;
}
