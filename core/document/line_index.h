#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace codecore {

struct LineSpan {
  size_t start = 0;
  size_t contentEnd = 0;
  size_t end = 0;
};

/*
 * Incremental line-start index.
 *
 * Only line starts are stored; content ends are derived from the current text.
 * An edit updates the affected boundaries and shifts later starts instead of
 * rescanning the whole document. This is O(number of lines) per edit and
 * O(log lines) per lookup; it avoids the previous full UTF-8 rescan after every
 * keystroke.
 */
class LineIndex {
 public:
  LineIndex();

  void rebuild(std::string_view text);
  void applyEdit(size_t start, size_t end, std::string_view inserted);

  size_t lineCount() const { return starts_.size(); }
  size_t startOfLine(size_t line) const;
  size_t lineOfOffset(size_t offset) const;
  LineSpan line(size_t index, std::string_view text) const;

 private:
  std::vector<uint32_t> starts_;
};

}  // namespace codecore
