#include "core/document/document.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <utility>

namespace codecore {
namespace {

constexpr size_t kNoOffset = std::numeric_limits<size_t>::max();

bool isContinuation(unsigned char c) { return (c & 0xC0u) == 0x80u; }

char asciiLower(char c) {
  if (c >= 'A' && c <= 'Z') {
    return static_cast<char>(c - 'A' + 'a');
  }
  return c;
}

std::string asciiLowerCopy(std::string_view text) {
  std::string lower(text);
  std::transform(lower.begin(), lower.end(), lower.begin(), asciiLower);
  return lower;
}

bool isWordByte(unsigned char c) {
  return std::isalnum(c) != 0 || c == '_' || c == '$';
}

bool isIdentifierByte(unsigned char c) { return isWordByte(c); }

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

size_t codepointCount(const std::string& text) {
  size_t count = 0;
  for (size_t index = 0; index < text.size();) {
    index = nextCodepointOffset(text, index);
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

char bracketFor(char opening) {
  switch (opening) {
    case '(':
      return ')';
    case '[':
      return ']';
    case '{':
      return '}';
    default:
      return '\0';
  }
}

bool isOpeningBracket(char c) { return c == '(' || c == '[' || c == '{'; }
bool isClosingBracket(char c) { return c == ')' || c == ']' || c == '}'; }
bool isBracket(char c) { return isOpeningBracket(c) || isClosingBracket(c); }

const char* completionDetail(CompletionKind kind) {
  switch (kind) {
    case CompletionKind::Keyword:
      return "keyword";
    case CompletionKind::Type:
      return "type";
    case CompletionKind::Builtin:
      return "builtin";
    case CompletionKind::Function:
      return "function";
    case CompletionKind::Class:
      return "class";
    case CompletionKind::Property:
      return "property";
    case CompletionKind::Symbol:
    default:
      return "symbol";
  }
}

int kindRank(CompletionKind kind) {
  switch (kind) {
    case CompletionKind::Class:
    case CompletionKind::Function:
      return 2;
    case CompletionKind::Type:
    case CompletionKind::Property:
      return 1;
    default:
      return 0;
  }
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
}

Document::~Document() = default;

Status Document::fail(Status status, const char* message) const {
  lastError_ = message;
  return status;
}

Status Document::succeed() const {
  lastError_ = nullptr;
  return Status::Ok;
}

std::string Document::allText() const { return buffer_.toString(); }

LexResult Document::lexAll() const {
  const std::string text = allText();
  return lex(text, *language_);
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
  buffer_.reset(text);
  version_ += 1;
  selection_ = Selection{};
  undoStack_.clear();
  redoStack_.clear();
  lastEditTick_ = 0;
  invalidateLines();
  emitChange(ChangeKind::SetText, 0, 0, text.size());
  return succeed();
}

std::string Document::text() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  return buffer_.toString();
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
  if (!text.empty()) {
    const std::string existing = buffer_.toString();
    if (offset < existing.size() && isContinuation(static_cast<unsigned char>(existing[offset]))) {
      return fail(Status::InvalidArgument, "insert offset splits a UTF-8 code point");
    }
  }
  buffer_.insert(offset, text);
  version_ += 1;
  recordEdit(offset, std::string(), std::string(text), true);
  invalidateLines();
  emitChange(ChangeKind::Insert, offset, 0, text.size());
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
  const std::string existing = buffer_.toString();
  if ((start < existing.size() &&
       isContinuation(static_cast<unsigned char>(existing[start]))) ||
      (end < existing.size() && isContinuation(static_cast<unsigned char>(existing[end])))) {
    return fail(Status::InvalidArgument, "erase range splits a UTF-8 code point");
  }
  const std::string removed = buffer_.slice(start, end - start);
  buffer_.erase(start, end - start);
  version_ += 1;
  recordEdit(start, removed, std::string(), false);
  invalidateLines();
  emitChange(ChangeKind::Erase, start, removed.size(), 0);
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
  const std::string existing = buffer_.toString();
  if ((start < existing.size() &&
       isContinuation(static_cast<unsigned char>(existing[start]))) ||
      (end < existing.size() && isContinuation(static_cast<unsigned char>(existing[end])))) {
    return fail(Status::InvalidArgument, "replace range splits a UTF-8 code point");
  }
  const std::string removed = buffer_.slice(start, end - start);
  buffer_.erase(start, end - start);
  buffer_.insert(start, text);
  version_ += 1;
  recordEdit(start, removed, std::string(text), false);
  invalidateLines();
  emitChange(ChangeKind::Replace, start, removed.size(), text.size());
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
  buffer_.erase(record.offset, record.inserted.size());
  buffer_.insert(record.offset, record.removed);
  redoStack_.push_back(record);
  version_ += 1;
  lastEditTick_ = 0;
  invalidateLines();
  emitChange(ChangeKind::Undo, record.offset, record.inserted.size(), record.removed.size());
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
  buffer_.erase(record.offset, record.removed.size());
  buffer_.insert(record.offset, record.inserted);
  undoStack_.push_back(record);
  version_ += 1;
  lastEditTick_ = 0;
  invalidateLines();
  emitChange(ChangeKind::Redo, record.offset, record.removed.size(), record.inserted.size());
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
                          bool coalesce) {
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
    if (previous.removed.empty() && adjacent && recent && previous.inserted.size() < 256) {
      previous.inserted += inserted;
      lastEditTick_ = now;
      return;
    }
  }
  undoStack_.push_back(EditRecord{offset, std::move(removed), std::move(inserted)});
  lastEditTick_ = now;
  while (undoStack_.size() > historyLimit_) {
    undoStack_.erase(undoStack_.begin());
  }
}

void Document::invalidateLines() const { linesDirty_ = true; }

void Document::ensureLines() const {
  if (!linesDirty_) {
    return;
  }
  const std::string text = buffer_.toString();
  lines_.clear();
  size_t start = 0;
  for (size_t index = 0; index < text.size(); ++index) {
    if (text[index] == '\n') {
      size_t contentEnd = index;
      if (index > start && text[index - 1] == '\r') {
        contentEnd = index - 1;
      }
      lines_.push_back(LineInfo{start, contentEnd, index + 1});
      start = index + 1;
    }
  }
  lines_.push_back(LineInfo{start, text.size(), text.size()});
  linesDirty_ = false;
}

size_t Document::lineOfOffset(size_t offset) const {
  ensureLines();
  size_t low = 0;
  size_t high = lines_.size();
  while (low + 1 < high) {
    const size_t middle = low + (high - low) / 2;
    if (lines_[middle].start <= offset) {
      low = middle;
    } else {
      high = middle;
    }
  }
  return low;
}

size_t Document::lineCount() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  ensureLines();
  return lines_.size();
}

Status Document::lineAt(size_t index, LineInfo* out) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!out) {
    return fail(Status::InvalidArgument, "line output is null");
  }
  ensureLines();
  if (index >= lines_.size()) {
    return fail(Status::OutOfRange, "line index is outside the document");
  }
  *out = lines_[index];
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
  ensureLines();
  const size_t lineIndex = lineOfOffset(offset);
  const size_t lineStart = lines_[lineIndex].start;
  const std::string prefix = buffer_.slice(lineStart, offset - lineStart);
  out->line = static_cast<uint32_t>(lineIndex);
  out->column = static_cast<uint32_t>(codepointCount(prefix));
  return succeed();
}

