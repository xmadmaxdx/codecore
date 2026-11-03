#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/language/language.h"

namespace codecore {

struct SyntaxPoint {
  uint32_t row = 0;
  /* Byte column inside the row, matching Tree-sitter's convention. */
  uint32_t column = 0;
};

struct SyntaxHighlight {
  size_t start = 0;
  size_t end = 0;
  TokenKind kind = TokenKind::Plain;
  std::string capture;
};

struct SyntaxError {
  size_t start = 0;
  size_t end = 0;
  SyntaxPoint startPoint;
  SyntaxPoint endPoint;
  bool missing = false;
  std::string nodeType;
};

/*
 * Structural syntax layer backed by Tree-sitter.
 *
 * CodeCore's lexer remains the fast, grammar-free fallback. An SyntaxTree adds
 * a concrete syntax tree, structural highlights and real syntax errors. Trees
 * are immutable once published; incremental reparse copies the previous tree,
 * applies one edit and reuses the unchanged subtrees.
 *
 * The layer never owns a document. It stores the exact text it was parsed from
 * so the next reparse can derive a byte edit without the caller describing it.
 */
class SyntaxTree {
 public:
  ~SyntaxTree();

  SyntaxTree(const SyntaxTree&) = delete;
  SyntaxTree& operator=(const SyntaxTree&) = delete;

  /* Returns nullptr when the language has no bundled grammar or parsing fails. */
  static std::shared_ptr<SyntaxTree> parse(std::string_view text,
                                           std::string_view languageId);

  /*
   * Parses `text` starting from `previous`. The changed byte range is derived by
   * comparing the previous text with the new text. Returns a fresh tree; the
   * previous tree is not modified.
   */
  static std::shared_ptr<SyntaxTree> reparse(const SyntaxTree& previous,
                                             std::string_view text);

  /* Grammar id, e.g. "python". Empty for an invalid tree. */
  const std::string& languageId() const;
  const std::string& text() const;
  std::string rootType() const;
  size_t nodeCount() const;
  bool hasErrors() const;

  /*
   * Structural highlights in source order. Overlapping captures are resolved in
   * favour of the more specific capture (function before variable, etc.).
   */
  std::vector<SyntaxHighlight> highlights() const;

  /* Error and missing nodes only, in source order. */
  std::vector<SyntaxError> errors() const;

  /* Smallest named node covering `offset`; false when no node matches. */
  bool nodeAt(size_t offset, std::string* outType, size_t* outStart,
              size_t* outEnd) const;

 private:
  SyntaxTree();

  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/* True when a bundled grammar exists for `languageId`. */
bool syntaxLanguageSupported(std::string_view languageId);

}  // namespace codecore
