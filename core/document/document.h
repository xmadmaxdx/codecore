#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "core/analysis/analysis.h"
#include "core/document/line_index.h"
#include "core/document/types.h"
#include "core/language/language.h"
#include "core/snapshot/snapshot.h"
#include "core/text_buffer/text_buffer.h"

namespace codecore {

/*
 * A document owns its text, undo history, selection and language services.
 *
 * Line starts are maintained incrementally. Language analysis is computed once
 * per version and cached in an immutable Analysis object; snapshots copy the
 * text and provide independent analysis caches for worker threads.
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

  /* Captures text, language and version for lock-free analysis elsewhere. */
  std::shared_ptr<Snapshot> createSnapshot() const;

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
  const std::string& currentText() const;
  std::shared_ptr<Analysis> currentAnalysis() const;

  std::shared_ptr<const Language> language_;
  TextBuffer buffer_;
  DocumentOptions options_;
  size_t maxDocumentBytes_ = 0;
  mutable std::recursive_mutex mutex_;

  mutable std::string textCache_;
  mutable bool textDirty_ = false;
  LineIndex lineIndex_;
  mutable std::shared_ptr<Analysis> analysis_;
  mutable uint64_t analysisVersion_ = 0;
  mutable int32_t analysisLanguageId_ = -1;

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
