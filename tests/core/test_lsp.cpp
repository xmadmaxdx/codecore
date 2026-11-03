#include "codecore/codecore.h"

#include <cstdint>
#include <cstdio>
#include <string>

#include "core/lsp/client.h"
#include "core/lsp/json.h"
#include "core/lsp/protocol.h"

namespace {

int checks = 0;
int failures = 0;

void check(bool condition, const char* expression, int line) {
  ++checks;
  if (!condition) {
    ++failures;
    std::printf("FAIL line %d: %s\n", line, expression);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

std::string bodyOf(const std::string& frame) {
  const size_t separator = frame.find("\r\n\r\n");
  return separator == std::string::npos ? std::string() : frame.substr(separator + 4);
}

void testJson() {
  codecore::lsp::JsonValue value;
  std::string error;
  CHECK(codecore::lsp::JsonValue::parse(
      R"({"a":1,"b":[true,null,"x\n"],"c":{"d":2}})", &value, &error));
  const codecore::lsp::JsonValue* a = value.find("a");
  CHECK(a != nullptr && a->asNumber() == 1.0);
  const codecore::lsp::JsonValue* b = value.find("b");
  CHECK(b != nullptr && b->isArray() && b->asArray().size() == 3);
  CHECK(b != nullptr && b->asArray()[1].isNull());
  CHECK(value.dump().find("\"a\":1") != std::string::npos);

  codecore::lsp::JsonValue unicode;
  CHECK(codecore::lsp::JsonValue::parse(R"({"s":"\u00e9\ud83d\ude00"})", &unicode, &error));
  const codecore::lsp::JsonValue* s = unicode.find("s");
  CHECK(s != nullptr && s->asString().size() == 6);

  codecore::lsp::JsonValue bad;
  CHECK(!codecore::lsp::JsonValue::parse("{", &bad, &error));
  CHECK(!error.empty());
  CHECK(!codecore::lsp::JsonValue::parse("{} trailing", &bad, &error));
}

void testFrames() {
  using namespace codecore::lsp;
  const std::string frame = encodeFrame(
      R"({"jsonrpc":"2.0","id":7,"method":"initialize","params":{}})");
  CHECK(frame.find("Content-Length:") == 0);

  FrameParser parser;
  LspMessage message;
  std::string error;
  parser.feed(frame.substr(0, 12));
  CHECK(!parser.next(&message, &error));
  parser.feed(frame.substr(12));
  CHECK(parser.next(&message, &error));
  CHECK(message.kind == LspMessageKind::Request);
  CHECK(message.method == "initialize");
  CHECK(message.hasId && message.id.asNumber() == 7.0);
  CHECK(!parser.next(&message, &error));

  const std::string notification = encodeFrame(
      R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
  parser.feed(notification);
  CHECK(parser.next(&message, &error));
  CHECK(message.kind == LspMessageKind::Notification);
  CHECK(message.method == "initialized");

  FrameParser invalid;
  invalid.feed("Content-Length: 2\r\n\r\n{}");
  CHECK(invalid.next(&message, &error));
  CHECK(message.kind == LspMessageKind::Invalid);
  CHECK(!error.empty());
}

void testClient() {
  using namespace codecore::lsp;
  LspClient client;
  client.initialize("file:///project");
  const std::string initializeFrame = client.takeOutgoing();
  CHECK(initializeFrame.find("Content-Length:") == 0);
  CHECK(bodyOf(initializeFrame).find("\"method\":\"initialize\"") != std::string::npos);
  CHECK(bodyOf(initializeFrame).find("\"rootUri\":\"file:///project\"") !=
        std::string::npos);

  client.receive(encodeFrame(
      R"({"jsonrpc":"2.0","id":1,"result":{"capabilities":{"completionProvider":{"triggerCharacters":["."]}}}})"));
  CHECK(client.isInitialized());
  CHECK(client.serverCapabilities().find("completionProvider") != std::string::npos);
  const std::string initializedFrame = client.takeOutgoing();
  CHECK(bodyOf(initializedFrame).find("\"method\":\"initialized\"") !=
        std::string::npos);

  client.didOpen("file:///a.py", "python", 1, "print(1)\n");
  CHECK(bodyOf(client.takeOutgoing()).find("textDocument/didOpen") !=
        std::string::npos);
  client.didChange("file:///a.py", 2, "print(2)\n");
  CHECK(bodyOf(client.takeOutgoing()).find("textDocument/didChange") !=
        std::string::npos);
  client.didClose("file:///a.py");
  CHECK(bodyOf(client.takeOutgoing()).find("textDocument/didClose") !=
        std::string::npos);

  const int64_t requestId = client.completion("file:///a.py", 2, 3);
  CHECK(requestId > 1);
  CHECK(bodyOf(client.takeOutgoing()).find("textDocument/completion") !=
        std::string::npos);
  CHECK(client.pendingRequests() == 1);

  client.receive(encodeFrame(
      "{\"jsonrpc\":\"2.0\",\"id\":" + std::to_string(requestId) +
      ",\"result\":{\"items\":[]}}"));
  LspResult result;
  CHECK(client.takeResult(&result));
  CHECK(result.id == requestId);
  CHECK(result.method == "textDocument/completion");
  CHECK(result.payload.find("items") != std::string::npos);
  CHECK(!result.error);
  CHECK(client.pendingRequests() == 0);

  client.receive(encodeFrame(
      R"({"jsonrpc":"2.0","method":"textDocument/publishDiagnostics","params":{"uri":"file:///a.py","diagnostics":[{"message":"bad"}]}})"));
  LspDiagnostics report;
  CHECK(client.takeDiagnostics(&report));
  CHECK(report.uri == "file:///a.py");
  CHECK(report.payload.find("bad") != std::string::npos);

  client.receive(encodeFrame(
      R"({"jsonrpc":"2.0","id":99,"method":"workspace/configuration","params":{"items":[{},{}]}})"));
  const std::string configuration = bodyOf(client.takeOutgoing());
  CHECK(configuration.find("\"id\":99") != std::string::npos);
  CHECK(configuration.find("\"result\":[null,null]") != std::string::npos);

  client.shutdown();
  CHECK(bodyOf(client.takeOutgoing()).find("\"method\":\"shutdown\"") !=
        std::string::npos);
  client.exit();
  CHECK(bodyOf(client.takeOutgoing()).find("\"method\":\"exit\"") !=
        std::string::npos);
}

void testCAbi() {
  cc_lsp_client* client = cc_lsp_create();
  CHECK(client != nullptr);
  cc_lsp_initialize(client, "file:///x");
  size_t length = 0;
  char* frame = cc_lsp_take_outgoing(client, &length);
  CHECK(frame != nullptr && length > 0);
  CHECK(std::string(frame, length).find("initialize") != std::string::npos);
  cc_string_free(frame);
  CHECK(cc_lsp_is_initialized(client) == 0);
  CHECK(cc_lsp_completion(client, "file:///x", 0, 0) > 0);
  char* method = nullptr;
  char* payload = nullptr;
  CHECK(cc_lsp_take_result(client, &method, &payload) == 0);
  CHECK(method == nullptr && payload == nullptr);
  cc_lsp_destroy(client);
}

}  // namespace

int main() {
  testJson();
  testFrames();
  testClient();
  testCAbi();

  std::printf("LSP checks: %d, failures: %d\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
