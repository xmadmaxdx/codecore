#include "core/document/document.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <limits>
#include <utility>

namespace codecore {
namespace {

bool isContinuation(unsigned char c) { return (c & 0xC0u) == 0x80u; }

size_t nextCodepointOffset(const std::string& text, size_t offset) {
  if (offset >= text.size()) {
    return text.size();
  }
  size_t next = offset + 1;
  while (next < text.size() && isContinuation(static_cast<unsigned char>(text[next]))) {
    ++next;
  }
  return next;
}

size_t previousCodepointOffset(const std::string& text, size_t offset) {
  if (offset == 0) {
    return 0;
  }
  size_t previous = offset - 1;
  while (previous > 0 && isContinuation(static_cast<unsigned char>(text[previous]))) {
    --previous;
  }
  return previous;
}

size_t codepointCount(const std::string& text, size_t start, size_t end) {
  size_t count = 0;
  size_t offset = start;
  while (offset < end) {
    offset = nextCodepointOffset(text, offset);
    ++count;
  }
  return count;
}

bool isValidUtf8(std::string_view text) {
  size_t index = 0;
  while (index < text.size()) {
    const unsigned char lead = static_cast<unsigned char>(text[index]);
    size_t continuation = 0;
    if (lead < 0x80) {
      ++index;
      continue;
    }
    if (lead >= 0xC2 && lead <= 0xDF) {
      continuation = 1;
    } else if (lead >= 0xE0 && lead <= 0xEF) {
      continuation = 2;
    } else if (lead >= 0xF0 && lead <= 0xF4) {
      continuation = 3;
    } else {
      return false;
    }
    if (index + continuation >= text.size()) {
      return false;
    }
    for (size_t offset = 1; offset <= continuation; ++offset) {
      if (!isContinuation(static_cast<unsigned char>(text[index + offset]))) {
        return false;
      }
    }
    index += continuation + 1;
  }
  return true;
}

bool isWordByte(unsigned char c) {
  return std::isalnum(c) != 0 || c == '_' || c == '$';
}

uint64_t steadyTicks() {
  return static_cast<uint64_t>(
      std::chrono::duration_cast<std::chrono::nanoseconds>(
          std::chrono::steady_clock::now().time_since_epoch())
          .count());
}

}  // namespace

Document::Document(std::shared_ptr<const Language> language, const DocumentOptions& options,
                   size_t maxDocumentBytes)
    : language_(std::move(language)), options_(options), maxDocumentBytes_(maxDocumentBytes) {
  if (maxDocumentBytes_ == 0) {
    maxDocumentBytes_ = 64u * 1024u * 1024u;
  }
  if (language_) {
    options_.languageId = language_->id;
  }
  lineIndex_.rebuild("");
}

Document::~Document() = default;

bool isValidOrigin(EditOrigin origin) {
  switch (origin) {
    case EditOrigin::User:
    case EditOrigin::Ai:
    case EditOrigin::Host:
    case EditOrigin::Formatter:
    case EditOrigin::ExternalTool:
      return true;
  }
  return false;
}

Status Document::fail(Status status, const char* message) const {
  lastError_ = message;
  return status;
}

Status Document::succeed() const {
  lastError_ = nullptr;
  return Status::Ok;
}

const std::string& Document::currentText() const {
  if (textDirty_) {
    textCache_ = buffer_.toString();
    textDirty_ = false;
  }
  return textCache_;
}

std::shared_ptr<Analysis> Document::currentAnalysis() const {
  const int32_t languageId = language_ ? language_->id : -1;
  if (!analysis_ || analysisVersion_ != version_ || analysisLanguageId_ != languageId) {
    analysis_ = std::make_shared<Analysis>(currentText(), language_);
    analysisVersion_ = version_;
    analysisLanguageId_ = languageId;
  }
  return analysis_;
}

Status Document::setText(std::string_view text) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (options_.readOnly) {
    return fail(Status::ReadOnly, "document is read-only");
  }
  if (!isValidUtf8(text)) {
    return fail(Status::InvalidArgument, "text is not valid UTF-8");
  }
  if (text.size() > maxDocumentBytes_) {
    return fail(Status::OutOfRange, "document exceeds the configured maximum size");
  }
  const EditOrigin origin = nextOrigin_;
  buffer_.reset(text);
  textCache_.assign(text.data(), text.size());
  textDirty_ = false;
  lineIndex_.rebuild(text);
  analysis_.reset();
  version_ += 1;
  selection_ = Selection{};
  clearTransientState();
  undoStack_.clear();
  redoStack_.clear();
  lastEditTick_ = 0;
  emitChange(ChangeKind::SetText, 0, 0, text.size(), origin);
  return succeed();
}

