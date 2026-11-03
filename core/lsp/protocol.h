#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "core/lsp/json.h"

namespace codecore::lsp {

enum class LspMessageKind { Request, Notification, Response, Invalid };

struct LspMessage {
  LspMessageKind kind = LspMessageKind::Invalid;
  std::string method;
  JsonValue id;
  bool hasId = false;
  JsonValue params;
  JsonValue result;
  JsonValue error;
  bool isError = false;
};

/* Adds the LSP base-protocol header around a JSON body. */
std::string encodeFrame(std::string_view json);

/*
 * Incremental reader for LSP frames.
 *
 * feed() may be called with partial headers, partial bodies or several frames
 * at once. next() returns false when more bytes are needed and true when a
 * message (possibly Invalid) was produced.
 */
class FrameParser {
 public:
  void feed(std::string_view bytes);
  bool next(LspMessage* out, std::string* error);
  size_t buffered() const { return buffer_.size(); }
  void reset();

 private:
  std::string buffer_;
};

}  // namespace codecore::lsp
