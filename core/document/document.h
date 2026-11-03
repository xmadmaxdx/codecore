#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "core/analysis/analysis.h"
#include "core/diff/diff.h"
#include "core/document/line_index.h"
#include "core/document/types.h"
#include "core/language/language.h"
#include "core/snapshot/snapshot.h"
#include "core/syntax/syntax.h"
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

  EditOrigin editOrigin() const;
  Status setEditOrigin(EditOrigin origin);

  /* Proposed content is preview-only until acceptProposal() commits it. */
  Status setProposedText(std::string_view text, EditOrigin origin);
  bool hasProposal() const;
  std::string proposedText() const;
  std::vector<DiffLine> proposalDiff() const;
  Status acceptProposal();
  Status rejectProposal();

  /* Ghost text never enters the document until it is accepted. */
  GhostTextState ghostText() const;
  Status setGhostText(size_t offset, std::string_view text, EditOrigin origin);
  Status acceptGhostText(size_t maxCodePoints);
  Status dismissGhostText();

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

  /* Multi-cursor state. Index 0 is the primary selection. */
  size_t cursorCount() const;
  Status cursorAt(size_t index, Selection* out) const;
  Status addCursor(uint32_t anchor, uint32_t focus);
  void clearCursors();
  Status moveCursors(CursorMove move, bool extendSelection);

  /* Replaces every selection with text as one undoable transaction. */
  Status multiInsert(std::string_view text);
  /* Erases every selection, or one code point per collapsed caret. */
  Status multiErase(bool backward);
  Status matchBracket(size_t offset, size_t* outMatch) const;

  /*
   * Structural syntax tree for the current version, parsed incrementally from
   * the previous tree. Returns nullptr when the language has no bundled
   * grammar. The returned tree is immutable and safe to read on a worker.
   */
  std::shared_ptr<SyntaxTree> syntaxTree() const;

  std::vector<Token> tokenize(size_t start, size_t end) const;
  std::vector<CompletionItem> complete(size_t offset) const;
  std::vector<Diagnostic> diagnose() const;
  std::vector<FoldingRange> foldingRanges() const;
  Status find(std::string_view query, const SearchOptions& options,
              std::vector<Range>* out) const;

  /* Captures text, language and version for lock-free analysis elsewhere. */
  std::shared_ptr<Snapshot> createSnapshot() const;
  Status restoreSnapshot(const Snapshot& snapshot);

  void setChangeListener(ChangeListener listener, void* userData);
  const char* lastError() const;

 private:
  struct EditRecord {
    struct Op {
      size_t offset = 0;
      std::string removed;
      std::string inserted;
    };
    std::vector<Op> ops;
    EditOrigin origin = EditOrigin::Host;
  };

  Status fail(Status status, const char* message) const;
  Status succeed() const;
  Status applyContent(std::string_view text, ChangeKind kind, EditOrigin origin);
  void clearTransientState();
  void emitChange(ChangeKind kind, size_t start, size_t removed, size_t inserted,
                  EditOrigin origin);
  void recordEdit(size_t offset, std::string removed, std::string inserted, bool coalesce,
                  EditOrigin origin);
  void recordEdit(std::vector<EditRecord::Op> ops, EditOrigin origin);
  Status computeCursorTarget(const std::string& text, Selection selection, CursorMove move,
                             bool extendSelection, size_t* outTarget) const;
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

  mutable std::shared_ptr<SyntaxTree> syntaxTree_;
  mutable uint64_t syntaxVersion_ = 0;
  mutable int32_t syntaxLanguageId_ = -1;

  uint64_t version_ = 0;
  EditOrigin nextOrigin_ = EditOrigin::Host;
  std::vector<Selection> selections_{Selection{}};
  std::string proposedText_;
  bool hasProposal_ = false;
  EditOrigin proposalOrigin_ = EditOrigin::Ai;
  GhostTextState ghost_;
  std::vector<EditRecord> undoStack_;
  std::vector<EditRecord> redoStack_;
  size_t historyLimit_ = 200;
  uint64_t lastEditTick_ = 0;

  ChangeListener listener_ = nullptr;
  void* listenerUserData_ = nullptr;
  mutable const char* lastError_ = nullptr;
};

}  // namespace codecore