std::string Document::text() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return currentText();
}

size_t Document::length() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return buffer_.size();
}

uint64_t Document::version() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return version_;
}

int32_t Document::languageId() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return language_ ? language_->id : -1;
}

const Language& Document::language() const { return *language_; }

Status Document::setLanguage(std::shared_ptr<const Language> language) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!language) {
    return fail(Status::UnknownLanguage, "unknown language");
  }
  language_ = std::move(language);
  options_.languageId = language_->id;
  analysis_.reset();
  return succeed();
}

bool Document::readOnly() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return options_.readOnly;
}

Status Document::setReadOnly(bool readOnly) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  options_.readOnly = readOnly;
  return succeed();
}

uint32_t Document::tabSize() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return options_.tabSize;
}

bool Document::insertSpaces() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return options_.insertSpaces;
}

size_t Document::maxDocumentBytes() const { return maxDocumentBytes_; }

EditOrigin Document::editOrigin() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return nextOrigin_;
}

Status Document::setEditOrigin(EditOrigin origin) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!isValidOrigin(origin)) {
    return fail(Status::InvalidArgument, "unknown edit origin");
  }
  nextOrigin_ = origin;
  return succeed();
}

Status Document::setProposedText(std::string_view text, EditOrigin origin) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!isValidOrigin(origin)) {
    return fail(Status::InvalidArgument, "unknown edit origin");
  }
  if (!isValidUtf8(text)) {
    return fail(Status::InvalidArgument, "proposed text is not valid UTF-8");
  }
  if (text.size() > maxDocumentBytes_) {
    return fail(Status::OutOfRange, "proposed text exceeds the configured maximum size");
  }
  proposedText_.assign(text.data(), text.size());
  hasProposal_ = true;
  proposalOrigin_ = origin;
  return succeed();
}

bool Document::hasProposal() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return hasProposal_;
}

std::string Document::proposedText() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return proposedText_;
}

std::vector<DiffLine> Document::proposalDiff() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!hasProposal_) {
    return {};
  }
  return diffLines(currentText(), proposedText_);
}

Status Document::acceptProposal() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!hasProposal_) {
    return fail(Status::InvalidArgument, "document has no proposed changes");
  }
  const std::string proposed = proposedText_;
  return applyContent(proposed, ChangeKind::ProposalAccepted, proposalOrigin_);
}

Status Document::rejectProposal() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!hasProposal_) {
    return succeed();
  }
  const EditOrigin origin = proposalOrigin_;
  clearTransientState();
  emitChange(ChangeKind::ProposalRejected, 0, 0, 0, origin);
  return succeed();
}

GhostTextState Document::ghostText() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return ghost_;
}

Status Document::setGhostText(size_t offset, std::string_view text, EditOrigin origin) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!isValidOrigin(origin)) {
    return fail(Status::InvalidArgument, "unknown edit origin");
  }
  if (offset > buffer_.size()) {
    return fail(Status::OutOfRange, "ghost text offset is outside the document");
  }
  if (!isValidUtf8(text)) {
    return fail(Status::InvalidArgument, "ghost text is not valid UTF-8");
  }
  const std::string& existing = currentText();
  if (offset < existing.size() &&
      isContinuation(static_cast<unsigned char>(existing[offset]))) {
    return fail(Status::InvalidArgument, "ghost text offset splits a UTF-8 code point");
  }
  if (text.size() > maxDocumentBytes_) {
    return fail(Status::OutOfRange, "ghost text exceeds the configured maximum size");
  }
  ghost_.active = !text.empty();
  ghost_.offset = offset;
  ghost_.text.assign(text.data(), text.size());
  ghost_.origin = origin;
  return succeed();
}

