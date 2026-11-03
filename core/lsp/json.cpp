#include "core/lsp/json.h"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <utility>

namespace codecore::lsp {
namespace {

constexpr int kMaxDepth = 128;

void appendUtf8(std::string& out, uint32_t codePoint) {
  if (codePoint <= 0x7F) {
    out.push_back(static_cast<char>(codePoint));
  } else if (codePoint <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
    out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
  } else if (codePoint <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
    out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
    out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
  }
}

int hexValue(char c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  }
  if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  }
  if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

class Parser {
 public:
  explicit Parser(std::string_view text) : text_(text) {}

  bool parse(JsonValue* out, std::string* error) {
    if (!out) {
      return fail("null output", error);
    }
    skipWhitespace();
    if (!parseValue(out, 0)) {
      if (error) {
        *error = error_;
      }
      return false;
    }
    skipWhitespace();
    if (position_ != text_.size()) {
      return fail("trailing characters", error);
    }
    return true;
  }

 private:
  bool fail(const char* message, std::string* error) {
    error_ = std::string(message) + " at byte " + std::to_string(position_);
    if (error) {
      *error = error_;
    }
    return false;
  }

  void skipWhitespace() {
    while (position_ < text_.size()) {
      const char c = text_[position_];
      if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
        ++position_;
      } else {
        break;
      }
    }
  }

  bool consume(char expected) {
    if (position_ < text_.size() && text_[position_] == expected) {
      ++position_;
      return true;
    }
    return false;
  }

  bool parseValue(JsonValue* out, int depth) {
    if (depth > kMaxDepth) {
      return fail("nesting is too deep", nullptr);
    }
    skipWhitespace();
    if (position_ >= text_.size()) {
      return fail("unexpected end of input", nullptr);
    }
    switch (text_[position_]) {
      case '{':
        return parseObject(out, depth);
      case '[':
        return parseArray(out, depth);
      case '"': {
        std::string value;
        if (!parseString(&value)) {
          return false;
        }
        *out = JsonValue(std::move(value));
        return true;
      }
      case 't':
        if (text_.substr(position_, 4) == "true") {
          position_ += 4;
          *out = JsonValue(true);
          return true;
        }
        return fail("invalid literal", nullptr);
      case 'f':
        if (text_.substr(position_, 5) == "false") {
          position_ += 5;
          *out = JsonValue(false);
          return true;
        }
        return fail("invalid literal", nullptr);
      case 'n':
        if (text_.substr(position_, 4) == "null") {
          position_ += 4;
          *out = JsonValue();
          return true;
        }
        return fail("invalid literal", nullptr);
      default:
        return parseNumber(out);
    }
  }

  bool parseObject(JsonValue* out, int depth) {
    ++position_;  // {
    JsonValue::Object object;
    skipWhitespace();
    if (consume('}')) {
      *out = JsonValue::makeObject();
      out->object() = std::move(object);
      return true;
    }
    while (true) {
      skipWhitespace();
      if (position_ >= text_.size() || text_[position_] != '"') {
        return fail("object key must be a string", nullptr);
      }
      std::string key;
      if (!parseString(&key)) {
        return false;
      }
      skipWhitespace();
      if (!consume(':')) {
        return fail("missing ':' after object key", nullptr);
      }
      JsonValue value;
      if (!parseValue(&value, depth + 1)) {
        return false;
      }
      object.emplace(std::move(key), std::move(value));
      skipWhitespace();
      if (consume('}')) {
        break;
      }
      if (!consume(',')) {
        return fail("missing ',' in object", nullptr);
      }
    }
    *out = JsonValue::makeObject();
    out->object() = std::move(object);
    return true;
  }

  bool parseArray(JsonValue* out, int depth) {
    ++position_;  // [
    JsonValue::Array array;
    skipWhitespace();
    if (consume(']')) {
      *out = JsonValue::makeArray();
      out->array() = std::move(array);
      return true;
    }
    while (true) {
      JsonValue value;
      if (!parseValue(&value, depth + 1)) {
        return false;
      }
      array.push_back(std::move(value));
      skipWhitespace();
      if (consume(']')) {
        break;
      }
      if (!consume(',')) {
        return fail("missing ',' in array", nullptr);
      }
    }
    *out = JsonValue::makeArray();
    out->array() = std::move(array);
    return true;
  }

