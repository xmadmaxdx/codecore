#include "core/language/language.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <string>

namespace codecore {
namespace {

bool isAsciiSpace(unsigned char c) {
  return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

bool isDigit(unsigned char c) { return c >= '0' && c <= '9'; }

bool isAlpha(unsigned char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool isHexDigit(unsigned char c) {
  return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool isIdentifierStart(unsigned char c) { return isAlpha(c) || c == '_' || c == '$'; }

bool isIdentifierPart(unsigned char c) { return isIdentifierStart(c) || isDigit(c); }

char asciiLower(char c) {
  if (c >= 'A' && c <= 'Z') {
    return static_cast<char>(c - 'A' + 'a');
  }
  return c;
}

bool isUpperWord(const std::string& word) {
  bool hasAlpha = false;
  for (char c : word) {
    if (isAlpha(static_cast<unsigned char>(c))) {
      hasAlpha = true;
      if (!(c >= 'A' && c <= 'Z')) {
        return false;
      }
    }
  }
  return hasAlpha;
}

struct StringScan {
  size_t end = 0;
  bool closed = false;
};

StringScan scanQuotedString(const std::string& text, size_t start, char quote, bool triple,
                            bool allowNewline) {
  const size_t size = text.size();
  if (triple) {
    if (start + 2 < size && text[start + 1] == quote && text[start + 2] == quote) {
      const std::string closing(3, quote);
      const size_t found = text.find(closing, start + 3);
      if (found != std::string::npos) {
        return {found + 3, true};
      }
      return {size, false};
    }
  }
  size_t index = start + 1;
  while (index < size) {
    const char current = text[index];
    if (current == '\\') {
      index += 2;
      continue;
    }
    if (current == quote) {
      return {index + 1, true};
    }
    if (current == '\n' && !allowNewline) {
      return {index, false};
    }
    ++index;
  }
  return {size, false};
}

std::string lowerCopy(const std::string& word) {
  std::string lower = word;
  std::transform(lower.begin(), lower.end(), lower.begin(), asciiLower);
  return lower;
}

void emit(LexResult& result, size_t start, size_t end, TokenKind kind) {
  if (end > start) {
    result.tokens.push_back({static_cast<uint32_t>(start), static_cast<uint32_t>(end), kind});
  }
}

size_t scanNumber(const std::string& text, size_t start) {
  const size_t size = text.size();
  size_t index = start;

  if (text[index] == '.') {
    ++index;
    while (index < size && (isDigit(static_cast<unsigned char>(text[index])) || text[index] == '_')) {
      ++index;
    }
    return index;
  }

  if (index + 1 < size && text[index] == '0' &&
      (text[index + 1] == 'x' || text[index + 1] == 'X' || text[index + 1] == 'b' ||
       text[index + 1] == 'B' || text[index + 1] == 'o' || text[index + 1] == 'O')) {
    index += 2;
    while (index < size &&
           (isHexDigit(static_cast<unsigned char>(text[index])) || text[index] == '_')) {
      ++index;
    }
  } else {
    while (index < size && (isDigit(static_cast<unsigned char>(text[index])) || text[index] == '_')) {
      ++index;
    }
    if (index < size && text[index] == '.' && index + 1 < size &&
        isDigit(static_cast<unsigned char>(text[index + 1]))) {
      ++index;
      while (index < size &&
             (isDigit(static_cast<unsigned char>(text[index])) || text[index] == '_')) {
        ++index;
      }
    }
    if (index < size && (text[index] == 'e' || text[index] == 'E' || text[index] == 'p' ||
                         text[index] == 'P')) {
      size_t exponent = index + 1;
      if (exponent < size && (text[exponent] == '+' || text[exponent] == '-')) {
        ++exponent;
      }
      if (exponent < size && isDigit(static_cast<unsigned char>(text[exponent]))) {
        index = exponent;
        while (index < size &&
               (isDigit(static_cast<unsigned char>(text[index])) || text[index] == '_')) {
          ++index;
        }
      }
    }
  }

  // Numeric suffixes such as u32, f64, L, UL.
  while (index < size &&
         (isAlpha(static_cast<unsigned char>(text[index])) || text[index] == '_')) {
    ++index;
  }
  return index;
}

LexResult lexMarkup(const std::string& text, const Language& language) {
  LexResult result;
  const size_t size = text.size();
  const std::string& commentStart = language.blockCommentStart;
  const std::string& commentEnd = language.blockCommentEnd;
  size_t index = 0;

  while (index < size) {
    if (!commentStart.empty() && text.compare(index, commentStart.size(), commentStart) == 0) {
      const size_t found = text.find(commentEnd, index + commentStart.size());
      if (found == std::string::npos) {
        emit(result, index, size, TokenKind::Comment);
        result.unterminatedBlockComment = true;
        result.unterminatedBlockStart = static_cast<uint32_t>(index);
        break;
      }
      emit(result, index, found + commentEnd.size(), TokenKind::Comment);
      index = found + commentEnd.size();
      continue;
    }

    if (text[index] != '<') {
      const size_t next = text.find('<', index);
      index = next == std::string::npos ? size : next;
      continue;
    }

    size_t tag = index + 1;
    if (tag < size && (text[tag] == '/' || text[tag] == '!' || text[tag] == '?')) {
      ++tag;
    }
    const size_t nameStart = tag;
    while (tag < size && !isAsciiSpace(static_cast<unsigned char>(text[tag])) &&
           text[tag] != '>' && text[tag] != '<' && text[tag] != '/') {
      ++tag;
    }
    if (tag == nameStart) {
      emit(result, index, index + 1, TokenKind::Punctuation);
      index += 1;
      continue;
    }
    emit(result, index, tag, TokenKind::Tag);
    size_t cursor = tag;

    while (cursor < size && text[cursor] != '>') {
      const char current = text[cursor];
      if (isAsciiSpace(static_cast<unsigned char>(current))) {
        ++cursor;
        continue;
      }
      if (current == '"' || current == '\'') {
        const StringScan scan = scanQuotedString(text, cursor, current, false, true);
        emit(result, cursor, scan.end, scan.closed ? TokenKind::String : TokenKind::Invalid);
        if (!scan.closed) {
          result.unterminatedString = true;
          result.unterminatedStringStart = static_cast<uint32_t>(cursor);
        }
        cursor = scan.end;
        continue;
      }
      if (isIdentifierStart(static_cast<unsigned char>(current))) {
        size_t end = cursor + 1;
        while (end < size && isIdentifierPart(static_cast<unsigned char>(text[end]))) {
          ++end;
        }
        emit(result, cursor, end, TokenKind::Attribute);
        cursor = end;
        continue;
      }
      if (current == '=') {
        emit(result, cursor, cursor + 1, TokenKind::Operator);
        ++cursor;
        continue;
      }
      ++cursor;
    }

    if (cursor < size) {
      emit(result, cursor, cursor + 1, TokenKind::Punctuation);
      cursor += 1;
    }
    index = cursor;
  }

  return result;
}

}  // namespace

LexResult lex(const std::string& text, const Language& language) {
  if (language.markup) {
    return lexMarkup(text, language);
  }

  LexResult result;
  const size_t size = text.size();
  size_t index = 0;
  int braceDepth = 0;
  bool inBlockComment = false;

  while (index < size) {
    if (inBlockComment) {
      const size_t found = language.blockCommentEnd.empty()
                               ? std::string::npos
                               : text.find(language.blockCommentEnd, index);
      if (found == std::string::npos) {
        emit(result, index, size, TokenKind::Comment);
        result.unterminatedBlockComment = true;
        result.unterminatedBlockStart = static_cast<uint32_t>(index);
        break;
      }
      emit(result, index, found + language.blockCommentEnd.size(), TokenKind::Comment);
      index = found + language.blockCommentEnd.size();
      inBlockComment = false;
      continue;
    }

    if (!language.blockCommentStart.empty() &&
        text.compare(index, language.blockCommentStart.size(), language.blockCommentStart) == 0) {
      const size_t contentStart = index + language.blockCommentStart.size();
      const size_t found = text.find(language.blockCommentEnd, contentStart);
      if (found == std::string::npos) {
        emit(result, index, size, TokenKind::Comment);
        result.unterminatedBlockComment = true;
        result.unterminatedBlockStart = static_cast<uint32_t>(index);
        break;
      }
      emit(result, index, found + language.blockCommentEnd.size(), TokenKind::Comment);
      index = found + language.blockCommentEnd.size();
      continue;
    }

    bool isLineComment = false;
    for (const std::string& marker : language.lineComments) {
      if (!marker.empty() && text.compare(index, marker.size(), marker) == 0) {
        isLineComment = true;
        break;
      }
    }
    if (isLineComment) {
      const size_t lineEnd = text.find('\n', index);
      const size_t commentEnd = lineEnd == std::string::npos ? size : lineEnd;
      emit(result, index, commentEnd, TokenKind::Comment);
      index = commentEnd;
      continue;
    }

    const unsigned char current = static_cast<unsigned char>(text[index]);
    if (isAsciiSpace(current)) {
      ++index;
      continue;
    }

    if (language.quotes.find(static_cast<char>(current)) != std::string::npos) {
      const bool triple = language.tripleQuotes;
      const bool allowNewline = current == '`';
      const StringScan scan = scanQuotedString(text, index, static_cast<char>(current), triple,
                                                allowNewline);
      if (scan.closed) {
        emit(result, index, scan.end, TokenKind::String);
      } else {
        emit(result, index, scan.end, TokenKind::Invalid);
        result.unterminatedString = true;
        result.unterminatedStringStart = static_cast<uint32_t>(index);
      }
      index = scan.end;
      continue;
    }

    if (current == '@' && index + 1 < size &&
        isIdentifierStart(static_cast<unsigned char>(text[index + 1]))) {
      size_t end = index + 2;
      while (end < size && isIdentifierPart(static_cast<unsigned char>(text[end]))) {
        ++end;
      }
      if (language.cssLike) {
        const std::string word = text.substr(index + 1, end - index - 1);
        const std::string lookup = language.caseInsensitive ? lowerCopy(word) : word;
        if (language.keywordSet.find(lookup) != language.keywordSet.end()) {
          emit(result, index, end, TokenKind::Keyword);
          index = end;
          continue;
        }
      }
      emit(result, index, end, TokenKind::Annotation);
      index = end;
      continue;
    }

    if (isDigit(current) ||
        (current == '.' && index + 1 < size &&
         isDigit(static_cast<unsigned char>(text[index + 1])))) {
      const size_t end = scanNumber(text, index);
      emit(result, index, end, TokenKind::Number);
      index = end;
      continue;
    }

    if (isIdentifierStart(current)) {
      size_t end = index + 1;
      while (end < size && isIdentifierPart(static_cast<unsigned char>(text[end]))) {
        ++end;
      }
      const std::string word = text.substr(index, end - index);
      const std::string lookup = language.caseInsensitive ? lowerCopy(word) : word;

      TokenKind kind = TokenKind::Identifier;
      if (language.keywordSet.find(lookup) != language.keywordSet.end()) {
        kind = TokenKind::Keyword;
      } else if (language.typeSet.find(lookup) != language.typeSet.end()) {
        kind = TokenKind::Type;
      } else if (language.builtinSet.find(lookup) != language.builtinSet.end()) {
        kind = TokenKind::Builtin;
      } else if (isUpperWord(word)) {
        kind = TokenKind::Constant;
      }

      if (kind == TokenKind::Identifier) {
        size_t lookahead = end;
        while (lookahead < size && (text[lookahead] == ' ' || text[lookahead] == '\t')) {
          ++lookahead;
        }
        if (lookahead < size && text[lookahead] == '(') {
          kind = TokenKind::Function;
        } else if (language.cssLike && braceDepth > 0) {
          size_t propertyLookahead = end;
          while (propertyLookahead < size &&
                 (text[propertyLookahead] == ' ' || text[propertyLookahead] == '\t')) {
            ++propertyLookahead;
          }
          if (propertyLookahead < size && text[propertyLookahead] == ':') {
            kind = TokenKind::Property;
          }
        }
      }

      emit(result, index, end, kind);
      index = end;
      continue;
    }

    constexpr const char* kOperators = "+-*/%=<>!&|^~?:.";
    if (std::strchr(kOperators, static_cast<char>(current)) != nullptr) {
      emit(result, index, index + 1, TokenKind::Operator);
      ++index;
      continue;
    }

    if (current == '{') {
      ++braceDepth;
      emit(result, index, index + 1, TokenKind::Punctuation);
      ++index;
      continue;
    }
    if (current == '}') {
      braceDepth = std::max(0, braceDepth - 1);
      emit(result, index, index + 1, TokenKind::Punctuation);
      ++index;
      continue;
    }
    if (std::strchr("()[];,", static_cast<char>(current)) != nullptr) {
      emit(result, index, index + 1, TokenKind::Punctuation);
      ++index;
      continue;
    }

    ++index;
  }

  return result;
}

}  // namespace codecore
