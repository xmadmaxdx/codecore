#include "core/document/line_index.h"

#include <algorithm>

namespace codecore {

LineIndex::LineIndex() { starts_.push_back(0); }

void LineIndex::rebuild(std::string_view text) {
  starts_.clear();
  starts_.push_back(0);
  for (size_t index = 0; index < text.size(); ++index) {
    if (text[index] == '\n') {
      starts_.push_back(static_cast<uint32_t>(index + 1));
    }
  }
}

void LineIndex::applyEdit(size_t start, size_t end, std::string_view inserted) {
  if (start > end) {
    return;
  }
  const int64_t delta =
      static_cast<int64_t>(inserted.size()) - static_cast<int64_t>(end - start);

  std::vector<uint32_t> updated;
  updated.reserve(starts_.size() + inserted.size() / 32 + 1);
  for (const uint32_t lineStart : starts_) {
    if (lineStart <= start) {
      updated.push_back(lineStart);
    } else if (lineStart <= end) {
      // Line boundary removed by the edit.
      continue;
    } else {
      updated.push_back(static_cast<uint32_t>(static_cast<int64_t>(lineStart) + delta));
    }
  }

  for (size_t index = 0; index < inserted.size(); ++index) {
    if (inserted[index] == '\n') {
      updated.push_back(static_cast<uint32_t>(start + index + 1));
    }
  }

  std::sort(updated.begin(), updated.end());
  updated.erase(std::unique(updated.begin(), updated.end()), updated.end());
  if (updated.empty()) {
    updated.push_back(0);
  }
  starts_.swap(updated);
}

size_t LineIndex::startOfLine(size_t line) const {
  if (line >= starts_.size()) {
    return 0;
  }
  return starts_[line];
}

size_t LineIndex::lineOfOffset(size_t offset) const {
  const auto upper = std::upper_bound(starts_.begin(), starts_.end(),
                                     static_cast<uint32_t>(offset));
  if (upper == starts_.begin()) {
    return 0;
  }
  return static_cast<size_t>(std::distance(starts_.begin(), upper) - 1);
}

LineSpan LineIndex::line(size_t index, std::string_view text) const {
  if (index >= starts_.size()) {
    return LineSpan{};
  }
  const size_t start = starts_[index];
  const size_t end = index + 1 < starts_.size() ? starts_[index + 1] : text.size();
  size_t contentEnd = end;
  if (contentEnd > start && text[contentEnd - 1] == '\n') {
    --contentEnd;
    if (contentEnd > start && text[contentEnd - 1] == '\r') {
      --contentEnd;
    }
  }
  return LineSpan{start, contentEnd, end};
}

}  // namespace codecore
