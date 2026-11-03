#pragma once

#include <cstddef>
#include <cstdint>

namespace codecore {

/*
 * Visible-line geometry for a viewport renderer.
 *
 * The renderer asks for the line range intersecting the viewport and draws only
 * that range plus a small overscan. This function is allocation-free and safe to
 * call on every frame.
 */
struct ViewportRange {
  size_t firstLine = 0;
  size_t endLine = 0;  // exclusive
};

ViewportRange visibleLineRange(size_t totalLines, uint32_t scrollY,
                               uint32_t viewportHeight, uint32_t lineHeight,
                               uint32_t overscanLines);

/* Top edge of a line in content coordinates, or 0 when out of range. */
uint64_t lineTop(size_t line, uint32_t lineHeight);

/* Total content height for a line count, saturating rather than overflowing. */
uint64_t contentHeight(size_t totalLines, uint32_t lineHeight);

}  // namespace codecore
