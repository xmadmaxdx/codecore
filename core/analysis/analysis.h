#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "core/document/line_index.h"
#include "core/document/types.h"
#include "core/language/language.h"

namespace codecore {

/*
 * Immutable text analysis for one document version.
 *
 * Analysis owns a text copy and lazily caches lexical tokens, diagnostics and
 * folding ranges. It is safe to query from multiple threads; the cached data is
 * published once and only read afterwards. Documents create one Analysis per
 * version, and snapshots create one per snapshot.
 */
class Analysis {
 public:
  Analysis(std::string text, std::shared_ptr<const Language> language);

  const std::string& text() const { return text_; }
  int32_t languageId() const;
  const Language& language() const { return *language_; }

  std::vector<Token> tokenize(size_t start, size_t end) const;
  std::vector<CompletionItem> complete(size_t offset) const;
  std::vector<Diagnostic> diagnose() const;
  std::vector<FoldingRange> foldingRanges() const;
  Status find(std::string_view query, const SearchOptions& options,
              std::vector<Range>* out) const;
  bool matchBracket(size_t offset, size_t* outMatch) const;

 private:
  void ensureLexed() const;
  const std::vector<Token>& tokens() const;

  std::string text_;
  std::shared_ptr<const Language> language_;
  LineIndex lineIndex_;

  mutable std::mutex mutex_;
  mutable bool lexed_ = false;
  mutable LexResult lex_;
  mutable bool diagnosticsReady_ = false;
  mutable std::vector<Diagnostic> diagnostics_;
  mutable bool foldingReady_ = false;
  mutable std::vector<FoldingRange> folding_;
};

}  // namespace codecore
