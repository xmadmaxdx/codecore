#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace codecore {

enum class DiffKind : uint32_t {
  Equal = 0,
  Insert = 1,
  Delete = 2,
};

struct DiffLine {
  DiffKind kind = DiffKind::Equal;
  uint32_t oldLine = 0;
  uint32_t newLine = 0;
  std::string text;
};

/*
 * Line-oriented diff for proposed changes.
 *
 * Lines are compared without their terminators. The implementation uses a
 * bounded LCS table; for documents that exceed the bound it falls back to a
 * delete-all/insert-all diff, which is correct but intentionally coarse.
 */
std::vector<DiffLine> diffLines(std::string_view original, std::string_view proposed);

}  // namespace codecore
