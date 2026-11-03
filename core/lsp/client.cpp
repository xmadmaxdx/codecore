#include "core/lsp/client.h"

#include <cstdint>
#include <string>
#include <utility>

namespace codecore::lsp {
namespace {

JsonValue object() { return JsonValue::makeObject(); }

JsonValue positionParams(const std::string& uri, uint32_t line, uint32_t character) {
  JsonValue params = object();
  JsonValue textDocument = object();
  textDocument.set("uri", JsonValue(uri));
  params.set("textDocument", std::move(textDocument));
  JsonValue position = object();
  position.set("line", JsonValue(static_cast<int>(line)));
  position.set("character", JsonValue(static_cast<int>(character)));
  params.set("position", std::move(position));
  return params;
}

}  // namespace

void LspClient::enqueue(JsonValue message) {
  message.set("jsonrpc", JsonValue("2.0"));
  outgoing_.push_back(encodeFrame(message.dump()));
}

int64_t LspClient::request(std::string method, JsonValue params) {
  const int64_t id = nextId_++;
  JsonValue message = object();
  message.set("id", JsonValue(static_cast<int>(id)));
  message.set("method", JsonValue(method));
  message.set("params", std::move(params));
  pending_[id] = method;
  enqueue(std::move(message));
  return id;
}

void LspClient::initialize(std::string rootUri) {
  if (initializeId_ != 0) {
    return;
  }
  rootUri_ = std::move(rootUri);

  JsonValue textDocument = object();
  JsonValue synchronization = object();
  synchronization.set("dynamicRegistration", JsonValue(false));
  synchronization.set("didSave", JsonValue(false));
  textDocument.set("synchronization", std::move(synchronization));

  JsonValue completion = object();
  JsonValue completionItem = object();
  completionItem.set("snippetSupport", JsonValue(false));
  completion.set("completionItem", std::move(completionItem));
  textDocument.set("completion", std::move(completion));

  JsonValue hover = object();
  JsonValue formats = JsonValue::makeArray();
  formats.push(JsonValue("plaintext"));
  formats.push(JsonValue("markdown"));
  hover.set("contentFormat", std::move(formats));
  textDocument.set("hover", std::move(hover));

  textDocument.set("definition", object());

  JsonValue workspace = object();
  workspace.set("configuration", JsonValue(false));
  workspace.set("workspaceFolders", JsonValue(false));
  workspace.set("applyEdit", JsonValue(false));

  JsonValue capabilities = object();
  capabilities.set("textDocument", std::move(textDocument));
  capabilities.set("workspace", std::move(workspace));

  JsonValue clientInfo = object();
  clientInfo.set("name", JsonValue("CodeCore"));
  clientInfo.set("version", JsonValue("0.4.0"));

  JsonValue params = object();
  params.set("processId", JsonValue());
  params.set("clientInfo", std::move(clientInfo));
  params.set("rootUri", rootUri_.empty() ? JsonValue() : JsonValue(rootUri_));
  params.set("capabilities", std::move(capabilities));

  initializeId_ = request("initialize", std::move(params));
}

void LspClient::shutdown() {
  request("shutdown", JsonValue());
}

void LspClient::exit() {
  JsonValue message = object();
  message.set("method", JsonValue("exit"));
  enqueue(std::move(message));
}

void LspClient::reset() {
  outgoing_.clear();
  results_.clear();
  diagnostics_.clear();
  pending_.clear();
  parser_.reset();
  rootUri_.clear();
  capabilitiesJson_ = "{}";
  lastError_.clear();
  nextId_ = 1;
  initializeId_ = 0;
  initialized_ = false;
}

void LspClient::didOpen(std::string uri, std::string languageId, int32_t version,
                        std::string text) {
  JsonValue textDocument = object();
  textDocument.set("uri", JsonValue(uri));
  textDocument.set("languageId", JsonValue(std::move(languageId)));
  textDocument.set("version", JsonValue(static_cast<int>(version)));
  textDocument.set("text", JsonValue(std::move(text)));

  JsonValue params = object();
  params.set("textDocument", std::move(textDocument));

  JsonValue message = object();
  message.set("method", JsonValue("textDocument/didOpen"));
  message.set("params", std::move(params));
  enqueue(std::move(message));
}

void LspClient::didChange(std::string uri, int32_t version, std::string text) {
  JsonValue textDocument = object();
  textDocument.set("uri", JsonValue(uri));
  textDocument.set("version", JsonValue(static_cast<int>(version)));

  JsonValue change = object();
  change.set("text", JsonValue(std::move(text)));
  JsonValue changes = JsonValue::makeArray();
  changes.push(std::move(change));

  JsonValue params = object();
  params.set("textDocument", std::move(textDocument));
  params.set("contentChanges", std::move(changes));

  JsonValue message = object();
  message.set("method", JsonValue("textDocument/didChange"));
  message.set("params", std::move(params));
  enqueue(std::move(message));
}

void LspClient::didClose(std::string uri) {
  JsonValue textDocument = object();
  textDocument.set("uri", JsonValue(std::move(uri)));

  JsonValue params = object();
  params.set("textDocument", std::move(textDocument));

  JsonValue message = object();
  message.set("method", JsonValue("textDocument/didClose"));
  message.set("params", std::move(params));
  enqueue(std::move(message));
}

int64_t LspClient::completion(std::string uri, uint32_t line, uint32_t character) {
  JsonValue params = positionParams(uri, line, character);
  JsonValue context = object();
  context.set("triggerKind", JsonValue(1));
  params.set("context", std::move(context));
  return request("textDocument/completion", std::move(params));
}

int64_t LspClient::hover(std::string uri, uint32_t line, uint32_t character) {
  return request("textDocument/hover", positionParams(uri, line, character));
}

int64_t LspClient::definition(std::string uri, uint32_t line, uint32_t character) {
  return request("textDocument/definition", positionParams(uri, line, character));
}

void LspClient::didSave(std::string uri, std::string text, bool includeText) {
  JsonValue textDocument = object();
  textDocument.set("uri", JsonValue(std::move(uri)));

  JsonValue params = object();
  params.set("textDocument", std::move(textDocument));
  if (includeText) {
    params.set("text", JsonValue(std::move(text)));
  }

  JsonValue message = object();
  message.set("method", JsonValue("textDocument/didSave"));
  message.set("params", std::move(params));
  enqueue(std::move(message));
}

int64_t LspClient::signatureHelp(std::string uri, uint32_t line, uint32_t character) {
  return request("textDocument/signatureHelp", positionParams(uri, line, character));
}

int64_t LspClient::references(std::string uri, uint32_t line, uint32_t character,
                             bool includeDeclaration) {
  JsonValue params = positionParams(uri, line, character);
  JsonValue context = object();
  context.set("includeDeclaration", JsonValue(includeDeclaration));
  params.set("context", std::move(context));
  return request("textDocument/references", std::move(params));
}

int64_t LspClient::documentSymbols(std::string uri) {
  JsonValue textDocument = object();
  textDocument.set("uri", JsonValue(std::move(uri)));
  JsonValue params = object();
  params.set("textDocument", std::move(textDocument));
  return request("textDocument/documentSymbol", std::move(params));
}

int64_t LspClient::workspaceSymbols(std::string query) {
  JsonValue params = object();
  params.set("query", JsonValue(std::move(query)));
  return request("workspace/symbol", std::move(params));
}

int64_t LspClient::rename(std::string uri, uint32_t line, uint32_t character,
                          std::string newName) {
  JsonValue params = positionParams(uri, line, character);
  params.set("newName", JsonValue(std::move(newName)));
  return request("textDocument/rename", std::move(params));
}

int64_t LspClient::formatting(std::string uri, uint32_t tabSize, bool insertSpaces) {
  JsonValue textDocument = object();
  textDocument.set("uri", JsonValue(std::move(uri)));
  JsonValue options = object();
  options.set("tabSize", JsonValue(static_cast<int>(tabSize)));
  options.set("insertSpaces", JsonValue(insertSpaces));
  JsonValue params = object();
  params.set("textDocument", std::move(textDocument));
  params.set("options", std::move(options));
  return request("textDocument/formatting", std::move(params));
}

int64_t LspClient::codeAction(std::string uri, uint32_t startLine,
                              uint32_t startCharacter, uint32_t endLine,
                              uint32_t endCharacter) {
  JsonValue textDocument = object();
  textDocument.set("uri", JsonValue(std::move(uri)));
  JsonValue start = object();
  start.set("line", JsonValue(static_cast<int>(startLine)));
  start.set("character", JsonValue(static_cast<int>(startCharacter)));
  JsonValue end = object();
  end.set("line", JsonValue(static_cast<int>(endLine)));
  end.set("character", JsonValue(static_cast<int>(endCharacter)));
  JsonValue range = object();
  range.set("start", std::move(start));
  range.set("end", std::move(end));
  JsonValue context = object();
  context.set("diagnostics", JsonValue::makeArray());
  JsonValue params = object();
  params.set("textDocument", std::move(textDocument));
  params.set("range", std::move(range));
  params.set("context", std::move(context));
  return request("textDocument/codeAction", std::move(params));
}

int64_t LspClient::semanticTokens(std::string uri) {
  JsonValue textDocument = object();
  textDocument.set("uri", JsonValue(std::move(uri)));
  JsonValue params = object();
  params.set("textDocument", std::move(textDocument));
  return request("textDocument/semanticTokens/full", std::move(params));
}

int64_t LspClient::requestJson(std::string method, std::string_view paramsJson,
                               std::string* error) {
  if (error) {
    error->clear();
  }
  JsonValue params;
  std::string parseError;
  if (!JsonValue::parse(paramsJson, &params, &parseError)) {
    if (error) {
      *error = parseError;
    }
    return -1;
  }
  return request(std::move(method), std::move(params));
}

bool LspClient::notifyJson(std::string method, std::string_view paramsJson,
                           std::string* error) {
  if (error) {
    error->clear();
  }
  JsonValue params;
  std::string parseError;
  if (!JsonValue::parse(paramsJson, &params, &parseError)) {
    if (error) {
      *error = parseError;
    }
    return false;
  }
  JsonValue message = object();
  message.set("method", JsonValue(std::move(method)));
  message.set("params", std::move(params));
  enqueue(std::move(message));
  return true;
}

void LspClient::cancel(int64_t id) {
  JsonValue params = object();
  params.set("id", JsonValue(static_cast<int>(id)));
  JsonValue message = object();
  message.set("method", JsonValue("$/cancelRequest"));
  message.set("params", std::move(params));
  enqueue(std::move(message));
}

void LspClient::receive(std::string_view bytes) {
  parser_.feed(bytes);
  LspMessage message;
  std::string error;
  while (parser_.next(&message, &error)) {
    if (message.kind == LspMessageKind::Invalid) {
      if (!error.empty()) {
        lastError_ = error;
      }
      continue;
    }
    handleMessage(message);
  }
}

void LspClient::handleMessage(const LspMessage& message) {
  switch (message.kind) {
    case LspMessageKind::Response:
      handleResponse(message);
      break;
    case LspMessageKind::Request:
      handleServerRequest(message);
      break;
    case LspMessageKind::Notification:
      handleNotification(message);
      break;
    case LspMessageKind::Invalid:
      break;
  }
}

void LspClient::handleResponse(const LspMessage& message) {
  if (!message.id.isNumber()) {
    lastError_ = "response id is not numeric";
    return;
  }
  const int64_t id = static_cast<int64_t>(message.id.asNumber());
  const auto found = pending_.find(id);
  if (found == pending_.end()) {
    return;
  }

  LspResult result;
  result.id = id;
  result.method = found->second;
  result.error = message.isError;
  result.payload = message.isError ? message.error.dump() : message.result.dump();
  results_.push_back(std::move(result));
  pending_.erase(found);

  if (id == initializeId_ && !initialized_ && !message.isError) {
    initialized_ = true;
    const JsonValue* capabilities = message.result.find("capabilities");
    capabilitiesJson_ = capabilities ? capabilities->dump() : "{}";

    JsonValue notification = JsonValue::makeObject();
    notification.set("method", JsonValue("initialized"));
    notification.set("params", JsonValue::makeObject());
    enqueue(std::move(notification));
  }
}

void LspClient::handleServerRequest(const LspMessage& message) {
  JsonValue result;
  if (message.method == "workspace/configuration") {
    result = JsonValue::makeArray();
    const JsonValue* items = message.params.find("items");
    const size_t count = items ? items->asArray().size() : 0;
    for (size_t index = 0; index < count; ++index) {
      result.push(JsonValue());
    }
  } else if (message.method == "workspace/applyEdit") {
    result = JsonValue::makeObject();
    result.set("applied", JsonValue(false));
    result.set("failureReason", JsonValue("CodeCore does not apply workspace edits"));
  } else {
    result = JsonValue();
  }

  JsonValue response = JsonValue::makeObject();
  response.set("id", message.id);
  response.set("result", std::move(result));
  enqueue(std::move(response));
}

void LspClient::handleNotification(const LspMessage& message) {
  if (message.method != "textDocument/publishDiagnostics") {
    return;
  }
  LspDiagnostics report;
  const JsonValue* uri = message.params.find("uri");
  report.uri = uri ? uri->asString() : std::string();
  report.payload = message.params.dump();
  diagnostics_.push_back(std::move(report));
}

std::string LspClient::takeOutgoing() {
  if (outgoing_.empty()) {
    return std::string();
  }
  std::string frame = std::move(outgoing_.front());
  outgoing_.pop_front();
  return frame;
}

bool LspClient::takeResult(LspResult* out) {
  if (!out || results_.empty()) {
    return false;
  }
  *out = std::move(results_.front());
  results_.pop_front();
  return true;
}

bool LspClient::takeDiagnostics(LspDiagnostics* out) {
  if (!out || diagnostics_.empty()) {
    return false;
  }
  *out = std::move(diagnostics_.front());
  diagnostics_.pop_front();
  return true;
}

}  // namespace codecore::lsp