Status Document::positionToOffset(uint32_t line, uint32_t column, size_t* out) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (!out) {
    return fail(Status::InvalidArgument, "offset output is null");
  }
  ensureLines();
  if (line >= lines_.size()) {
    return fail(Status::OutOfRange, "line index is outside the document");
  }
  const LineInfo& info = lines_[line];
  const std::string text = buffer_.toString();
  size_t offset = info.start;
  uint32_t currentColumn = 0;
  while (currentColumn < column && offset < info.contentEnd) {
    offset = nextCodepointOffset(text, offset);
    ++currentColumn;
  }
  if (offset > info.contentEnd) {
    offset = info.contentEnd;
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
  const std::string text = buffer_.toString();
  if ((anchor < size && isContinuation(static_cast<unsigned char>(text[anchor]))) ||
      (focus < size && isContinuation(static_cast<unsigned char>(text[focus])))) {
    return fail(Status::InvalidArgument, "selection splits a UTF-8 code point");
  }
  selection_ = Selection{anchor, focus};
  return succeed();
}

Status Document::moveCursor(CursorMove move, bool extendSelection) {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  ensureLines();
  const std::string text = buffer_.toString();
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
      target = lines_[lineOfOffset(focus)].start;
      break;
    }
    case CursorMove::LineEnd: {
      target = lines_[lineOfOffset(focus)].contentEnd;
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
      const size_t lineIndex = lineOfOffset(focus);
      const size_t lineStart = lines_[lineIndex].start;
      const std::string prefix = buffer_.slice(lineStart, focus - lineStart);
      const uint32_t column = static_cast<uint32_t>(codepointCount(prefix));
      uint32_t targetLine = static_cast<uint32_t>(lineIndex);
      if (move == CursorMove::Up) {
        if (targetLine == 0) {
          target = 0;
          break;
        }
        --targetLine;
      } else {
        if (targetLine + 1 >= lines_.size()) {
          target = size;
          break;
        }
        ++targetLine;
      }
      size_t resolved = lines_[targetLine].start;
      const size_t contentEnd = lines_[targetLine].contentEnd;
      uint32_t currentColumn = 0;
      while (currentColumn < column && resolved < contentEnd) {
        resolved = nextCodepointOffset(text, resolved);
        ++currentColumn;
      }
      target = std::min(resolved, contentEnd);
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
  const std::string text = buffer_.toString();
  if (offset > text.size()) {
    return fail(Status::OutOfRange, "offset is outside the document");
  }
  if (offset == text.size()) {
    if (offset == 0) {
      return fail(Status::Unsupported, "document is empty");
    }
    --offset;
  }
  if (!isBracket(text[offset])) {
    if (offset > 0 && isBracket(text[offset - 1])) {
      --offset;
    } else {
      return fail(Status::Unsupported, "offset is not on a bracket");
    }
  }
  const char bracket = text[offset];
  const char partner = isOpeningBracket(bracket) ? bracketFor(bracket)
                                                 : (bracket == ')' ? '(' : bracket == ']' ? '[' : '{');

  const LexResult lexed = lex(text, *language_);
  std::vector<size_t> brackets;
  brackets.reserve(lexed.tokens.size() / 4);
  for (const Token& token : lexed.tokens) {
    if (token.kind == TokenKind::Punctuation && token.start < text.size() &&
        isBracket(text[token.start])) {
      brackets.push_back(token.start);
    }
  }
  const auto found = std::find(brackets.begin(), brackets.end(), offset);
  if (found == brackets.end()) {
    return fail(Status::Unsupported, "offset is not on an active bracket");
  }
  const size_t tokenIndex = static_cast<size_t>(std::distance(brackets.begin(), found));

  if (isOpeningBracket(bracket)) {
    int depth = 0;
    for (size_t index = tokenIndex + 1; index < brackets.size(); ++index) {
      const char current = text[brackets[index]];
      if (current == bracket) {
        ++depth;
      } else if (current == partner) {
        if (depth == 0) {
          *outMatch = brackets[index];
          return succeed();
        }
        --depth;
      }
    }
  } else {
    int depth = 0;
    for (size_t index = tokenIndex; index-- > 0;) {
      const char current = text[brackets[index]];
      if (current == bracket) {
        ++depth;
      } else if (current == partner) {
        if (depth == 0) {
          *outMatch = brackets[index];
          return succeed();
        }
        --depth;
      }
    }
  }
  return fail(Status::Unsupported, "no matching bracket");
}

