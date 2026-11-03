#pragma once

#include <cstdint>
#include <deque>
#include <map>
#include <string>
#include <string_view>

#include "core/lsp/json.h"
#include "core/lsp/protocol.h"

namespace codecore::lsp {

struct LspResult {
  int64_t id = 0;
  std::string method;
  std::string payload;
  bool error = false;
};

struct LspDiagnostics {
  std::string uri;
  std::string payload;
};

/*
 * Transport-agnostic LSP client.
 *
 * The host owns the connection, process or socket. It drains takeOutgoing()
 * and writes those bytes to the server, then passes received bytes to
 * receive(). Requests are tracked by id; completed responses are exposed
 * through takeResult(), and publishDiagnostics notifications through
 * takeDiagnostics().
 *
 * Document sync is full-text (TextDocumentSyncKind.Full). Positions passed to
 * completion(), hover() and definition() use LSP's UTF-16 line/character
 * coordinates; the host is responsible for converting CodeCore byte offsets.
 */
class LspClient {
 public:
  void initialize(std::string rootUri);
  bool isInitialized() const { return initialized_; }
  const std::string& serverCapabilities() const { return capabilitiesJson_; }

  void shutdown();
  void exit();
  void reset();

  void didOpen(std::string uri, std::string languageId, int32_t version, std::string text);
  void didChange(std::string uri, int32_t version, std::string text);
  void didClose(std::string uri);
  void didSave(std::string uri, std::string text, bool includeText);

  int64_t completion(std::string uri, uint32_t line, uint32_t character);
  int64_t hover(std::string uri, uint32_t line, uint32_t character);
  int64_t definition(std::string uri, uint32_t line, uint32_t character);
  int64_t signatureHelp(std::string uri, uint32_t line, uint32_t character);
  int64_t references(std::string uri, uint32_t line, uint32_t character,
                     bool includeDeclaration);
  int64_t documentSymbols(std::string uri);
  int64_t workspaceSymbols(std::string query);
  int64_t rename(std::string uri, uint32_t line, uint32_t character, std::string newName);
  int64_t formatting(std::string uri, uint32_t tabSize, bool insertSpaces);
  int64_t codeAction(std::string uri, uint32_t startLine, uint32_t startCharacter,
                     uint32_t endLine, uint32_t endCharacter);
  int64_t semanticTokens(std::string uri);
  int64_t requestJson(std::string method, std::string_view paramsJson, std::string* error);
  bool notifyJson(std::string method, std::string_view paramsJson, std::string* error);
  void cancel(int64_t id);

  void receive(std::string_view bytes);
  std::string takeOutgoing();
  bool takeResult(LspResult* out);
  bool takeDiagnostics(LspDiagnostics* out);

  size_t pendingRequests() const { return pending_.size(); }
  const std::string& lastError() const { return lastError_; }

 private:
  void enqueue(JsonValue message);
  int64_t request(std::string method, JsonValue params);
  void handleMessage(const LspMessage& message);
  void handleResponse(const LspMessage& message);
  void handleServerRequest(const LspMessage& message);
  void handleNotification(const LspMessage& message);

  std::deque<std::string> outgoing_;
  std::deque<LspResult> results_;
  std::deque<LspDiagnostics> diagnostics_;
  std::map<int64_t, std::string> pending_;
  FrameParser parser_;
  std::string rootUri_;
  std::string capabilitiesJson_ = "{}";
  std::string lastError_;
  int64_t nextId_ = 1;
  int64_t initializeId_ = 0;
  bool initialized_ = false;
};

}  // namespace codecore::lsp
