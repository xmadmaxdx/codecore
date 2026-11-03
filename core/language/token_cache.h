#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include "core/language/language.h"

namespace codecore {

/*
 * An immutable token set for one text revision.
 *
 * The flags mirror LexResult's unterminated-literal state. `relexedBytes` and
 * `reusedTokens` expose how much work the incremental pass actually did; tests
 * use them to prove that untouched regions are not lexed again.
 */
struct TokenSet {
  std::vector<Token> tokens;
  bool unterminatedString = false;
  uint32_t unterminatedStringStart = 0;
  bool unterminatedBlockComment = false;
  uint32_t unterminatedBlockStart = 0;

  size_t relexedBytes = 0;
  size_t reusedTokens = 0;
};

/*
 * Incremental tokenizer cache for one document and language.
 *
 * Callers hand it the current text; it keeps the previous text, tokens and a
 * table of safe restart offsets. When the text changed it re-lexes only from
 * the start of the edited line to the first point where the lexical state
 * matches the old state again, then splices the untouched tail back in with
 * shifted offsets. Editing one line in a 100,000-line file therefore lexes one
 * line, not the whole file.
 *
 * Safe restart offsets are line starts that are not inside a multi-line token
 * (a block comment or a triple-quoted/backtick string) and carry the brace
 * depth needed for CSS-like property detection. Markup languages (HTML/XML)
 * have no resumable state machine and always take the full-lex path.
 *
 * Thread safety: one mutex guards the cache. Analysis holds its own immutable
 * TokenSet, so worker threads never touch the cache.
 */
class TokenCache {
 public:
  /* Returns the token set for `text`, reusing or extending cached work. */
  std::shared_ptr<const TokenSet> tokensFor(const std::string& text,
                                            const Language& language);

  /* Drops all cached state; the next call lexes from scratch. */
  void clear();

  /* Bytes lexed by the most recent tokensFor() call. */
  size_t lastRelexedBytes() const { return lastRelexedBytes_; }

 private:
  std::shared_ptr<const TokenSet> fullLex(const std::string& text, const Language& language);
  void rebuildSafeStarts(const std::string& text, const TokenSet& set);

  mutable std::mutex mutex_;
  std::string text_;
  int32_t languageId_ = -1;
  std::shared_ptr<const TokenSet> set_;
  std::vector<size_t> safeStarts_;
  std::vector<int32_t> safeDepths_;
  size_t lastRelexedBytes_ = 0;
};

}  // namespace codecore