std::vector<Token> Document::tokenize(size_t start, size_t end) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  if (start > end || end > buffer_.size()) {
    return {};
  }
  const LexResult lexed = lexAll();
  std::vector<Token> filtered;
  filtered.reserve(lexed.tokens.size());
  for (const Token& token : lexed.tokens) {
    if (token.end > start && token.start < end) {
      filtered.push_back(token);
    }
  }
  return filtered;
}

std::vector<CompletionItem> Document::complete(size_t offset) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  const std::string text = buffer_.toString();
  offset = std::min(offset, text.size());

  size_t prefixStart = offset;
  while (prefixStart > 0 &&
         isIdentifierByte(static_cast<unsigned char>(text[prefixStart - 1]))) {
    --prefixStart;
  }
  const std::string prefix = text.substr(prefixStart, offset - prefixStart);
  const std::string lowerPrefix = asciiLowerCopy(prefix);

  const LexResult lexed = lex(text, *language_);
  for (const Token& token : lexed.tokens) {
    const bool covers = token.start < offset && offset <= token.end;
    if (covers && (token.kind == TokenKind::Comment || token.kind == TokenKind::String ||
                   token.kind == TokenKind::Invalid)) {
      return {};
    }
  }

  std::unordered_map<std::string, CompletionItem> candidates;
  auto consider = [&](const std::string& label, CompletionKind kind, int32_t score) {
    if (label.empty() || label.size() > 64) {
      return;
    }
    if (!lowerPrefix.empty()) {
      const std::string lowerLabel = asciiLowerCopy(label);
      if (lowerLabel.size() < lowerPrefix.size() ||
          lowerLabel.compare(0, lowerPrefix.size(), lowerPrefix) != 0) {
        return;
      }
    }
    auto existing = candidates.find(label);
    if (existing != candidates.end()) {
      if (score > existing->second.score) {
        existing->second.score = score;
        existing->second.kind = kind;
        existing->second.detail = completionDetail(kind);
      }
      return;
    }
    CompletionItem item;
    item.label = label;
    item.insertText = label;
    item.detail = completionDetail(kind);
    item.kind = kind;
    item.score = score;
    candidates.emplace(label, std::move(item));
  };

  for (const std::string& keyword : language_->keywords) {
    consider(keyword, CompletionKind::Keyword, 40);
  }
  for (const std::string& type : language_->types) {
    consider(type, CompletionKind::Type, 36);
  }
  for (const std::string& builtin : language_->builtins) {
    consider(builtin, CompletionKind::Builtin, 44);
  }

  struct SymbolStat {
    int32_t occurrences = 0;
    CompletionKind kind = CompletionKind::Symbol;
    size_t lastOffset = 0;
  };
  std::unordered_map<std::string, SymbolStat> symbols;
  const auto isDeclarationKeyword = [](const std::string& word, CompletionKind* kind) {
    if (word == "class" || word == "struct" || word == "interface" || word == "trait" ||
        word == "enum" || word == "record" || word == "object") {
      *kind = CompletionKind::Class;
      return true;
    }
    if (word == "def" || word == "function" || word == "fn" || word == "func" ||
        word == "fun" || word == "sub") {
      *kind = CompletionKind::Function;
      return true;
    }
    if (word == "let" || word == "var" || word == "const" || word == "val" ||
        word == "type") {
      *kind = CompletionKind::Symbol;
      return true;
    }
    return false;
  };

  const Token* previous = nullptr;
  for (const Token& token : lexed.tokens) {
    const bool named = token.kind == TokenKind::Identifier || token.kind == TokenKind::Function ||
                       token.kind == TokenKind::Type || token.kind == TokenKind::Constant;
    if (!named) {
      if (token.kind != TokenKind::Comment) {
        previous = &token;
      }
      continue;
    }
    if (token.end > text.size()) {
      continue;
    }
    const std::string label = text.substr(token.start, token.end - token.start);
    if (label.empty() || label.size() > 64) {
      continue;
    }
    CompletionKind kind = CompletionKind::Symbol;
    if (token.kind == TokenKind::Function) {
      kind = CompletionKind::Function;
    } else if (token.kind == TokenKind::Type) {
      kind = CompletionKind::Type;
    }
    if (previous && previous->kind == TokenKind::Keyword) {
      const std::string previousWord =
          text.substr(previous->start, previous->end - previous->start);
      CompletionKind declared = CompletionKind::Symbol;
      if (isDeclarationKeyword(asciiLowerCopy(previousWord), &declared)) {
        kind = declared;
      }
    }
    SymbolStat& stat = symbols[label];
    stat.occurrences += 1;
    stat.lastOffset = token.start;
    if (kindRank(kind) > kindRank(stat.kind)) {
      stat.kind = kind;
    }
    if (token.kind != TokenKind::Keyword) {
      previous = &token;
    }
  }

  for (const auto& entry : symbols) {
    const SymbolStat& stat = entry.second;
    int32_t score = 58 + std::min<int32_t>(stat.occurrences, 20) * 2;
    if (offset > stat.lastOffset && offset - stat.lastOffset < 2000) {
      score += 6;
    }
    consider(entry.first, stat.kind, score);
  }

  std::vector<CompletionItem> result;
  result.reserve(candidates.size());
  for (auto& entry : candidates) {
    result.push_back(std::move(entry.second));
  }
  std::sort(result.begin(), result.end(),
            [](const CompletionItem& left, const CompletionItem& right) {
              if (left.score != right.score) {
                return left.score > right.score;
              }
              return left.label < right.label;
            });
  if (result.size() > 100) {
    result.resize(100);
  }
  return result;
}

