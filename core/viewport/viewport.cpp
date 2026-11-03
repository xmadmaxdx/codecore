#include "core/viewport/viewport.h"

#include <algorithm>
#include <limits>

namespace codecore {

ViewportRange visibleLineRange(size_t totalLines, uint32_t scrollY,
                               uint32_t viewportHeight, uint32_t lineHeight,
                               uint32_t overscanLines) {
  ViewportRange range;
  if (totalLines == 0 || lineHeight == 0 || viewportHeight == 0) {
    return range;
  }

  const uint64_t height = viewportHeight;
  const uint64_t line = lineHeight;
  const uint64_t firstVisible = static_cast<uint64_t>(scrollY) / line;
  const uint64_t lastVisible = (static_cast<uint64_t>(scrollY) + height - 1) / line;

  const uint64_t first = firstVisible > overscanLines ? firstVisible - overscanLines : 0;
  const uint64_t end = std::min<uint64_t>(
      totalLines,
      lastVisible + static_cast<uint64_t>(overscanLines) + 1);

  range.firstLine = static_cast<size_t>(std::min<uint64_t>(first, totalLines));
  range.endLine = static_cast<size_t>(std::max<uint64_t>(range.firstLine, end));
  return range;
}

uint64_t lineTop(size_t line, uint32_t lineHeight) {
  if (lineHeight == 0) {
    return 0;
  }
  constexpr uint64_t kMax = std::numeric_limits<uint64_t>::max();
  if (line > kMax / lineHeight) {
    return kMax;
  }
  return static_cast<uint64_t>(line) * lineHeight;
}

uint64_t contentHeight(size_t totalLines, uint32_t lineHeight) {
  constexpr uint64_t kMax = std::numeric_limits<uint64_t>::max();
  if (lineHeight != 0 && totalLines > kMax / lineHeight) {
    return kMax;
  }
  return static_cast<uint64_t>(totalLines) * lineHeight;
}

}  // namespace codecore
