#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace codecore {

enum class Status : int32_t {
  Ok = 0,
  InvalidArgument = 1,
  OutOfRange = 2,
  ReadOnly = 3,
  UnknownLanguage = 4,
  OutOfMemory = 5,
  Unsupported = 6,
  InternalError = 7,
};

enum class ChangeKind : uint32_t {
  SetText = 0,
  Insert = 1,
  Erase = 2,
  Replace = 3,
  Undo = 4,
  Redo = 5,
};

enum class CursorMove : uint32_t {
  Left = 0,
  Right = 1,
  WordLeft = 2,
  WordRight = 3,
  LineStart = 4,
  LineEnd = 5,
  Up = 6,
  Down = 7,
  DocumentStart = 8,
  DocumentEnd = 9,
};

enum class CompletionKind : uint32_t {
  Keyword = 0,
  Symbol = 1,
  Function = 2,
  Class = 3,
  Type = 4,
  Builtin = 5,
  Property = 6,
};

struct LineInfo {
  size_t start = 0;
  size_t contentEnd = 0;
  size_t end = 0;
};

struct Position {
  uint32_t line = 0;
  uint32_t column = 0;
};

struct Selection {
  uint32_t anchor = 0;
  uint32_t focus = 0;
};

struct CompletionItem {
  std::string label;
  std::string insertText;
  std::string detail;
  CompletionKind kind = CompletionKind::Symbol;
  int32_t score = 0;
};

struct Diagnostic {
  uint32_t start = 0;
  uint32_t end = 0;
  uint32_t severity = 1;
  std::string message;
  std::string source;
};

struct FoldingRange {
  uint32_t startLine = 0;
  uint32_t endLine = 0;
  uint32_t kind = 0;
};

struct SearchOptions {
  bool caseSensitive = false;
  bool wholeWord = false;
};

struct Range {
  uint32_t start = 0;
  uint32_t end = 0;
};

struct DocumentOptions {
  int32_t languageId = 0;
  uint32_t tabSize = 4;
  bool insertSpaces = true;
  bool readOnly = false;
};

struct ChangeEvent {
  uint64_t version = 0;
  uint32_t kind = 0;
  uint32_t start = 0;
  uint32_t removedLength = 0;
  uint32_t insertedLength = 0;
};

using ChangeListener = void (*)(void* userData, const ChangeEvent* event);

}  // namespace codecore