std::vector<Diagnostic> Document::diagnose() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  const std::string text = buffer_.toString();
  const LexResult lexed = lex(text, *language_);
  std::vector<Diagnostic> diagnostics;

  struct OpenBracket {
    char bracket;
    uint32_t offset;
  };
  std::vector<OpenBracket> stack;

  for (const Token& token : lexed.tokens) {
    if (diagnostics.size() >= 200) {
      break;
    }
    if (token.kind != TokenKind::Punctuation || token.start >= text.size()) {
      continue;
    }
    const char current = text[token.start];
    if (isOpeningBracket(current)) {
      stack.push_back(OpenBracket{current, token.start});
    } else if (isClosingBracket(current)) {
      if (stack.empty()) {
        diagnostics.push_back(Diagnostic{token.start, token.start + 1, 1,
                                         std::string("Unmatched '") + current + "'", "codecore"});
        continue;
      }
      const char opening = stack.back().bracket;
      if (bracketFor(opening) == current) {
        stack.pop_back();
      } else {
        diagnostics.push_back(Diagnostic{token.start, token.start + 1, 1,
                                         std::string("Mismatched '") + current + "', expected '" +
                                             bracketFor(opening) + "'",
                                         "codecore"});
      }
    }
  }

  for (const OpenBracket& open : stack) {
    if (diagnostics.size() >= 200) {
      break;
    }
    diagnostics.push_back(Diagnostic{open.offset, open.offset + 1, 1,
                                     std::string("Unclosed '") + open.bracket + "'", "codecore"});
  }

  if (lexed.unterminatedString) {
    const uint32_t start = std::min<uint32_t>(lexed.unterminatedStringStart,
                                              static_cast<uint32_t>(text.size()));
    diagnostics.push_back(Diagnostic{start, static_cast<uint32_t>(text.size()), 1,
                                     "Unterminated string literal", "codecore"});
  }
  if (lexed.unterminatedBlockComment) {
    const uint32_t start = std::min<uint32_t>(lexed.unterminatedBlockStart,
                                              static_cast<uint32_t>(text.size()));
    diagnostics.push_back(Diagnostic{start, static_cast<uint32_t>(text.size()), 1,
                                     "Unterminated block comment", "codecore"});
  }

  std::sort(diagnostics.begin(), diagnostics.end(),
            [](const Diagnostic& left, const Diagnostic& right) {
              if (left.start != right.start) {
                return left.start < right.start;
              }
              return left.message < right.message;
            });
  return diagnostics;
}

