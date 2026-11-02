#include "core/diff/diff.h"

#include <algorithm>
#include <limits>

namespace codecore {
namespace {

constexpr uint64_t kMaxDiffCells = 4'000'000;
constexpr size_t kMaxDiffLines = 20'000;

std::vector<std::string> splitLines(std::string_view text) {
  std::vector<std::string> lines;
  if (text.empty()) {
    return lines;
  }
  size_t start = 0;
  while (start < text.size()) {
    const size_t newline = text.find('\n', start);
    const size_t end = newline == std::string_view::npos ? text.size() : newline;
    size_t contentEnd = end;
    if (contentEnd > start && text[contentEnd - 1] == '\r') {
      --contentEnd;
    }
    lines.emplace_back(text.substr(start, contentEnd - start));
    if (newline == std::string_view::npos) {
      break;
    }
    start = newline + 1;
  }
  if (!text.empty() && text.back() == '\n' && start == text.size()) {
    // A trailing newline terminates the last line; it does not create an
    // additional blank line in the diff view.
  }
  return lines;
}

void appendCoarseDiff(const std::vector<std::string>& original,
                      const std::vector<std::string>& proposed,
                      std::vector<DiffLine>& output) {
  const uint32_t missing = std::numeric_limits<uint32_t>::max();
  for (size_t index = 0; index < original.size(); ++index) {
    output.push_back(
        DiffLine{DiffKind::Delete, static_cast<uint32_t>(index), missing, original[index]});
  }
  for (size_t index = 0; index < proposed.size(); ++index) {
    output.push_back(
        DiffLine{DiffKind::Insert, missing, static_cast<uint32_t>(index), proposed[index]});
  }
}

}  // namespace

std::vector<DiffLine> diffLines(std::string_view original, std::string_view proposed) {
  const std::vector<std::string> oldLines = splitLines(original);
  const std::vector<std::string> newLines = splitLines(proposed);
  std::vector<DiffLine> output;

  const size_t oldCount = oldLines.size();
  const size_t newCount = newLines.size();
  const uint64_t cells = static_cast<uint64_t>(oldCount + 1) * (newCount + 1);
  if (cells > kMaxDiffCells || oldCount + newCount > kMaxDiffLines) {
    appendCoarseDiff(oldLines, newLines, output);
    return output;
  }

  const size_t width = newCount + 1;
  std::vector<uint32_t> table((oldCount + 1) * width, 0);
  for (size_t oldIndex = oldCount; oldIndex-- > 0;) {
    for (size_t newIndex = newCount; newIndex-- > 0;) {
      if (oldLines[oldIndex] == newLines[newIndex]) {
        table[oldIndex * width + newIndex] = table[(oldIndex + 1) * width + newIndex + 1] + 1;
      } else {
        table[oldIndex * width + newIndex] =
            std::max(table[(oldIndex + 1) * width + newIndex],
                     table[oldIndex * width + newIndex + 1]);
      }
    }
  }

  const uint32_t missing = std::numeric_limits<uint32_t>::max();
  size_t oldIndex = 0;
  size_t newIndex = 0;
  while (oldIndex < oldCount && newIndex < newCount) {
    if (oldLines[oldIndex] == newLines[newIndex]) {
      output.push_back(DiffLine{DiffKind::Equal, static_cast<uint32_t>(oldIndex),
                                static_cast<uint32_t>(newIndex), oldLines[oldIndex]});
      ++oldIndex;
      ++newIndex;
    } else if (table[(oldIndex + 1) * width + newIndex] >=
               table[oldIndex * width + newIndex + 1]) {
      output.push_back(DiffLine{DiffKind::Delete, static_cast<uint32_t>(oldIndex), missing,
                                oldLines[oldIndex]});
      ++oldIndex;
    } else {
      output.push_back(DiffLine{DiffKind::Insert, missing, static_cast<uint32_t>(newIndex),
                                newLines[newIndex]});
      ++newIndex;
    }
  }
  while (oldIndex < oldCount) {
    output.push_back(DiffLine{DiffKind::Delete, static_cast<uint32_t>(oldIndex), missing,
                              oldLines[oldIndex]});
    ++oldIndex;
  }
  while (newIndex < newCount) {
    output.push_back(DiffLine{DiffKind::Insert, missing, static_cast<uint32_t>(newIndex),
                              newLines[newIndex]});
    ++newIndex;
  }
  return output;
}

}  // namespace codecore