Status Document::acceptGhostText(size_t maxCodePoints) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!ghost_.active) {
    return fail(Status::InvalidArgument, "document has no ghost text");
  }
  if (options_.readOnly) {
    return fail(Status::ReadOnly, "document is read-only");
  }
  if (ghost_.offset > buffer_.size()) {
    return fail(Status::OutOfRange, "ghost text offset is outside the document");
  }

  size_t take = ghost_.text.size();
  if (maxCodePoints > 0) {
    take = 0;
    while (take < ghost_.text.size() && maxCodePoints > 0) {
      take = nextCodepointOffset(ghost_.text, take);
      --maxCodePoints;
    }
  }
  const std::string inserted = ghost_.text.substr(0, take);
  const EditOrigin origin = ghost_.origin;
  const size_t offset = ghost_.offset;
  ghost_ = GhostTextState{};
  if (inserted.empty()) {
    return succeed();
  }
  if (buffer_.size() + inserted.size() > maxDocumentBytes_) {
    return fail(Status::OutOfRange, "document exceeds the configured maximum size");
  }
  buffer_.insert(offset, inserted);
  lineIndex_.applyEdit(offset, offset, inserted);
  textDirty_ = true;
  analysis_.reset();
  version_ += 1;
  recordEdit(offset, std::string(), inserted, false, origin);
  emitChange(ChangeKind::GhostTextAccepted, offset, 0, inserted.size(), origin);
  return succeed();
}

Status Document::dismissGhostText() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!ghost_.active) {
    return succeed();
  }
  const EditOrigin origin = ghost_.origin;
  ghost_ = GhostTextState{};
  emitChange(ChangeKind::GhostTextDismissed, 0, 0, 0, origin);
  return succeed();
}

Status Document::insert(size_t offset, std::string_view text) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (options_.readOnly) {
    return fail(Status::ReadOnly, "document is read-only");
  }
  if (offset > buffer_.size()) {
    return fail(Status::OutOfRange, "insert offset is outside the document");
  }
  if (!isValidUtf8(text)) {
    return fail(Status::InvalidArgument, "inserted text is not valid UTF-8");
  }
  if (buffer_.size() + text.size() > maxDocumentBytes_) {
    return fail(Status::OutOfRange, "document exceeds the configured maximum size");
  }
  if (text.empty()) {
    return succeed();
  }
  const std::string& existing = currentText();
  if (offset < existing.size() &&
      isContinuation(static_cast<unsigned char>(existing[offset]))) {
    return fail(Status::InvalidArgument, "insert offset splits a UTF-8 code point");
  }

  const EditOrigin origin = nextOrigin_;
  clearTransientState();
  buffer_.insert(offset, text);
  lineIndex_.applyEdit(offset, offset, text);
  textDirty_ = true;
  analysis_.reset();
  version_ += 1;
  recordEdit(offset, std::string(), std::string(text), true, origin);
  emitChange(ChangeKind::Insert, offset, 0, text.size(), origin);
  return succeed();
}

Status Document::erase(size_t start, size_t end) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (options_.readOnly) {
    return fail(Status::ReadOnly, "document is read-only");
  }
  if (start > end || end > buffer_.size()) {
    return fail(Status::OutOfRange, "erase range is outside the document");
  }
  if (start == end) {
    return succeed();
  }
  const std::string& existing = currentText();
  if ((start < existing.size() &&
       isContinuation(static_cast<unsigned char>(existing[start]))) ||
      (end < existing.size() &&
       isContinuation(static_cast<unsigned char>(existing[end])))) {
    return fail(Status::InvalidArgument, "erase range splits a UTF-8 code point");
  }
  const EditOrigin origin = nextOrigin_;
  const std::string removed = buffer_.slice(start, end - start);
  clearTransientState();
  buffer_.erase(start, end - start);
  lineIndex_.applyEdit(start, end, std::string_view());
  textDirty_ = true;
  analysis_.reset();
  version_ += 1;
  recordEdit(start, removed, std::string(), false, origin);
  emitChange(ChangeKind::Erase, start, removed.size(), 0, origin);
  return succeed();
}

