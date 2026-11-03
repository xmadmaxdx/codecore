#include "core/analysis/analysis.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <unordered_map>
#include <utility>

namespace codecore {
namespace {

char asciiLowerChar(char c) {
  if (c >= 'A' && c <= 'Z') {
    return static_cast<char>(c - 'A' + 'a');
  }
  return c;
}

std::string asciiLowerCopy(std::string_view text) {
  std::string lower(text);
  std::transform(lower.begin(), lower.end(), lower.begin(), asciiLowerChar);
  return lower;
}

bool isWordByte(unsigned char c) {
  return std::isalnum(c) != 0 || c == '_' || c == '$';
}

bool isIdentifierByte(unsigned char c) { return isWordByte(c); }

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

}  // namespace

Analysis::Analysis(std::string text, std::shared_ptr<const Language> language,
                   std::shared_ptr<TokenCache> cache)
    : text_(std::move(text)), cache_(std::move(cache)), language_(std::move(language)) {
  lineIndex_.rebuild(text_);
}

int32_t Analysis::languageId() const { return language_ ? language_->id : -1; }

void Analysis::ensureLexed() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (lexed_) {
    return;
  }
  if (cache_ && language_) {
    tokenSet_ = cache_->tokensFor(text_, *language_);
  }
  if (!tokenSet_) {
    LexResult lexed = lex(text_, *language_);
    auto set = std::make_shared<TokenSet>();
    set->tokens = std::move(lexed.tokens);
    set->unterminatedString = lexed.unterminatedString;
    set->unterminatedStringStart = lexed.unterminatedStringStart;
    set->unterminatedBlockComment = lexed.unterminatedBlockComment;
    set->unterminatedBlockStart = lexed.unterminatedBlockStart;
    set->relexedBytes = text_.size();
    tokenSet_ = std::move(set);
  }
  lexed_ = true;
}

const std::vector<Token>& Analysis::tokens() const {
  ensureLexed();
  return tokenSet_->tokens;
}

std::vector<Token> Analysis::tokenize(size_t start, size_t end) const {
  if (start > end || end > text_.size()) {
    return {};
  }
  const std::vector<Token>& all = tokens();
  std::vector<Token> filtered;
  filtered.reserve(all.size());
  for (const Token& token : all) {
    if (token.end > start && token.start < end) {
      filtered.push_back(token);
    }
  }
  return filtered;
}