  bool parseString(std::string* out) {
    if (!consume('"')) {
      return fail("expected string", nullptr);
    }
    out->clear();
    while (position_ < text_.size()) {
      const unsigned char c = static_cast<unsigned char>(text_[position_]);
      if (c == '"') {
        ++position_;
        return true;
      }
      if (c == '\\') {
        ++position_;
        if (position_ >= text_.size()) {
          return fail("unterminated escape", nullptr);
        }
        const char escape = text_[position_++];
        switch (escape) {
          case '"':
            out->push_back('"');
            break;
          case '\\':
            out->push_back('\\');
            break;
          case '/':
            out->push_back('/');
            break;
          case 'b':
            out->push_back('\b');
            break;
          case 'f':
            out->push_back('\f');
            break;
          case 'n':
            out->push_back('\n');
            break;
          case 'r':
            out->push_back('\r');
            break;
          case 't':
            out->push_back('\t');
            break;
          case 'u': {
            uint32_t codePoint = 0;
            if (!parseHex4(&codePoint)) {
              return false;
            }
            if (codePoint >= 0xD800 && codePoint <= 0xDBFF && position_ + 2 < text_.size() &&
                text_[position_] == '\\' && text_[position_ + 1] == 'u') {
              const size_t saved = position_;
              position_ += 2;
              uint32_t low = 0;
              if (parseHex4(&low) && low >= 0xDC00 && low <= 0xDFFF) {
                codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
              } else {
                position_ = saved;
              }
            }
            if (codePoint >= 0xD800 && codePoint <= 0xDFFF) {
              return fail("unpaired surrogate", nullptr);
            }
            appendUtf8(*out, codePoint);
            break;
          }
          default:
            return fail("unknown escape", nullptr);
        }
        continue;
      }
      if (c < 0x20) {
        return fail("control character in string", nullptr);
      }
      out->push_back(static_cast<char>(c));
      ++position_;
    }
    return fail("unterminated string", nullptr);
  }

  bool parseHex4(uint32_t* out) {
    if (position_ + 4 > text_.size()) {
      return fail("short unicode escape", nullptr);
    }
    uint32_t value = 0;
    for (int index = 0; index < 4; ++index) {
      const int digit = hexValue(text_[position_ + index]);
      if (digit < 0) {
        return fail("invalid unicode escape", nullptr);
      }
      value = (value << 4) | static_cast<uint32_t>(digit);
    }
    position_ += 4;
    *out = value;
    return true;
  }

  bool parseNumber(JsonValue* out) {
    const size_t start = position_;
    if (position_ < text_.size() && text_[position_] == '-') {
      ++position_;
    }
    if (position_ >= text_.size() ||
        !(text_[position_] >= '0' && text_[position_] <= '9')) {
      return fail("invalid number", nullptr);
    }
    if (text_[position_] == '0') {
      ++position_;
    } else {
      while (position_ < text_.size() && text_[position_] >= '0' &&
             text_[position_] <= '9') {
        ++position_;
      }
    }
    if (position_ < text_.size() && text_[position_] == '.') {
      ++position_;
      if (position_ >= text_.size() ||
          !(text_[position_] >= '0' && text_[position_] <= '9')) {
        return fail("invalid fraction", nullptr);
      }
      while (position_ < text_.size() && text_[position_] >= '0' &&
             text_[position_] <= '9') {
        ++position_;
      }
    }
    if (position_ < text_.size() &&
        (text_[position_] == 'e' || text_[position_] == 'E')) {
      ++position_;
      if (position_ < text_.size() &&
          (text_[position_] == '+' || text_[position_] == '-')) {
        ++position_;
      }
      if (position_ >= text_.size() ||
          !(text_[position_] >= '0' && text_[position_] <= '9')) {
        return fail("invalid exponent", nullptr);
      }
      while (position_ < text_.size() && text_[position_] >= '0' &&
             text_[position_] <= '9') {
        ++position_;
      }
    }
    const std::string literal(text_.substr(start, position_ - start));
    char* end = nullptr;
    const double value = std::strtod(literal.c_str(), &end);
    if (!end || *end != '\0' || !std::isfinite(value)) {
      return fail("invalid number", nullptr);
    }
    *out = JsonValue(value);
    return true;
  }