std::vector<FoldingRange> Document::foldingRanges() const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  ensureLines();
  const std::string text = buffer_.toString();
  const LexResult lexed = lex(text, *language_);
  std::vector<FoldingRange> ranges;
  std::vector<uint32_t> openLines;

  for (const Token& token : lexed.tokens) {
    if (ranges.size() >= 500) {
      break;
    }
    if (token.start >= text.size()) {
      continue;
    }
    if (token.kind == TokenKind::Punctuation) {
      const char current = text[token.start];
      if (current == '{') {
        openLines.push_back(static_cast<uint32_t>(lineOfOffset(token.start)));
      } else if (current == '}' && !openLines.empty()) {
        const uint32_t startLine = openLines.back();
        openLines.pop_back();
        const uint32_t endLine = static_cast<uint32_t>(lineOfOffset(token.start));
        if (endLine > startLine) {
          ranges.push_back(FoldingRange{startLine, endLine, 0});
        }
      }
    } else if (token.kind == TokenKind::Comment) {
      const uint32_t startLine = static_cast<uint32_t>(lineOfOffset(token.start));
      const uint32_t endLine =
          static_cast<uint32_t>(lineOfOffset(token.end > 0 ? token.end - 1 : token.start));
      if (endLine > startLine) {
        ranges.push_back(FoldingRange{startLine, endLine, 1});
      }
    }
  }

  std::sort(ranges.begin(), ranges.end(),
            [](const FoldingRange& left, const FoldingRange& right) {
              if (left.startLine != right.startLine) {
                return left.startLine < right.startLine;
              }
              return left.endLine > right.endLine;
            });
  return ranges;
}