std::vector<CompletionItem> Analysis::complete(size_t offset) const {
  offset = std::min(offset, text_.size());
  size_t prefixStart = offset;
  while (prefixStart > 0 &&
         isIdentifierByte(static_cast<unsigned char>(text_[prefixStart - 1]))) {
    --prefixStart;
  }
  const std::string prefix = text_.substr(prefixStart, offset - prefixStart);
  const std::string lowerPrefix = asciiLowerCopy(prefix);
  const std::vector<Token>& allTokens = tokens();

  for (const Token& token : allTokens) {
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
  for (const Token& token : allTokens) {
    const bool named = token.kind == TokenKind::Identifier || token.kind == TokenKind::Function ||
                       token.kind == TokenKind::Type || token.kind == TokenKind::Constant;
    if (!named) {
      if (token.kind != TokenKind::Comment) {
        previous = &token;
      }
      continue;
    }
    if (token.end > text_.size()) {
      continue;
    }
    const std::string label = text_.substr(token.start, token.end - token.start);
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
          text_.substr(previous->start, previous->end - previous->start);
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

std::vector<Diagnostic> Analysis::diagnose() const {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (diagnosticsReady_) {
      return diagnostics_;
    }
  }

  ensureLexed();
  std::vector<Diagnostic> diagnostics;
  struct OpenBracket {
    char bracket;
    uint32_t offset;
  };
  std::vector<OpenBracket> stack;

  for (const Token& token : tokens()) {
    if (diagnostics.size() >= 200) {
      break;
    }
    if (token.kind != TokenKind::Punctuation || token.start >= text_.size()) {
      continue;
    }
    const char current = text_[token.start];
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

  if (tokenSet_->unterminatedString) {
    const uint32_t start = std::min<uint32_t>(tokenSet_->unterminatedStringStart,
                                              static_cast<uint32_t>(text_.size()));
    diagnostics.push_back(Diagnostic{start, static_cast<uint32_t>(text_.size()), 1,
                                     "Unterminated string literal", "codecore"});
  }
  if (tokenSet_->unterminatedBlockComment) {
    const uint32_t start = std::min<uint32_t>(tokenSet_->unterminatedBlockStart,
                                              static_cast<uint32_t>(text_.size()));
    diagnostics.push_back(Diagnostic{start, static_cast<uint32_t>(text_.size()), 1,
                                     "Unterminated block comment", "codecore"});
  }

  std::sort(diagnostics.begin(), diagnostics.end(),
            [](const Diagnostic& left, const Diagnostic& right) {
              if (left.start != right.start) {
                return left.start < right.start;
              }
              return left.message < right.message;
            });

  std::lock_guard<std::mutex> lock(mutex_);
  if (!diagnosticsReady_) {
    diagnostics_ = std::move(diagnostics);
    diagnosticsReady_ = true;
  }
  return diagnostics_;
}

std::vector<FoldingRange> Analysis::foldingRanges() const {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (foldingReady_) {
      return folding_;
    }
  }

  const std::vector<Token>& allTokens = tokens();
  std::vector<FoldingRange> ranges;
  std::vector<uint32_t> openLines;

  for (const Token& token : allTokens) {
    if (ranges.size() >= 500) {
      break;
    }
    if (token.start >= text_.size()) {
      continue;
    }
    if (token.kind == TokenKind::Punctuation) {
      const char current = text_[token.start];
      if (current == '{') {
        openLines.push_back(static_cast<uint32_t>(lineIndex_.lineOfOffset(token.start)));
      } else if (current == '}' && !openLines.empty()) {
        const uint32_t startLine = openLines.back();
        openLines.pop_back();
        const uint32_t endLine = static_cast<uint32_t>(lineIndex_.lineOfOffset(token.start));
        if (endLine > startLine) {
          ranges.push_back(FoldingRange{startLine, endLine, 0});
        }
      }
    } else if (token.kind == TokenKind::Comment) {
      const uint32_t startLine = static_cast<uint32_t>(lineIndex_.lineOfOffset(token.start));
      const uint32_t endLine = static_cast<uint32_t>(
          lineIndex_.lineOfOffset(token.end > 0 ? token.end - 1 : token.start));
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

  std::lock_guard<std::mutex> lock(mutex_);
  if (!foldingReady_) {
    folding_ = std::move(ranges);
    foldingReady_ = true;
  }
  return folding_;
}

Status Analysis::find(std::string_view query, const SearchOptions& options,
                      std::vector<Range>* out) const {
  if (!out) {
    return Status::InvalidArgument;
  }
  out->clear();
  if (query.empty()) {
    return Status::Ok;
  }

  if (!options.regex) {
    const std::string needle = options.caseSensitive ? std::string(query)
                                                      : asciiLowerCopy(query);
    const std::string haystack = options.caseSensitive ? text_ : asciiLowerCopy(text_);

    size_t position = 0;
    while (position + needle.size() <= haystack.size()) {
      const size_t found = haystack.find(needle, position);
      if (found == std::string::npos) {
        break;
      }
      const size_t matchEnd = found + needle.size();
      bool valid = true;
      if (options.wholeWord) {
        if (found > 0 && isWordByte(static_cast<unsigned char>(text_[found - 1]))) {
          valid = false;
        }
        if (matchEnd < text_.size() && isWordByte(static_cast<unsigned char>(text_[matchEnd]))) {
          valid = false;
        }
      }
      if (valid) {
        out->push_back(
            Range{static_cast<uint32_t>(found), static_cast<uint32_t>(matchEnd)});
        if (out->size() >= 10000) {
          break;
        }
      }
      position = found + 1;
    }
    return Status::Ok;
  }

  // ECMAScript syntax is the common editor subset. Multiline makes ^ and $
  // match line boundaries, which is what code search expects.
  std::regex_constants::syntax_option_type flags =
      std::regex_constants::ECMAScript | std::regex_constants::optimize |
      std::regex_constants::multiline;
  if (!options.caseSensitive) {
    flags |= std::regex_constants::icase;
  }

  try {
    const std::regex pattern(query.begin(), query.end(), flags);
    for (std::sregex_iterator it(text_.begin(), text_.end(), pattern), end;
         it != end && out->size() < 10000; ++it) {
      const std::smatch& match = *it;
      const size_t found = static_cast<size_t>(match.position(0));
      const size_t matchEnd = found + static_cast<size_t>(match.length(0));
      if (matchEnd <= found) {
        continue;
      }
      bool valid = true;
      if (options.wholeWord) {
        if (found > 0 && isWordByte(static_cast<unsigned char>(text_[found - 1]))) {
          valid = false;
        }
        if (matchEnd < text_.size() && isWordByte(static_cast<unsigned char>(text_[matchEnd]))) {
          valid = false;
        }
      }
      if (valid) {
        out->push_back(
            Range{static_cast<uint32_t>(found), static_cast<uint32_t>(matchEnd)});
      }
    }
  } catch (const std::regex_error&) {
    out->clear();
    return Status::InvalidArgument;
  }
  return Status::Ok;
}

bool Analysis::matchBracket(size_t offset, size_t* outMatch) const {
  if (!outMatch || offset > text_.size()) {
    return false;
  }
  if (offset == text_.size()) {
    if (offset == 0) {
      return false;
    }
    --offset;
  }
  if (!isBracket(text_[offset])) {
    if (offset > 0 && isBracket(text_[offset - 1])) {
      --offset;
    } else {
      return false;
    }
  }
  const char bracket = text_[offset];
  const char partner = isOpeningBracket(bracket)
                           ? bracketFor(bracket)
                           : (bracket == ')' ? '(' : (bracket == ']' ? '[' : '{'));

  const std::vector<Token>& allTokens = tokens();
  std::vector<size_t> brackets;
  brackets.reserve(allTokens.size() / 4);
  for (const Token& token : allTokens) {
    if (token.kind == TokenKind::Punctuation && token.start < text_.size() &&
        isBracket(text_[token.start])) {
      brackets.push_back(token.start);
    }
  }
  const auto found = std::find(brackets.begin(), brackets.end(), offset);
  if (found == brackets.end()) {
    return false;
  }
  const size_t tokenIndex = static_cast<size_t>(std::distance(brackets.begin(), found));

  if (isOpeningBracket(bracket)) {
    int depth = 0;
    for (size_t index = tokenIndex + 1; index < brackets.size(); ++index) {
      const char current = text_[brackets[index]];
      if (current == bracket) {
        ++depth;
      } else if (current == partner) {
        if (depth == 0) {
          *outMatch = brackets[index];
          return true;
        }
        --depth;
      }
    }
  } else {
    int depth = 0;
    for (size_t index = tokenIndex; index-- > 0;) {
      const char current = text_[brackets[index]];
      if (current == bracket) {
        ++depth;
      } else if (current == partner) {
        if (depth == 0) {
          *outMatch = brackets[index];
          return true;
        }
        --depth;
      }
    }
  }
  return false;
}

}  // namespace codecore
