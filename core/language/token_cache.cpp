#include "core/language/token_cache.h"

#include <algorithm>
#include <utility>

namespace codecore {
namespace {

std::shared_ptr<const TokenSet> makeSet(LexResult lex, size_t relexedBytes) {
  auto set = std::make_shared<TokenSet>();
  set->tokens = std::move(lex.tokens);
  set->unterminatedString = lex.unterminatedString;
  set->unterminatedStringStart = lex.unterminatedStringStart;
  set->unterminatedBlockComment = lex.unterminatedBlockComment;
  set->unterminatedBlockStart = lex.unterminatedBlockStart;
  set->relexedBytes = relexedBytes;
  return set;
}

}  // namespace

void TokenCache::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  text_.clear();
  set_.reset();
  languageId_ = -1;
  safeStarts_.clear();
  safeDepths_.clear();
  lastRelexedBytes_ = 0;
}

std::shared_ptr<const TokenSet> TokenCache::fullLex(const std::string& text,
                                                    const Language& language) {
  LexResult lex = codecore::lex(text, language);
  auto set = makeSet(std::move(lex), text.size());
  rebuildSafeStarts(text, *set);
  text_ = text;
  languageId_ = language.id;
  set_ = set;
  lastRelexedBytes_ = text.size();
  return set;
}

std::shared_ptr<const TokenSet> TokenCache::tokensFor(const std::string& text,
                                                      const Language& language) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (set_ && languageId_ == language.id && text_ == text) {
    lastRelexedBytes_ = 0;
    return set_;
  }

  // Markup, a language switch or a cold cache: no reusable prefix.
  if (!set_ || languageId_ != language.id || language.markup || text.empty() ||
      text_.empty()) {
    return fullLex(text, language);
  }

  const size_t previousSize = text_.size();
  const size_t size = text.size();
  const size_t minSize = std::min(previousSize, size);

  size_t prefix = 0;
  while (prefix < minSize && text_[prefix] == text[prefix]) {
    ++prefix;
  }
  size_t suffix = 0;
  while (suffix < minSize - prefix &&
         text_[previousSize - 1 - suffix] == text[size - 1 - suffix]) {
    ++suffix;
  }

  const size_t previousChangedEnd = previousSize - suffix;
  const size_t changedEnd = size - suffix;
  const int64_t delta = static_cast<int64_t>(size) - static_cast<int64_t>(previousSize);

  // Restart at the closest safe offset at or before the first change.
  size_t restart = 0;
  int32_t restartDepth = 0;
  if (!safeStarts_.empty()) {
    const auto bound = std::upper_bound(safeStarts_.begin(), safeStarts_.end(), prefix);
    if (bound != safeStarts_.begin()) {
      const size_t index = static_cast<size_t>(std::distance(safeStarts_.begin(), bound)) - 1;
      restart = safeStarts_[index];
      restartDepth = safeDepths_[index];
    }
  }

  // Tokens entirely before the restart point are untouched by definition: the
  // restart is the first safe boundary at or before the first changed byte.
  std::vector<Token> prefixTokens;
  {
    const auto first = std::lower_bound(
        set_->tokens.begin(), set_->tokens.end(), restart,
        [](const Token& token, size_t offset) { return token.start < offset; });
    prefixTokens.assign(set_->tokens.begin(), first);
  }

  // Lex forward from the restart point until the lexical state agrees with the
  // old state at a safe boundary again; everything past that boundary is
  // reused with shifted offsets.
  LexResult run;
  size_t lexEnd = restart;
  int32_t endDepth = restartDepth;
  size_t stopLimit = changedEnd;
  size_t splice = previousSize;
  bool reachedEnd = false;

  for (int iteration = 0; iteration < 8; ++iteration) {
    const size_t partStart = lexEnd;
    LexResult part =
        lexFrom(text, language, partStart, stopLimit, endDepth, &lexEnd, &endDepth);
    run.tokens.insert(run.tokens.end(), part.tokens.begin(), part.tokens.end());
    // An unterminated literal consumes to end of text, so a later part cannot
    // legitimately clear the flag: keep the first report.
    if (part.unterminatedString && !run.unterminatedString) {
      run.unterminatedString = true;
      run.unterminatedStringStart = part.unterminatedStringStart;
    }
    if (part.unterminatedBlockComment && !run.unterminatedBlockComment) {
      run.unterminatedBlockComment = true;
      run.unterminatedBlockStart = part.unterminatedBlockStart;
    }

    if (lexEnd >= size) {
      reachedEnd = true;
      splice = previousSize;
      break;
    }

    // Map the new offset back into old-text coordinates through the unchanged
    // suffix, then take the first safe old boundary with matching depth.
    size_t oldResume = previousChangedEnd;
    const int64_t mapped = static_cast<int64_t>(lexEnd) - delta;
    if (mapped > 0) {
      oldResume = std::max(oldResume, static_cast<size_t>(mapped));
    }

    size_t candidate = previousSize;
    auto it = std::lower_bound(safeStarts_.begin(), safeStarts_.end(), oldResume);
    for (; it != safeStarts_.end(); ++it) {
      const size_t index = static_cast<size_t>(std::distance(safeStarts_.begin(), it));
      if (safeDepths_[index] == endDepth) {
        candidate = *it;
        break;
      }
    }

    if (candidate >= previousSize) {
      splice = previousSize;
      break;
    }

    const int64_t newCandidate = static_cast<int64_t>(candidate) + delta;
    if (newCandidate < 0) {
      splice = previousSize;
      break;
    }
    if (lexEnd == static_cast<size_t>(newCandidate)) {
      splice = candidate;
      break;
    }
    if (lexEnd > static_cast<size_t>(newCandidate)) {
      // A token ran across the candidate boundary, so the two revisions are
      // not aligned there. Lex to the end; the merged token tail differs.
      stopLimit = size;
      splice = previousSize;
      continue;
    }

    // Not covered yet: keep lexing up to the reuse point.
    stopLimit = static_cast<size_t>(newCandidate);
    if (iteration == 7) {
      splice = previousSize;
    }
  }

  if (splice >= previousSize && !reachedEnd) {
    // No reusable boundary: finish lexing to the end of the document.
    LexResult tail = lexFrom(text, language, lexEnd, size, endDepth, &lexEnd, &endDepth);
    run.tokens.insert(run.tokens.end(), tail.tokens.begin(), tail.tokens.end());
    if (tail.unterminatedString && !run.unterminatedString) {
      run.unterminatedString = true;
      run.unterminatedStringStart = tail.unterminatedStringStart;
    }
    if (tail.unterminatedBlockComment && !run.unterminatedBlockComment) {
      run.unterminatedBlockComment = true;
      run.unterminatedBlockStart = tail.unterminatedBlockStart;
    }
  }

  auto set = std::make_shared<TokenSet>();
  set->reusedTokens = prefixTokens.size();

  if (splice >= previousSize) {
    set->tokens = std::move(prefixTokens);
    set->tokens.reserve(set->tokens.size() + run.tokens.size());
    set->tokens.insert(set->tokens.end(), run.tokens.begin(), run.tokens.end());
    set->unterminatedString = run.unterminatedString;
    set->unterminatedStringStart = run.unterminatedStringStart;
    set->unterminatedBlockComment = run.unterminatedBlockComment;
    set->unterminatedBlockStart = run.unterminatedBlockStart;
  } else {
    const size_t newSplice = static_cast<size_t>(static_cast<int64_t>(splice) + delta);

    // A token may have run past the convergence point; drop it so the reused
    // tail cannot overlap.
    while (!run.tokens.empty() && run.tokens.back().end > newSplice) {
      run.tokens.pop_back();
    }

    const auto first = std::lower_bound(
        set_->tokens.begin(), set_->tokens.end(), splice,
        [](const Token& token, size_t offset) { return token.start < offset; });
    const size_t reused = static_cast<size_t>(std::distance(first, set_->tokens.end()));

    set->reusedTokens = prefixTokens.size() + reused;
    set->tokens = std::move(prefixTokens);
    set->tokens.reserve(set->tokens.size() + run.tokens.size() + reused);
    set->tokens.insert(set->tokens.end(), run.tokens.begin(), run.tokens.end());
    for (auto it = first; it != set_->tokens.end(); ++it) {
      Token token = *it;
      token.start = static_cast<uint32_t>(static_cast<int64_t>(token.start) + delta);
      token.end = static_cast<uint32_t>(static_cast<int64_t>(token.end) + delta);
      set->tokens.push_back(token);
    }

    // The tail is byte-identical to the previous revision, so its
    // unterminated-literal state is still accurate.
    set->unterminatedString = set_->unterminatedString;
    set->unterminatedStringStart = set_->unterminatedStringStart;
    set->unterminatedBlockComment = set_->unterminatedBlockComment;
    set->unterminatedBlockStart = set_->unterminatedBlockStart;
  }

  set->relexedBytes = lexEnd - restart;

  rebuildSafeStarts(text, *set);
  text_ = text;
  set_ = set;
  lastRelexedBytes_ = set->relexedBytes;
  return set;
}