Status Document::replace(size_t start, size_t end, std::string_view text) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (options_.readOnly) {
    return fail(Status::ReadOnly, "document is read-only");
  }
  if (start > end || end > buffer_.size()) {
    return fail(Status::OutOfRange, "replace range is outside the document");
  }
  if (!isValidUtf8(text)) {
    return fail(Status::InvalidArgument, "replacement text is not valid UTF-8");
  }
  if (buffer_.size() - (end - start) + text.size() > maxDocumentBytes_) {
    return fail(Status::OutOfRange, "document exceeds the configured maximum size");
  }
  if (start == end && text.empty()) {
    return succeed();
  }
  const std::string& existing = currentText();
  if ((start < existing.size() &&
       isContinuation(static_cast<unsigned char>(existing[start]))) ||
      (end < existing.size() &&
       isContinuation(static_cast<unsigned char>(existing[end])))) {
    return fail(Status::InvalidArgument, "replace range splits a UTF-8 code point");
  }

  const EditOrigin origin = nextOrigin_;
  const std::string removed = buffer_.slice(start, end - start);
  clearTransientState();
  buffer_.erase(start, end - start);
  buffer_.insert(start, text);
  lineIndex_.applyEdit(start, end, text);
  textDirty_ = true;
  analysis_.reset();
  version_ += 1;
  recordEdit(start, removed, std::string(text), false, origin);
  emitChange(ChangeKind::Replace, start, removed.size(), text.size(), origin);
  return succeed();
}

bool Document::canUndo() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return !undoStack_.empty();
}

bool Document::canRedo() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return !redoStack_.empty();
}

Status Document::undo() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (options_.readOnly) {
    return fail(Status::ReadOnly, "document is read-only");
  }
  if (undoStack_.empty()) {
    return succeed();
  }
  EditRecord record = std::move(undoStack_.back());
  undoStack_.pop_back();
  clearTransientState();
  buffer_.erase(record.offset, record.inserted.size());
  buffer_.insert(record.offset, record.removed);
  lineIndex_.applyEdit(record.offset, record.offset + record.inserted.size(),
                       record.removed);
  textDirty_ = true;
  analysis_.reset();
  redoStack_.push_back(record);
  version_ += 1;
  lastEditTick_ = 0;
  emitChange(ChangeKind::Undo, record.offset, record.inserted.size(), record.removed.size(),
             record.origin);
  return succeed();
}

Status Document::redo() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (options_.readOnly) {
    return fail(Status::ReadOnly, "document is read-only");
  }
  if (redoStack_.empty()) {
    return succeed();
  }
  EditRecord record = std::move(redoStack_.back());
  redoStack_.pop_back();
  clearTransientState();
  buffer_.erase(record.offset, record.removed.size());
  buffer_.insert(record.offset, record.inserted);
  lineIndex_.applyEdit(record.offset, record.offset + record.removed.size(),
                       record.inserted);
  textDirty_ = true;
  analysis_.reset();
  undoStack_.push_back(record);
  version_ += 1;
  lastEditTick_ = 0;
  emitChange(ChangeKind::Redo, record.offset, record.removed.size(), record.inserted.size(),
             record.origin);
  return succeed();
}

void Document::clearHistory() {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  undoStack_.clear();
  redoStack_.clear();
  lastEditTick_ = 0;
}

void Document::setHistoryLimit(size_t limit) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  historyLimit_ = limit;
  if (historyLimit_ == 0) {
    undoStack_.clear();
    redoStack_.clear();
  } else {
    while (undoStack_.size() > historyLimit_) {
      undoStack_.erase(undoStack_.begin());
    }
  }
}

