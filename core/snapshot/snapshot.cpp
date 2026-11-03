#include "core/snapshot/snapshot.h"

#include <utility>

namespace codecore {

Snapshot::Snapshot(std::string text, std::shared_ptr<const Language> language,
                   uint64_t version)
    : text_(std::move(text)), language_(std::move(language)), version_(version) {}

int32_t Snapshot::languageId() const { return language_ ? language_->id : -1; }

std::shared_ptr<const Analysis> Snapshot::analysis() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!analysis_) {
    analysis_ = std::make_shared<Analysis>(text_, language_);
  }
  return analysis_;
}

std::vector<Token> Snapshot::tokenize(size_t start, size_t end) const {
  return analysis()->tokenize(start, end);
}

std::vector<CompletionItem> Snapshot::complete(size_t offset) const {
  return analysis()->complete(offset);
}

std::vector<Diagnostic> Snapshot::diagnose() const {
  return analysis()->diagnose();
}

std::vector<FoldingRange> Snapshot::foldingRanges() const {
  return analysis()->foldingRanges();
}

Status Snapshot::find(std::string_view query, const SearchOptions& options,
                      std::vector<Range>* out) const {
  return analysis()->find(query, options, out);
}

}  // namespace codecore
