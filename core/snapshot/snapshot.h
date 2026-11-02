#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "core/analysis/analysis.h"
#include "core/document/types.h"
#include "core/language/language.h"

namespace codecore {

/*
 * Immutable snapshot of a document at a specific version.
 *
 * A snapshot owns its text and analysis caches, so hosts can move language
 * services to a worker thread or isolate without locking the document. The
 * snapshot's Analysis is created lazily and shared by all its queries.
 */
class Snapshot {
 public:
  Snapshot(std::string text, std::shared_ptr<const Language> language, uint64_t version);

  uint64_t version() const { return version_; }
  size_t length() const { return text_.size(); }
  const std::string& text() const { return text_; }
  std::shared_ptr<const Language> language() const { return language_; }
  int32_t languageId() const;

  std::shared_ptr<const Analysis> analysis() const;

  std::vector<Token> tokenize(size_t start, size_t end) const;
  std::vector<CompletionItem> complete(size_t offset) const;
  std::vector<Diagnostic> diagnose() const;
  std::vector<FoldingRange> foldingRanges() const;
  std::vector<Range> find(std::string_view query, const SearchOptions& options) const;

 private:
  std::string text_;
  std::shared_ptr<const Language> language_;
  uint64_t version_ = 0;
  mutable std::mutex mutex_;
  mutable std::shared_ptr<const Analysis> analysis_;
};

}  // namespace codecore
