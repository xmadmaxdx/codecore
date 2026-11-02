#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_set>
#include <vector>

namespace codecore {

enum class TokenKind : uint32_t {
  Plain = 0,
  Comment = 1,
  Keyword = 2,
  Type = 3,
  Builtin = 4,
  String = 5,
  Number = 6,
  Operator = 7,
  Punctuation = 8,
  Identifier = 9,
  Function = 10,
  Annotation = 11,
  Constant = 12,
  Tag = 13,
  Attribute = 14,
  Property = 15,
  Invalid = 16,
};

struct Token {
  uint32_t start = 0;
  uint32_t end = 0;
  TokenKind kind = TokenKind::Plain;
};

struct LexResult {
  std::vector<Token> tokens;
  bool unterminatedBlockComment = false;
  bool unterminatedString = false;
  uint32_t unterminatedBlockStart = 0;
  uint32_t unterminatedStringStart = 0;
};

struct Language {
  int32_t id = 0;
  std::string idString;
  std::string displayName;
  std::string extensions;
  std::vector<std::string> keywords;
  std::vector<std::string> types;
  std::vector<std::string> builtins;
  std::unordered_set<std::string> keywordSet;
  std::unordered_set<std::string> typeSet;
  std::unordered_set<std::string> builtinSet;
  std::vector<std::string> lineComments;
  std::string blockCommentStart;
  std::string blockCommentEnd;
  std::string quotes;
  bool tripleQuotes = false;
  bool caseInsensitive = false;
  bool markup = false;
  bool cssLike = false;
  uint32_t capabilities = 0;

  void finalize();
};

/* Tokenizes a complete document. Offsets are byte offsets into `text`. */
LexResult lex(const std::string& text, const Language& language);

class LanguageRegistry {
 public:
  LanguageRegistry();

  size_t count() const { return languages_.size(); }
  const Language* at(size_t index) const;
  std::shared_ptr<const Language> sharedById(int32_t id) const;
  std::shared_ptr<const Language> sharedByIdString(const std::string& id) const;
  int32_t findId(const std::string& id) const;

 private:
  std::vector<std::shared_ptr<Language>> languages_;
};

}  // namespace codecore