std::vector<Range> Document::find(std::string_view query, const SearchOptions& options) const {
  std::lock_guard<std::recursive_mutex> lock(mutex_);
  std::vector<Range> results;
  if (query.empty()) {
    return results;
  }
  const std::string text = buffer_.toString();
  const std::string needle = options.caseSensitive ? std::string(query)
                                                    : asciiLowerCopy(query);
  const std::string haystack = options.caseSensitive ? text : asciiLowerCopy(text);

  size_t position = 0;
  while (position + needle.size() <= haystack.size()) {
    const size_t found = haystack.find(needle, position);
    if (found == std::string::npos) {
      break;
    }
    const size_t matchEnd = found + needle.size();
    bool valid = true;
    if (options.wholeWord) {
      if (found > 0 && isWordByte(static_cast<unsigned char>(text[found - 1]))) {
        valid = false;
      }
      if (matchEnd < text.size() && isWordByte(static_cast<unsigned char>(text[matchEnd]))) {
        valid = false;
      }
    }
    if (valid) {
      results.push_back(Range{static_cast<uint32_t>(found), static_cast<uint32_t>(matchEnd)});
      if (results.size() >= 10000) {
        break;
      }
    }
    position = found + 1;
  }
  return results;
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

void Document::emitChange(ChangeKind kind, size_t start, size_t removed, size_t inserted) {
  if (!listener_) {
    return;
  }
  ChangeEvent event;
  event.version = version_;
  event.kind = static_cast<uint32_t>(kind);
  event.start = static_cast<uint32_t>(start);
  event.removedLength = static_cast<uint32_t>(removed);
  event.insertedLength = static_cast<uint32_t>(inserted);
  listener_(listenerUserData_, &event);
}

}  // namespace codecore