void Document::recordEdit(size_t offset, std::string removed, std::string inserted,
                          bool coalesce, EditOrigin origin) {
  redoStack_.clear();
  if (historyLimit_ == 0) {
    return;
  }
  const uint64_t now = steadyTicks();
  if (coalesce && !undoStack_.empty() && removed.empty() && !inserted.empty()) {
    EditRecord& previous = undoStack_.back();
    const bool adjacent = offset == previous.offset + previous.inserted.size();
    const bool recent = lastEditTick_ != 0 && now > lastEditTick_ &&
                        now - lastEditTick_ < 1'000'000'000ull;
    if (previous.removed.empty() && adjacent && recent && previous.origin == origin &&
        previous.inserted.size() < 256) {
      previous.inserted += inserted;
      lastEditTick_ = now;
      return;
    }
  }
  undoStack_.push_back(EditRecord{offset, std::move(removed), std::move(inserted), origin});
  lastEditTick_ = now;
  while (undoStack_.size() > historyLimit_) {
    undoStack_.erase(undoStack_.begin());
  }
}

size_t Document::lineCount() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return lineIndex_.lineCount();
}

Status Document::lineAt(size_t index, LineInfo* out) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!out) {
    return fail(Status::InvalidArgument, "line output is null");
  }
  if (index >= lineIndex_.lineCount()) {
    return fail(Status::OutOfRange, "line index is outside the document");
  }
  const LineSpan span = lineIndex_.line(index, currentText());
  out->start = span.start;
  out->contentEnd = span.contentEnd;
  out->end = span.end;
  return succeed();
}

Status Document::offsetToPosition(size_t offset, Position* out) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!out) {
    return fail(Status::InvalidArgument, "position output is null");
  }
  if (offset > buffer_.size()) {
    return fail(Status::OutOfRange, "offset is outside the document");
  }
  const std::string& text = currentText();
  const size_t lineIndex = lineIndex_.lineOfOffset(offset);
  const LineSpan span = lineIndex_.line(lineIndex, text);
  out->line = static_cast<uint32_t>(lineIndex);
  out->column = static_cast<uint32_t>(codepointCount(text, span.start, offset));
  return succeed();
}

Status Document::positionToOffset(uint32_t line, uint32_t column, size_t* out) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!out) {
    return fail(Status::InvalidArgument, "offset output is null");
  }
  if (line >= lineIndex_.lineCount()) {
    return fail(Status::OutOfRange, "line index is outside the document");
  }
  const std::string& text = currentText();
  const LineSpan span = lineIndex_.line(line, text);
  size_t offset = span.start;
  uint32_t currentColumn = 0;
  while (currentColumn < column && offset < span.contentEnd) {
    offset = nextCodepointOffset(text, offset);
    ++currentColumn;
  }
  *out = offset;
  return succeed();
}

Selection Document::selection() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return selection_;
}

Status Document::setSelection(uint32_t anchor, uint32_t focus) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  const size_t size = buffer_.size();
  if (anchor > size || focus > size) {
    return fail(Status::OutOfRange, "selection is outside the document");
  }
  const std::string& text = currentText();
  if ((anchor < size && isContinuation(static_cast<unsigned char>(text[anchor]))) ||
      (focus < size && isContinuation(static_cast<unsigned char>(text[focus])))) {
    return fail(Status::InvalidArgument, "selection splits a UTF-8 code point");
  }
  selection_ = Selection{anchor, focus};
  return succeed();
}

