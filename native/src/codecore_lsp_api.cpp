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

uint8_t cc_lsp_take_result(cc_lsp_client* client, char** out_method, char** out_payload) {
  if (out_method) {
    *out_method = nullptr;
  }
  if (out_payload) {
    *out_payload = nullptr;
  }
  if (!client || !out_method || !out_payload) {
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
  *out_method = method;
  *out_payload = payload;
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
