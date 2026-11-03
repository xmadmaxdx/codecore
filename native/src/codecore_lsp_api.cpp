#include "codecore/codecore.h"

#include <cstring>
#include <new>
#include <string>
#include <utility>

#include "core/lsp/client.h"

struct cc_lsp_client {
  codecore::lsp::LspClient client;
};

namespace {

char* copyString(std::string_view value) {
  auto* memory = static_cast<char*>(cc_alloc(value.size() + 1));
  if (!memory) {
    return nullptr;
  }
  if (!value.empty()) {
    std::memcpy(memory, value.data(), value.size());
  }
  memory[value.size()] = '\0';
  return memory;
}

}  // namespace

extern "C" {

cc_lsp_client* cc_lsp_create(void) {
  return new (std::nothrow) cc_lsp_client();
}

void cc_lsp_destroy(cc_lsp_client* client) { delete client; }

void cc_lsp_initialize(cc_lsp_client* client, const char* root_uri) {
  if (!client) {
    return;
  }
  client->client.initialize(root_uri ? root_uri : "");
}

void cc_lsp_feed(cc_lsp_client* client, const char* data, size_t length) {
  if (!client || (!data && length > 0)) {
    return;
  }
  client->client.receive(std::string_view(data ? data : "", length));
}

char* cc_lsp_take_outgoing(cc_lsp_client* client, size_t* out_length) {
  if (out_length) {
    *out_length = 0;
  }
  if (!client) {
    return nullptr;
  }
  const std::string frame = client->client.takeOutgoing();
  if (frame.empty()) {
    return nullptr;
  }
  if (out_length) {
    *out_length = frame.size();
  }
  return copyString(frame);
}

uint8_t cc_lsp_take_result(cc_lsp_client* client, int64_t* out_id, char** out_method,
                           char** out_payload, uint8_t* out_is_error) {
  if (out_id) {
    *out_id = 0;
  }
  if (out_is_error) {
    *out_is_error = 0;
  }
  if (out_method) {
    *out_method = nullptr;
  }
  if (out_payload) {
    *out_payload = nullptr;
  }
  if (!client || !out_id || !out_method || !out_payload) {
    return 0;
  }
  codecore::lsp::LspResult result;
  if (!client->client.takeResult(&result)) {
    return 0;
  }
  char* method = copyString(result.method);
  char* payload = copyString(result.payload);
  if (!method || !payload) {
    cc_string_free(method);
    cc_string_free(payload);
    return 0;
  }
  *out_id = result.id;
  *out_method = method;
  *out_payload = payload;
  if (out_is_error) {
    *out_is_error = result.error ? 1 : 0;
  }
  return 1;
}

uint8_t cc_lsp_take_diagnostics(cc_lsp_client* client, char** out_uri,
                                char** out_payload) {
  if (out_uri) {
    *out_uri = nullptr;
  }
  if (out_payload) {
    *out_payload = nullptr;
  }
  if (!client || !out_uri || !out_payload) {
    return 0;
  }
  codecore::lsp::LspDiagnostics report;
  if (!client->client.takeDiagnostics(&report)) {
    return 0;
  }
  char* uri = copyString(report.uri);
  char* payload = copyString(report.payload);
  if (!uri || !payload) {
    cc_string_free(uri);
    cc_string_free(payload);
    return 0;
  }
  *out_uri = uri;
  *out_payload = payload;
  return 1;
}

uint8_t cc_lsp_is_initialized(const cc_lsp_client* client) {
  return client && client->client.isInitialized() ? 1 : 0;
}

const char* cc_lsp_server_capabilities(const cc_lsp_client* client) {
  return client ? client->client.serverCapabilities().c_str() : "";
}

cc_status cc_lsp_did_open(cc_lsp_client* client, const char* uri, const char* language_id,
                          int32_t version, const char* text, size_t text_length) {
  if (!client || !uri || !language_id || (!text && text_length > 0)) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  client->client.didOpen(uri, language_id, version,
                         std::string(text ? text : "", text_length));
  return CC_STATUS_OK;
}

cc_status cc_lsp_did_change(cc_lsp_client* client, const char* uri, int32_t version,
                            const char* text, size_t text_length) {
  if (!client || !uri || (!text && text_length > 0)) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  client->client.didChange(uri, version, std::string(text ? text : "", text_length));
  return CC_STATUS_OK;
}

cc_status cc_lsp_did_close(cc_lsp_client* client, const char* uri) {
  if (!client || !uri) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  client->client.didClose(uri);
  return CC_STATUS_OK;
}

cc_status cc_lsp_did_save(cc_lsp_client* client, const char* uri, const char* text,
                          size_t text_length, uint8_t include_text) {
  if (!client || !uri || (!text && text_length > 0)) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  client->client.didSave(uri, std::string(text ? text : "", text_length),
                         include_text != 0);
  return CC_STATUS_OK;
}

int64_t cc_lsp_completion(cc_lsp_client* client, const char* uri, uint32_t line,
                          uint32_t character) {
  if (!client || !uri) {
    return -1;
  }
  return client->client.completion(uri, line, character);
}

int64_t cc_lsp_hover(cc_lsp_client* client, const char* uri, uint32_t line,
                     uint32_t character) {
  if (!client || !uri) {
    return -1;
  }
  return client->client.hover(uri, line, character);
}

int64_t cc_lsp_definition(cc_lsp_client* client, const char* uri, uint32_t line,
                          uint32_t character) {
  if (!client || !uri) {
    return -1;
  }
  return client->client.definition(uri, line, character);
}

int64_t cc_lsp_signature_help(cc_lsp_client* client, const char* uri, uint32_t line,
                              uint32_t character) {
  if (!client || !uri) {
    return -1;
  }
  return client->client.signatureHelp(uri, line, character);
}

int64_t cc_lsp_references(cc_lsp_client* client, const char* uri, uint32_t line,
                          uint32_t character, uint8_t include_declaration) {
  if (!client || !uri) {
    return -1;
  }
  return client->client.references(uri, line, character, include_declaration != 0);
}

int64_t cc_lsp_document_symbols(cc_lsp_client* client, const char* uri) {
  if (!client || !uri) {
    return -1;
  }
  return client->client.documentSymbols(uri);
}

int64_t cc_lsp_workspace_symbols(cc_lsp_client* client, const char* query) {
  if (!client || !query) {
    return -1;
  }
  return client->client.workspaceSymbols(query);
}

int64_t cc_lsp_rename(cc_lsp_client* client, const char* uri, uint32_t line,
                      uint32_t character, const char* new_name) {
  if (!client || !uri || !new_name) {
    return -1;
  }
  return client->client.rename(uri, line, character, new_name);
}

int64_t cc_lsp_formatting(cc_lsp_client* client, const char* uri, uint32_t tab_size,
                          uint8_t insert_spaces) {
  if (!client || !uri) {
    return -1;
  }
  return client->client.formatting(uri, tab_size, insert_spaces != 0);
}

int64_t cc_lsp_code_action(cc_lsp_client* client, const char* uri, uint32_t start_line,
                           uint32_t start_character, uint32_t end_line,
                           uint32_t end_character) {
  if (!client || !uri) {
    return -1;
  }
  return client->client.codeAction(uri, start_line, start_character, end_line,
                                   end_character);
}

int64_t cc_lsp_semantic_tokens(cc_lsp_client* client, const char* uri) {
  if (!client || !uri) {
    return -1;
  }
  return client->client.semanticTokens(uri);
}

int64_t cc_lsp_request(cc_lsp_client* client, const char* method, const char* params_json,
                       size_t params_length) {
  if (!client || !method || (!params_json && params_length > 0)) {
    return -1;
  }
  std::string error;
  return client->client.requestJson(method,
                                    std::string_view(params_json ? params_json : "",
                                                     params_length),
                                    &error);
}

cc_status cc_lsp_notify(cc_lsp_client* client, const char* method, const char* params_json,
                        size_t params_length) {
  if (!client || !method || (!params_json && params_length > 0)) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  std::string error;
  const bool ok = client->client.notifyJson(
      method,
      std::string_view(params_json ? params_json : "", params_length), &error);
  return ok ? CC_STATUS_OK : CC_STATUS_INVALID_ARGUMENT;
}

void cc_lsp_cancel(cc_lsp_client* client, int64_t id) {
  if (client) {
    client->client.cancel(id);
  }
}

void cc_lsp_shutdown(cc_lsp_client* client) {
  if (client) {
    client->client.shutdown();
  }
}

void cc_lsp_exit(cc_lsp_client* client) {
  if (client) {
    client->client.exit();
  }
}

const char* cc_lsp_last_error(const cc_lsp_client* client) {
  return client ? client->client.lastError().c_str() : "";
}

}  // extern "C"