Status Document::moveCursor(CursorMove move, bool extendSelection) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  const std::string& text = currentText();
  const size_t size = text.size();
  size_t focus = std::min<size_t>(selection_.focus, size);
  size_t anchor = std::min<size_t>(selection_.anchor, size);
  size_t target = focus;

  switch (move) {
    case CursorMove::Left: {
      if (!extendSelection && anchor != focus) {
        target = std::min(anchor, focus);
      } else {
        target = previousCodepointOffset(text, focus);
      }
      break;
    }
    case CursorMove::Right: {
      if (!extendSelection && anchor != focus) {
        target = std::max(anchor, focus);
      } else {
        target = nextCodepointOffset(text, focus);
      }
      break;
    }
    case CursorMove::WordLeft: {
      target = focus;
      while (target > 0) {
        const size_t previous = previousCodepointOffset(text, target);
        if (!isWordByte(static_cast<unsigned char>(text[previous]))) {
          target = previous;
        } else {
          break;
        }
      }
      while (target > 0) {
        const size_t previous = previousCodepointOffset(text, target);
        if (isWordByte(static_cast<unsigned char>(text[previous]))) {
          target = previous;
        } else {
          break;
        }
      }
      break;
    }
    case CursorMove::WordRight: {
      target = focus;
      while (target < size) {
        if (!isWordByte(static_cast<unsigned char>(text[target]))) {
          target = nextCodepointOffset(text, target);
        } else {
          break;
        }
      }
      while (target < size) {
        if (isWordByte(static_cast<unsigned char>(text[target]))) {
          target = nextCodepointOffset(text, target);
        } else {
          break;
        }
      }
      break;
    }
    case CursorMove::LineStart: {
      const LineSpan span = lineIndex_.line(lineIndex_.lineOfOffset(focus), text);
      target = span.start;
      break;
    }
    case CursorMove::LineEnd: {
      const LineSpan span = lineIndex_.line(lineIndex_.lineOfOffset(focus), text);
      target = span.contentEnd;
      break;
    }
    case CursorMove::DocumentStart: {
      target = 0;
      break;
    }
    case CursorMove::DocumentEnd: {
      target = size;
      break;
    }
    case CursorMove::Up:
    case CursorMove::Down: {
      const size_t lineNumber = lineIndex_.lineOfOffset(focus);
      const LineSpan current = lineIndex_.line(lineNumber, text);
      const uint32_t column = static_cast<uint32_t>(
          codepointCount(text, current.start, focus));
      uint32_t targetLine = static_cast<uint32_t>(lineNumber);
      if (move == CursorMove::Up) {
        if (targetLine == 0) {
          target = 0;
          break;
        }
        --targetLine;
      } else {
        if (targetLine + 1 >= lineIndex_.lineCount()) {
          target = size;
          break;
        }
        ++targetLine;
      }
      const LineSpan span = lineIndex_.line(targetLine, text);
      size_t resolved = span.start;
      uint32_t currentColumn = 0;
      while (currentColumn < column && resolved < span.contentEnd) {
        resolved = nextCodepointOffset(text, resolved);
        ++currentColumn;
      }
      target = std::min(resolved, span.contentEnd);
      break;
    }
    default:
      return fail(Status::InvalidArgument, "unknown cursor movement");
  }

  focus = std::min(target, size);
  if (!extendSelection) {
    anchor = focus;
  }
  selection_ = Selection{static_cast<uint32_t>(anchor), static_cast<uint32_t>(focus)};
  return succeed();
}

Status Document::matchBracket(size_t offset, size_t* outMatch) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!outMatch) {
    return fail(Status::InvalidArgument, "match output is null");
  }
  if (offset > buffer_.size()) {
    return fail(Status::OutOfRange, "offset is outside the document");
  }
  size_t match = 0;
  if (!currentAnalysis()->matchBracket(offset, &match)) {
    return fail(Status::Unsupported, "no matching bracket");
  }
  *outMatch = match;
  return succeed();
}

std::vector<Token> Document::tokenize(size_t start, size_t end) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (start > end || end > buffer_.size()) {
    return {};
  }
  return currentAnalysis()->tokenize(start, end);
}

std::vector<CompletionItem> Document::complete(size_t offset) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  offset = std::min(offset, buffer_.size());
  return currentAnalysis()->complete(offset);
}

std::vector<Diagnostic> Document::diagnose() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return currentAnalysis()->diagnose();
}

std::vector<FoldingRange> Document::foldingRanges() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return currentAnalysis()->foldingRanges();
}

