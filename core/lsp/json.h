#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace codecore::lsp {

/*
 * Minimal JSON value used by the LSP layer.
 *
 * Objects use std::map, so serialization is deterministic. The parser accepts
 * standard JSON (including \u escapes with surrogate pairs) and rejects
 * trailing data, control characters in strings and excessive nesting.
 */
class JsonValue {
 public:
  enum class Type { Null, Bool, Number, String, Array, Object };

  using Array = std::vector<JsonValue>;
  using Object = std::map<std::string, JsonValue>;

  JsonValue() = default;
  explicit JsonValue(bool value) : type_(Type::Bool), bool_(value) {}
  explicit JsonValue(double value) : type_(Type::Number), number_(value) {}
  explicit JsonValue(int value) : type_(Type::Number), number_(value) {}
  explicit JsonValue(std::string value) : type_(Type::String), string_(std::move(value)) {}
  explicit JsonValue(const char* value)
      : type_(Type::String), string_(value ? value : "") {}

  static JsonValue makeArray();
  static JsonValue makeObject();

  Type type() const { return type_; }
  bool isNull() const { return type_ == Type::Null; }
  bool isBool() const { return type_ == Type::Bool; }
  bool isNumber() const { return type_ == Type::Number; }
  bool isString() const { return type_ == Type::String; }
  bool isArray() const { return type_ == Type::Array; }
  bool isObject() const { return type_ == Type::Object; }

  bool asBool(bool fallback = false) const;
  double asNumber(double fallback = 0.0) const;
  const std::string& asString() const;
  const Array& asArray() const;
  const Object& asObject() const;
  Array& array();
  Object& object();

  const JsonValue* find(std::string_view key) const;
  JsonValue* find(std::string_view key);
  void set(std::string key, JsonValue value);
  void push(JsonValue value);

  std::string dump() const;
  static bool parse(std::string_view text, JsonValue* out, std::string* error = nullptr);

 private:
  void dumpTo(std::string& out) const;

  Type type_ = Type::Null;
  bool bool_ = false;
  double number_ = 0.0;
  std::string string_;
  Array array_;
  Object object_;
};

}  // namespace codecore::lsp
