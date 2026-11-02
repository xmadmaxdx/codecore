#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "core/language/language.h"
#include "core/text_buffer/text_buffer.h"

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

/*
 * A document owns its text, undo history, selection and language services.
 *
 * All public methods serialize on an internal recursive mutex. Change listeners
 * run on the mutating thread while the document is locked; listeners must not
 * destroy the document and should hand off expensive work asynchronously.
 */
class Document {
 public:
  Document(std::shared_ptr<const Language> language, const DocumentOptions& options,
           size_t maxDocumentBytes);
  ~Document();

  Document(const Document&) = delete;
  Document& operator=(const Document&) = delete;

  Status setText(std::string_view text);
  std::string text() const;
  size_t length() const;
  uint64_t version() const;

  int32_t languageId() const;
  const Language& language() const;
  Status setLanguage(std::shared_ptr<const Language> language);

  bool readOnly() const;
  Status setReadOnly(bool readOnly);
  uint32_t tabSize() const;
  bool insertSpaces() const;
  size_t maxDocumentBytes() const;

  Status insert(size_t offset, std::string_view text);
  Status erase(size_t start, size_t end);
  Status replace(size_t start, size_t end, std::string_view text);

  bool canUndo() const;
  bool canRedo() const;
  Status undo();
  Status redo();
  void clearHistory();
  void setHistoryLimit(size_t limit);

  size_t lineCount() const;
  Status lineAt(size_t index, LineInfo* out) const;
  Status offsetToPosition(size_t offset, Position* out) const;
  Status positionToOffset(uint32_t line, uint32_t column, size_t* out) const;

  Selection selection() const;
  Status setSelection(uint32_t anchor, uint32_t focus);
  Status moveCursor(CursorMove move, bool extendSelection);
  Status matchBracket(size_t offset, size_t* outMatch) const;

  std::vector<Token> tokenize(size_t start, size_t end) const;
  std::vector<CompletionItem> complete(size_t offset) const;
  std::vector<Diagnostic> diagnose() const;
  std::vector<FoldingRange> foldingRanges() const;
  std::vector<Range> find(std::string_view query, const SearchOptions& options) const;

  void setChangeListener(ChangeListener listener, void* userData);
  const char* lastError() const;

 private:
  struct EditRecord {
    size_t offset = 0;
    std::string removed;
    std::string inserted;
  };

  Status fail(Status status, const char* message) const;
  Status succeed() const;
  void emitChange(ChangeKind kind, size_t start, size_t removed, size_t inserted);
  void recordEdit(size_t offset, std::string removed, std::string inserted, bool coalesce);
  void invalidateLines() const;
  void ensureLines() const;
  size_t lineOfOffset(size_t offset) const;
  std::string allText() const;
  LexResult lexAll() const;

  std::shared_ptr<const Language> language_;
  TextBuffer buffer_;
  DocumentOptions options_;
  size_t maxDocumentBytes_ = 0;
  mutable std::recursive_mutex mutex_;

  mutable std::vector<LineInfo> lines_;
  mutable bool linesDirty_ = true;

  uint64_t version_ = 0;
  Selection selection_;
  std::vector<EditRecord> undoStack_;
  std::vector<EditRecord> redoStack_;
  size_t historyLimit_ = 200;
  uint64_t lastEditTick_ = 0;

  ChangeListener listener_ = nullptr;
  void* listenerUserData_ = nullptr;
  mutable const char* lastError_ = nullptr;
};

}  // namespace codecore