void TokenCache::rebuildSafeStarts(const std::string& text, const TokenSet& set) {
  safeStarts_.clear();
  safeDepths_.clear();
  safeStarts_.push_back(0);
  safeDepths_.push_back(0);
  if (text.empty() || set.tokens.empty()) {
    return;
  }

  const std::vector<Token>& tokens = set.tokens;
  size_t tokenIndex = 0;
  int32_t depth = 0;

  size_t lineStart = text.find('\n');
  while (lineStart != std::string::npos && lineStart + 1 < text.size()) {
    const size_t next = lineStart + 1;

    while (tokenIndex < tokens.size() && tokens[tokenIndex].start < next) {
      const Token& token = tokens[tokenIndex];
      if (token.kind == TokenKind::Punctuation && token.start < text.size()) {
        const char current = text[token.start];
        if (current == '{') {
          ++depth;
        } else if (current == '}') {
          depth = std::max<int32_t>(0, depth - 1);
        }
      }
      ++tokenIndex;
    }

    // Tokens never overlap, so only the last consumed one can span `next`.
    const bool covered = tokenIndex > 0 && tokens[tokenIndex - 1].end > next;

    if (!covered) {
      safeStarts_.push_back(next);
      safeDepths_.push_back(depth);
    }

    lineStart = text.find('\n', next);
  }
}

}  // namespace codecore
