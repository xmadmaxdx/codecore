#include "core/text_buffer/text_buffer.h"

#include <algorithm>
#include <cstring>

namespace codecore {

TextBuffer::TextBuffer() = default;

void TextBuffer::reset(std::string_view text) {
  original_.assign(text.data(), text.size());
  added_.clear();
  pieces_.clear();
  size_ = text.size();
  if (!original_.empty()) {
    pieces_.push_back({0, static_cast<uint32_t>(original_.size()), false});
  }
}

const char* TextBuffer::pieceData(const Piece& piece) const {
  const std::string& source = piece.fromAdd ? added_ : original_;
  return source.data() + piece.start;
}

std::string TextBuffer::toString() const {
  std::string result;
  result.reserve(size_);
  for (const Piece& piece : pieces_) {
    result.append(pieceData(piece), piece.length);
  }
  return result;
}

std::string TextBuffer::slice(size_t offset, size_t length) const {
  if (offset >= size_ || length == 0) {
    return {};
  }
  const size_t end = std::min(size_, offset + length);
  std::string result;
  result.reserve(end - offset);

  size_t cursor = 0;
  for (const Piece& piece : pieces_) {
    const size_t pieceEnd = cursor + piece.length;
    if (pieceEnd <= offset) {
      cursor = pieceEnd;
      continue;
    }
    if (cursor >= end) {
      break;
    }
    const size_t copyStart = std::max(cursor, offset);
    const size_t copyEnd = std::min(pieceEnd, end);
    if (copyEnd > copyStart) {
      result.append(pieceData(piece) + (copyStart - cursor), copyEnd - copyStart);
    }
    cursor = pieceEnd;
    if (cursor >= end) {
      break;
    }
  }
  return result;
}

void TextBuffer::insert(size_t offset, std::string_view text) {
  if (text.empty()) {
    return;
  }
  const uint32_t addStart = static_cast<uint32_t>(added_.size());
  added_.append(text.data(), text.size());

  if (pieces_.empty()) {
    pieces_.push_back({addStart, static_cast<uint32_t>(text.size()), true});
    size_ += text.size();
    maybeCompactAddBuffer();
    return;
  }

  size_t remaining = offset;
  size_t index = 0;
  while (index < pieces_.size() && remaining >= pieces_[index].length) {
    remaining -= pieces_[index].length;
    ++index;
  }

  size_t insertIndex = index;
  if (index < pieces_.size() && remaining > 0 && remaining < pieces_[index].length) {
    const Piece& piece = pieces_[index];
    Piece right{piece.start + static_cast<uint32_t>(remaining),
                piece.length - static_cast<uint32_t>(remaining), piece.fromAdd};
    pieces_[index].length = static_cast<uint32_t>(remaining);
    pieces_.insert(pieces_.begin() + static_cast<std::ptrdiff_t>(index) + 1, right);
    insertIndex = index + 1;
  } else if (index >= pieces_.size()) {
    insertIndex = pieces_.size();
  }

  pieces_.insert(pieces_.begin() + static_cast<std::ptrdiff_t>(insertIndex),
                 Piece{addStart, static_cast<uint32_t>(text.size()), true});
  size_ += text.size();
  compactPieces();
  maybeCompactAddBuffer();
}

void TextBuffer::erase(size_t offset, size_t length) {
  if (offset >= size_ || length == 0) {
    return;
  }
  const size_t end = std::min(size_, offset + length);

  // Split the piece containing the start offset.
  size_t cursor = 0;
  for (size_t index = 0; index < pieces_.size(); ++index) {
    const Piece& piece = pieces_[index];
    const size_t pieceEnd = cursor + piece.length;
    if (offset > cursor && offset < pieceEnd) {
      const uint32_t leftLength = static_cast<uint32_t>(offset - cursor);
      Piece right{piece.start + leftLength, piece.length - leftLength, piece.fromAdd};
      pieces_[index].length = leftLength;
      pieces_.insert(pieces_.begin() + static_cast<std::ptrdiff_t>(index) + 1, right);
      break;
    }
    if (offset <= pieceEnd) {
      break;
    }
    cursor = pieceEnd;
  }

  // Split the piece containing the end offset.
  std::vector<Piece> split;
  split.reserve(pieces_.size() + 1);
  size_t position = 0;
  for (const Piece& piece : pieces_) {
    const size_t pieceEnd = position + piece.length;
    if (position < end && end < pieceEnd) {
      const uint32_t leftLength = static_cast<uint32_t>(end - position);
      split.push_back({piece.start, leftLength, piece.fromAdd});
      split.push_back({piece.start + leftLength, piece.length - leftLength, piece.fromAdd});
    } else {
      split.push_back(piece);
    }
    position = pieceEnd;
  }
  pieces_.swap(split);

  // Drop pieces inside the erased range.
  std::vector<Piece> kept;
  kept.reserve(pieces_.size());
  position = 0;
  for (const Piece& piece : pieces_) {
    const size_t pieceEnd = position + piece.length;
    if (pieceEnd <= offset || position >= end) {
      kept.push_back(piece);
    }
    position = pieceEnd;
  }
  pieces_.swap(kept);
  size_ -= (end - offset);
  compactPieces();
}

void TextBuffer::compactPieces() {
  std::vector<Piece> merged;
  merged.reserve(pieces_.size());
  for (const Piece& piece : pieces_) {
    if (piece.length == 0) {
      continue;
    }
    if (!merged.empty()) {
      Piece& previous = merged.back();
      if (previous.fromAdd == piece.fromAdd &&
          previous.start + previous.length == piece.start) {
        previous.length += piece.length;
        continue;
      }
    }
    merged.push_back(piece);
  }
  pieces_.swap(merged);
}

void TextBuffer::maybeCompactAddBuffer() {
  constexpr size_t kAddBufferSoftLimit = 4u * 1024u * 1024u;
  if (added_.size() < kAddBufferSoftLimit || added_.size() <= original_.size()) {
    return;
  }
  const std::string merged = toString();
  original_ = merged;
  added_.clear();
  pieces_.clear();
  if (!original_.empty()) {
    pieces_.push_back({0, static_cast<uint32_t>(original_.size()), false});
  }
}

}  // namespace codecore
