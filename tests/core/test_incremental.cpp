#include "core/document/document.h"
#include "core/engine/engine.h"
#include "core/language/language.h"
#include "core/language/token_cache.h"

#include <cstdint>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
#include <vector>

namespace {

int checks = 0;
int failures = 0;

void check(bool condition, const char* expression, int line) {
  ++checks;
  if (!condition) {
    ++failures;
    std::printf("FAIL line %d: %s\n", line, expression);
  }
}

#define CHECK(expression) check((expression), #expression, __LINE__)

bool sameTokens(const std::vector<codecore::Token>& left, const std::vector<codecore::Token>& right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (size_t index = 0; index < left.size(); ++index) {
    if (left[index].start != right[index].start || left[index].end != right[index].end ||
        left[index].kind != right[index].kind) {
      return false;
    }
  }
  return true;
}

size_t countKinds(const std::vector<codecore::Token>& tokens, codecore::TokenKind kind) {
  size_t count = 0;
  for (const codecore::Token& token : tokens) {
    if (token.kind == kind) {
      ++count;
    }
  }
  return count;
}

std::string pythonSample(int lines) {
  std::string text;
  for (int line = 0; line < lines; ++line) {
    text += "def function_" + std::to_string(line) + "(value):\n";
    text += "    total = value + " + std::to_string(line) + "  # comment\n";
    text += "    return f\"{total}\"\n\n";
  }
  return text;
}

void testMatchesFullLex() {
  codecore::LanguageRegistry registry;
  const auto python = registry.sharedByIdString("python");
  CHECK(python != nullptr);

  codecore::TokenCache cache;
  std::string text = "def total(a, b):\n    return a + b\n\nprint(total(1, 2))\n";
  auto tokens = cache.tokensFor(text, *python);
  CHECK(sameTokens(tokens->tokens, codecore::lex(text, *python).tokens));

  // Edit inside an identifier: the tail must be spliced, not re-lexed.
  text.insert(text.find("total(1"), "value, ");
  tokens = cache.tokensFor(text, *python);
  CHECK(sameTokens(tokens->tokens, codecore::lex(text, *python).tokens));
  CHECK(tokens->reusedTokens > 0);

  // Open a triple-quoted string, which changes the lexical state of the tail.
  text.insert(0, "doc = \"\"\"first\nsecond\n");
  tokens = cache.tokensFor(text, *python);
  CHECK(sameTokens(tokens->tokens, codecore::lex(text, *python).tokens));
  CHECK(tokens->unterminatedString);

  // Close it again; the tail is no longer part of a string.
  const size_t close = text.find("second\n");
  text.insert(close + 7, "\"\"\"\n");
  tokens = cache.tokensFor(text, *python);
  CHECK(sameTokens(tokens->tokens, codecore::lex(text, *python).tokens));
  CHECK(!tokens->unterminatedString);
  CHECK(countKinds(tokens->tokens, codecore::TokenKind::String) >= 1);

  // Block comments spanning lines (JavaScript has them).
  const auto javascript = registry.sharedByIdString("javascript");
  codecore::TokenCache jsCache;
  std::string js = "const a = 1;\nconst b = 2;\n";
  jsCache.tokensFor(js, *javascript);
  js.insert(js.size(), "/* open\nstill open\n");
  auto jsTokens = jsCache.tokensFor(js, *javascript);
  CHECK(sameTokens(jsTokens->tokens, codecore::lex(js, *javascript).tokens));
  CHECK(jsTokens->unterminatedBlockComment);
  js.insert(js.size(), "*/\nconsole.log(3);\n");
  jsTokens = jsCache.tokensFor(js, *javascript);
  CHECK(sameTokens(jsTokens->tokens, codecore::lex(js, *javascript).tokens));
  CHECK(!jsTokens->unterminatedBlockComment);

  // Whole-document replacement still converges.
  text = "x = 1\n";
  tokens = cache.tokensFor(text, *python);
  CHECK(sameTokens(tokens->tokens, codecore::lex(text, *python).tokens));
}

void testRelexScope() {
  codecore::LanguageRegistry registry;
  const auto python = registry.sharedByIdString("python");

  codecore::TokenCache cache;
  std::string text = pythonSample(4000);
  auto tokens = cache.tokensFor(text, *python);
  CHECK(tokens->relexedBytes == text.size());

  // Edit line 2000: one line of four must be re-lexed, not 4,000.
  const size_t target = text.find("def function_2000");
  text.insert(target, "x");
  tokens = cache.tokensFor(text, *python);
  CHECK(cache.lastRelexedBytes() < 400);
  CHECK(cache.lastRelexedBytes() > 0);
  CHECK(sameTokens(tokens->tokens, codecore::lex(text, *python).tokens));

  // Delete at the very end: only the last line is affected.
  text.erase(text.size() - 3, 1);
  tokens = cache.tokensFor(text, *python);
  CHECK(cache.lastRelexedBytes() <= 64);
  CHECK(sameTokens(tokens->tokens, codecore::lex(text, *python).tokens));

  // Unchanged text is a pure cache hit.
  tokens = cache.tokensFor(text, *python);
  CHECK(cache.lastRelexedBytes() == 0);
  CHECK(sameTokens(tokens->tokens, codecore::lex(text, *python).tokens));
}

void testRandomEdits() {
  codecore::LanguageRegistry registry;
  const auto python = registry.sharedByIdString("python");

  codecore::TokenCache cache;
  std::string text =
      "class A:\n    def run(self, value):\n        s = \"text\"\n        return value\n";
  cache.tokensFor(text, *python);

  std::mt19937 rng(12345);
  const char* inserts[] = {"x", "(", ")", "\"\"\"", "*/", "/*", "\n", "{", "}", "'", " # c\n",
                           "0x1f", "def ", "  ", "\t"};
  for (int step = 0; step < 400; ++step) {
    const size_t offset = text.empty() ? 0 : static_cast<size_t>(rng() % text.size());
    if ((rng() % 3) == 0 && !text.empty()) {
      const size_t length = 1 + static_cast<size_t>(rng() % 8);
      text.erase(offset, length);
    } else {
      text.insert(offset, inserts[rng() % (sizeof(inserts) / sizeof(inserts[0]))]);
    }
    auto tokens = cache.tokensFor(text, *python);
    bool equal = sameTokens(tokens->tokens, codecore::lex(text, *python).tokens);
    check(equal, "incremental tokens match a full lex", __LINE__);
    if (!equal) {
      std::printf("  diverged at step %d (text size %zu)\n", step, text.size());
      break;
    }
  }
}

void testDocumentUsesCache() {
  codecore::Engine engine;
  codecore::DocumentOptions options;
  options.languageId = engine.findLanguage("javascript");
  auto document = engine.createDocument(options);

  std::string source;
  for (int line = 0; line < 2000; ++line) {
    source += "function f" + std::to_string(line) + "(a) { return a + " +
              std::to_string(line) + "; }\n";
  }
  CHECK(document->setText(source) == codecore::Status::Ok);

  const size_t start = source.find("f1000(");
  CHECK(document->insert(start, "x") == codecore::Status::Ok);
  const std::vector<codecore::Token> tokens = document->tokenize(0, document->length());
  CHECK(tokens.size() > 4000);

  // The edited document must agree with one that was never edited.
  auto fresh = engine.createDocument(options);
  fresh->setText(document->text());
  CHECK(sameTokens(tokens, fresh->tokenize(0, fresh->length())));
}

}  // namespace

int main() {
  testMatchesFullLex();
  testRelexScope();
  testRandomEdits();
  testDocumentUsesCache();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