Status Document::find(std::string_view query, const SearchOptions& options,
                      std::vector<Range>* out) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!out) {
    return fail(Status::InvalidArgument, "search output is null");
  }
  const Status status = currentAnalysis()->find(query, options, out);
  if (status != Status::Ok) {
    out->clear();
    return fail(status, "invalid regular expression");
  }
  return succeed();
}

std::shared_ptr<Snapshot> Document::createSnapshot() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return std::make_shared<Snapshot>(currentText(), language_, version_);
}

Status Document::restoreSnapshot(const Snapshot& snapshot) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (options_.readOnly) {
    return fail(Status::ReadOnly, "document is read-only");
  }
  const EditOrigin origin = nextOrigin_;
  if (snapshot.language() && snapshot.languageId() != languageId()) {
    const Status status = setLanguage(snapshot.language());
    if (status != Status::Ok) {
      return status;
    }
  }
  return applyContent(snapshot.text(), ChangeKind::SnapshotRestored, origin);
}

Status Document::applyContent(std::string_view text, ChangeKind kind, EditOrigin origin) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (options_.readOnly) {
    return fail(Status::ReadOnly, "document is read-only");
  }
  if (!isValidUtf8(text)) {
    return fail(Status::InvalidArgument, "text is not valid UTF-8");
  }
  if (text.size() > maxDocumentBytes_) {
    return fail(Status::OutOfRange, "document exceeds the configured maximum size");
  }

  const std::string& existing = currentText();
  if (existing == text) {
    return succeed();
  }

  const size_t maxPrefix = std::min(existing.size(), text.size());
  size_t prefix = 0;
  while (prefix < maxPrefix && existing[prefix] == text[prefix]) {
    ++prefix;
  }
  while (prefix > 0 &&
         ((prefix < existing.size() &&
           isContinuation(static_cast<unsigned char>(existing[prefix]))) ||
          (prefix < text.size() &&
           isContinuation(static_cast<unsigned char>(text[prefix]))))) {
    --prefix;
  }

  const size_t maxSuffix = std::min(existing.size(), text.size()) - prefix;
  size_t suffix = 0;
  while (suffix < maxSuffix &&
         existing[existing.size() - 1 - suffix] == text[text.size() - 1 - suffix]) {
    ++suffix;
  }
  while (suffix > 0 &&
         (isContinuation(static_cast<unsigned char>(existing[existing.size() - suffix])) ||
          isContinuation(static_cast<unsigned char>(text[text.size() - suffix])))) {
    --suffix;
  }

  const size_t start = prefix;
  const size_t oldEnd = existing.size() - suffix;
  const std::string removed = buffer_.slice(start, oldEnd - start);
  const std::string inserted(text.substr(prefix, text.size() - prefix - suffix));

  clearTransientState();
  buffer_.erase(start, oldEnd - start);
  buffer_.insert(start, inserted);
  lineIndex_.applyEdit(start, oldEnd, inserted);
  textDirty_ = true;
  analysis_.reset();
  version_ += 1;
  selection_ = Selection{};
  recordEdit(start, removed, inserted, false, origin);
  emitChange(kind, start, removed.size(), inserted.size(), origin);
  return succeed();
}

void Document::clearTransientState() {
  proposedText_.clear();
  hasProposal_ = false;
  ghost_ = GhostTextState{};
}

void Document::setChangeListener(ChangeListener listener, void* userData) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  listener_ = listener;
  listenerUserData_ = userData;
}

const char* Document::lastError() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return lastError_ ? lastError_ : "";
}

void Document::emitChange(ChangeKind kind, size_t start, size_t removed, size_t inserted,
                          EditOrigin origin) {
  if (!listener_) {
    return;
  }
  ChangeEvent event;
  event.version = version_;
  event.kind = static_cast<uint32_t>(kind);
  event.start = static_cast<uint32_t>(start);
  event.removedLength = static_cast<uint32_t>(removed);
  event.insertedLength = static_cast<uint32_t>(inserted);
  event.origin = static_cast<uint32_t>(origin);
  listener_(listenerUserData_, &event);
}

}  // namespace codecore
