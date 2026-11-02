#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace codecore {

/*
 * Piece-table text buffer.
 *
 * The document's original text is immutable. Every insertion is appended to an
 * "add" buffer and referenced by a piece, so edits never copy the whole
 * document. Adjacent contiguous pieces are merged after every edit.
 *
 * The add buffer is compacted once it grows beyond a threshold relative to the
 * document, which bounds wasted memory for long editing sessions. The buffer is
 * not a rope: materializing text is O(n), which is documented in status.md.
 */
class TextBuffer {
 public:
  TextBuffer();

  void reset(std::string_view text);
  size_t size() const { return size_; }

  std::string toString() const;
  std::string slice(size_t offset, size_t length) const;

  void insert(size_t offset, std::string_view text);
  void erase(size_t offset, size_t length);

  size_t pieceCount() const { return pieces_.size(); }

 private:
  struct Piece {
    uint32_t start;
    uint32_t length;
    bool fromAdd;
  };

  const char* pieceData(const Piece& piece) const;
  void compactPieces();
  void maybeCompactAddBuffer();

  std::string original_;
  std::string added_;
  std::vector<Piece> pieces_;
  size_t size_ = 0;
};

}  // namespace codecore
