#include "core/lsp/protocol.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>
#include <utility>

namespace codecore::lsp {
namespace {

constexpr size_t kMaxFrameBytes = 16u * 1024u * 1024u;

std::string lowerAscii(std::string_view value) {
  std::string result(value);
  std::transform(result.begin(), result.end(), result.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

std::string_view trim(std::string_view value) {
  while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
    value.remove_prefix(1);
  }
  while (!value.empty() && (value.back() == ' ' || value.back() == '\t' ||
                            value.back() == '\r')) {
    value.remove_suffix(1);
  }
  return value;
}

}  // namespace

std::string encodeFrame(std::string_view json) {
  std::string frame;
  frame.reserve(json.size() + 32);
  frame += "Content-Length: ";
  frame += std::to_string(json.size());
  frame += "\r\n\r\n";
  frame.append(json.data(), json.size());
  return frame;
}

void FrameParser::feed(std::string_view bytes) {
  buffer_.append(bytes.data(), bytes.size());
}

void FrameParser::reset() { buffer_.clear(); }

bool FrameParser::next(LspMessage* out, std::string* error) {
  if (error) {
    error->clear();
  }
  if (!out) {
    return false;
  }
  size_t headerEnd = buffer_.find("\r\n\r\n");
  size_t separatorLength = 4;
  if (headerEnd == std::string::npos) {
    headerEnd = buffer_.find("\n\n");
    separatorLength = 2;
  }
  if (headerEnd == std::string::npos) {
    return false;
  }

  size_t contentLength = static_cast<size_t>(-1);
  size_t lineStart = 0;
  while (lineStart < headerEnd) {
    size_t lineEnd = buffer_.find('\n', lineStart);
    if (lineEnd == std::string::npos || lineEnd > headerEnd) {
      lineEnd = headerEnd;
    }
    const std::string_view line(buffer_.data() + lineStart, lineEnd - lineStart);
    const size_t colon = line.find(':');
    if (colon != std::string_view::npos) {
      const std::string name = lowerAscii(trim(line.substr(0, colon)));
      const std::string_view value = trim(line.substr(colon + 1));
      if (name == "content-length") {
        const std::string text(value);
        char* end = nullptr;
        const unsigned long long parsed = std::strtoull(text.c_str(), &end, 10);
        if (!end || *end != '\0' || parsed > kMaxFrameBytes) {
          buffer_.clear();
          if (error) {
            *error = "invalid Content-Length header";
          }
          out->kind = LspMessageKind::Invalid;
          return true;
        }
        contentLength = static_cast<size_t>(parsed);
      }
    }
    lineStart = lineEnd + 1;
  }

  if (contentLength == static_cast<size_t>(-1)) {
    buffer_.clear();
    if (error) {
      *error = "missing Content-Length header";
    }
    out->kind = LspMessageKind::Invalid;
    return true;
  }
  if (buffer_.size() < headerEnd + separatorLength + contentLength) {
    return false;
  }

  const std::string body = buffer_.substr(headerEnd + separatorLength, contentLength);
  buffer_.erase(0, headerEnd + separatorLength + contentLength);

  JsonValue json;
  std::string parseError;
  if (!JsonValue::parse(body, &json, &parseError)) {
    if (error) {
      *error = "invalid JSON body: " + parseError;
    }
    out->kind = LspMessageKind::Invalid;
    return true;
  }

  *out = LspMessage{};
  const JsonValue* method = json.find("method");
  const JsonValue* id = json.find("id");
  if (method && method->isString()) {
    out->method = method->asString();
    if (id) {
      out->id = *id;
      out->hasId = true;
    }
    const JsonValue* params = json.find("params");
    if (params) {
      out->params = *params;
    }
    out->kind = out->hasId ? LspMessageKind::Request : LspMessageKind::Notification;
    return true;
  }

  if (!id) {
    if (error) {
      *error = "response without an id";
    }
    out->kind = LspMessageKind::Invalid;
    return true;
  }
  out->kind = LspMessageKind::Response;
  out->id = *id;
  out->hasId = true;
  const JsonValue* errorValue = json.find("error");
  if (errorValue) {
    out->isError = true;
    out->error = *errorValue;
  } else {
    const JsonValue* result = json.find("result");
    if (result) {
      out->result = *result;
    }
  }
  return true;
}

}  // namespace codecore::lsp