  std::string_view text_;
  size_t position_ = 0;
  std::string error_;
};

void escapeString(std::string& out, std::string_view value) {
  out.push_back('"');
  for (const char raw : value) {
    const unsigned char c = static_cast<unsigned char>(raw);
    switch (c) {
      case '"':
        out += "\\\"";
        break;
      case '\\':
        out += "\\\\";
        break;
      case '\b':
        out += "\\b";
        break;
      case '\f':
        out += "\\f";
        break;
      case '\n':
        out += "\\n";
        break;
      case '\r':
        out += "\\r";
        break;
      case '\t':
        out += "\\t";
        break;
      default:
        if (c < 0x20) {
          constexpr char kHex[] = "0123456789abcdef";
          out += "\\u00";
          out.push_back(kHex[(c >> 4) & 0xF]);
          out.push_back(kHex[c & 0xF]);
        } else {
          out.push_back(raw);
        }
        break;
    }
  }
  out.push_back('"');
}

}  // namespace

JsonValue JsonValue::makeArray() {
  JsonValue value;
  value.type_ = Type::Array;
  return value;
}

JsonValue JsonValue::makeObject() {
  JsonValue value;
  value.type_ = Type::Object;
  return value;
}

bool JsonValue::asBool(bool fallback) const {
  return type_ == Type::Bool ? bool_ : fallback;
}

double JsonValue::asNumber(double fallback) const {
  return type_ == Type::Number ? number_ : fallback;
}

const std::string& JsonValue::asString() const {
  static const std::string empty;
  return type_ == Type::String ? string_ : empty;
}

const JsonValue::Array& JsonValue::asArray() const {
  static const Array empty;
  return type_ == Type::Array ? array_ : empty;
}

const JsonValue::Object& JsonValue::asObject() const {
  static const Object empty;
  return type_ == Type::Object ? object_ : empty;
}

JsonValue::Array& JsonValue::array() {
  if (type_ != Type::Array) {
    type_ = Type::Array;
    array_.clear();
  }
  return array_;
}

JsonValue::Object& JsonValue::object() {
  if (type_ != Type::Object) {
    type_ = Type::Object;
    object_.clear();
  }
  return object_;
}

const JsonValue* JsonValue::find(std::string_view key) const {
  if (type_ != Type::Object) {
    return nullptr;
  }
  const auto found = object_.find(std::string(key));
  return found == object_.end() ? nullptr : &found->second;
}

JsonValue* JsonValue::find(std::string_view key) {
  if (type_ != Type::Object) {
    return nullptr;
  }
  const auto found = object_.find(std::string(key));
  return found == object_.end() ? nullptr : &found->second;
}

void JsonValue::set(std::string key, JsonValue value) {
  object()[std::move(key)] = std::move(value);
}

void JsonValue::push(JsonValue value) { array().push_back(std::move(value)); }

std::string JsonValue::dump() const {
  std::string out;
  dumpTo(out);
  return out;
}

void JsonValue::dumpTo(std::string& out) const {
  switch (type_) {
    case Type::Null:
      out += "null";
      break;
    case Type::Bool:
      out += bool_ ? "true" : "false";
      break;
    case Type::Number: {
      char buffer[64];
      const auto result = std::to_chars(buffer, buffer + sizeof(buffer), number_);
      out.append(buffer, result.ptr);
      break;
    }
    case Type::String:
      escapeString(out, string_);
      break;
    case Type::Array: {
      out.push_back('[');
      bool first = true;
      for (const JsonValue& value : array_) {
        if (!first) {
          out.push_back(',');
        }
        first = false;
        value.dumpTo(out);
      }
      out.push_back(']');
      break;
    }
    case Type::Object: {
      out.push_back('{');
      bool first = true;
      for (const auto& entry : object_) {
        if (!first) {
          out.push_back(',');
        }
        first = false;
        escapeString(out, entry.first);
        out.push_back(':');
        entry.second.dumpTo(out);
      }
      out.push_back('}');
      break;
    }
  }
}

bool JsonValue::parse(std::string_view text, JsonValue* out, std::string* error) {
  Parser parser(text);
  return parser.parse(out, error);
}

}  // namespace codecore::lsp
