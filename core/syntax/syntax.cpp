#include "core/syntax/syntax.h"

#include <algorithm>
#include <cstring>
#include <map>
#include <utility>

#include <tree_sitter/api.h>

#include "core/syntax/javascript_highlights.h"
#include "core/syntax/python_highlights.h"

extern "C" {
const TSLanguage* tree_sitter_javascript(void);
const TSLanguage* tree_sitter_python(void);
}

namespace codecore {
namespace {

/*
 * Bundled grammar table. Adding a language means vendoring its generated
 * parser, adding one row here and one highlight header. Everything above this
 * layer (document, C ABI, Dart, JNI) is language-agnostic.
 */
struct GrammarEntry {
  const char* id;
  /* Space-separated accepted ids. The canonical id above is always accepted. */
  const char* aliases;
  const TSLanguage* (*language)();
  const char* query;
};

constexpr GrammarEntry kGrammars[] = {
    {"python", "py python3", tree_sitter_python, syntax_queries::kPythonHighlights},
    {"javascript", "js jsx mjs cjs", tree_sitter_javascript,
     syntax_queries::kJavaScriptHighlights},
};

const GrammarEntry* findGrammar(std::string_view id) {
  for (const GrammarEntry& grammar : kGrammars) {
    if (id == grammar.id) {
      return &grammar;
    }
    const std::string_view aliases(grammar.aliases);
    size_t offset = 0;
    while (offset < aliases.size()) {
      const size_t space = aliases.find(' ', offset);
      const size_t end = space == std::string_view::npos ? aliases.size() : space;
      if (id == aliases.substr(offset, end - offset)) {
        return &grammar;
      }
      if (space == std::string_view::npos) {
        break;
      }
      offset = space + 1;
    }
  }
  return nullptr;
}

TSPoint toPoint(const SyntaxPoint& point) {
  TSPoint result;
  result.row = point.row;
  result.column = point.column;
  return result;
}

/*
 * Byte column inside the row. Tree-sitter measures columns in bytes, while
 * CodeCore positions are code points, so this must not reuse Position.
 */
SyntaxPoint pointAt(std::string_view text, size_t offset) {
  offset = std::min(offset, text.size());
  SyntaxPoint point;
  size_t lineStart = 0;
  for (size_t index = 0; index < offset; ++index) {
    if (text[index] == '\n') {
      ++point.row;
      lineStart = index + 1;
    }
  }
  point.column = static_cast<uint32_t>(offset - lineStart);
  return point;
}

bool isContinuation(char c) { return (static_cast<unsigned char>(c) & 0xC0u) == 0x80u; }

/* Common prefix/suffix byte edit between two versions of the same document. */
void computeEdit(std::string_view before, std::string_view after, size_t* outStart,
                 size_t* outOldEnd, size_t* outNewEnd) {
  const size_t limit = std::min(before.size(), after.size());
  size_t prefix = 0;
  while (prefix < limit && before[prefix] == after[prefix]) {
    ++prefix;
  }
  while (prefix > 0 && ((prefix < before.size() && isContinuation(before[prefix])) ||
                        (prefix < after.size() && isContinuation(after[prefix])))) {
    --prefix;
  }

  size_t suffix = 0;
  const size_t remaining = limit - prefix;
  while (suffix < remaining &&
         before[before.size() - 1 - suffix] == after[after.size() - 1 - suffix]) {
    ++suffix;
  }
  while (suffix > 0 &&
         (isContinuation(before[before.size() - suffix]) ||
          isContinuation(after[after.size() - suffix]))) {
    --suffix;
  }

  *outStart = prefix;
  *outOldEnd = before.size() - suffix;
  *outNewEnd = after.size() - suffix;
}

TokenKind kindForCapture(std::string_view capture) {
  if (capture == "comment") return TokenKind::Comment;
  if (capture == "keyword") return TokenKind::Keyword;
  if (capture == "string" || capture == "embedded") return TokenKind::String;
  if (capture == "number") return TokenKind::Number;
  if (capture == "function" || capture == "function.call" || capture == "function.method") {
    return TokenKind::Function;
  }
  if (capture == "type" || capture == "constructor") return TokenKind::Type;
  if (capture == "constant" || capture == "constant.builtin") return TokenKind::Constant;
  if (capture == "property") return TokenKind::Property;
  if (capture == "operator") return TokenKind::Operator;
  if (capture.compare(0, 11, "punctuation") == 0) return TokenKind::Punctuation;
  if (capture == "variable" || capture == "variable.parameter") return TokenKind::Identifier;
  return TokenKind::Plain;
}

int priorityForCapture(std::string_view capture) {
  if (capture == "keyword") return 100;
  if (capture == "comment") return 95;
  if (capture == "string" || capture == "embedded") return 90;
  if (capture == "number" || capture == "constant.builtin") return 85;
  if (capture == "function" || capture == "function.method" || capture == "function.call") {
    return 80;
  }
  if (capture == "type" || capture == "constructor") return 70;
  if (capture == "property") return 60;
  if (capture == "variable.parameter") return 50;
  if (capture == "variable") return 40;
  if (capture == "operator") return 30;
  return 20;
}

struct TsTreeDeleter {
  void operator()(TSTree* tree) const {
    if (tree) {
      ts_tree_delete(tree);
    }
  }
};

struct TsQueryDeleter {
  void operator()(TSQuery* query) const {
    if (query) {
      ts_query_delete(query);
    }
  }
};

struct TsCursorDeleter {
  void operator()(TSQueryCursor* cursor) const {
    if (cursor) {
      ts_query_cursor_delete(cursor);
    }
  }
};

}  // namespace

struct SyntaxTree::Impl {
  std::unique_ptr<TSTree, TsTreeDeleter> tree;
  std::string text;
  std::string languageId;
  const TSLanguage* language = nullptr;
};

SyntaxTree::SyntaxTree() : impl_(std::make_unique<Impl>()) {}

SyntaxTree::~SyntaxTree() = default;

bool syntaxLanguageSupported(std::string_view languageId) {
  return findGrammar(languageId) != nullptr;
}

std::shared_ptr<SyntaxTree> SyntaxTree::parse(std::string_view text,
                                              std::string_view languageId) {
  const GrammarEntry* grammar = findGrammar(languageId);
  if (!grammar) {
    return nullptr;
  }
  const TSLanguage* language = grammar->language();
  if (!language) {
    return nullptr;
  }

  TSParser* parser = ts_parser_new();
  if (!parser) {
    return nullptr;
  }
  if (!ts_parser_set_language(parser, language)) {
    ts_parser_delete(parser);
    return nullptr;
  }

  TSTree* tree = ts_parser_parse_string(parser, nullptr, text.data(),
                                        static_cast<uint32_t>(text.size()));
  ts_parser_delete(parser);
  if (!tree) {
    return nullptr;
  }

  auto result = std::shared_ptr<SyntaxTree>(new SyntaxTree());
  result->impl_->tree.reset(tree);
  result->impl_->text.assign(text.data(), text.size());
  /* Aliases resolve to the canonical grammar id; "js" becomes "javascript". */
  result->impl_->languageId = grammar->id;
  result->impl_->language = language;
  return result;
}

std::shared_ptr<SyntaxTree> SyntaxTree::reparse(const SyntaxTree& previous,
                                                std::string_view text) {
  if (!previous.impl_ || !previous.impl_->tree || !previous.impl_->language) {
    return parse(text, previous.languageId());
  }
  if (previous.impl_->text == text) {
    TSTree* copy = ts_tree_copy(previous.impl_->tree.get());
    if (!copy) {
      return parse(text, previous.languageId());
    }
    auto result = std::shared_ptr<SyntaxTree>(new SyntaxTree());
    result->impl_->tree.reset(copy);
    result->impl_->text = previous.impl_->text;
    result->impl_->languageId = previous.impl_->languageId;
    result->impl_->language = previous.impl_->language;
    return result;
  }

  const std::string& before = previous.impl_->text;
  size_t start = 0;
  size_t oldEnd = 0;
  size_t newEnd = 0;
  computeEdit(before, text, &start, &oldEnd, &newEnd);

  TSTree* edited = ts_tree_copy(previous.impl_->tree.get());
  if (!edited) {
    return parse(text, previous.languageId());
  }

  TSInputEdit edit;
  edit.start_byte = static_cast<uint32_t>(start);
  edit.old_end_byte = static_cast<uint32_t>(oldEnd);
  edit.new_end_byte = static_cast<uint32_t>(newEnd);
  edit.start_point = toPoint(pointAt(before, start));
  edit.old_end_point = toPoint(pointAt(before, oldEnd));
  edit.new_end_point = toPoint(pointAt(text, newEnd));
  ts_tree_edit(edited, &edit);

  TSParser* parser = ts_parser_new();
  if (!parser) {
    ts_tree_delete(edited);
    return parse(text, previous.languageId());
  }
  if (!ts_parser_set_language(parser, previous.impl_->language)) {
    ts_parser_delete(parser);
    ts_tree_delete(edited);
    return parse(text, previous.languageId());
  }

  TSTree* tree = ts_parser_parse_string(parser, edited, text.data(),
                                        static_cast<uint32_t>(text.size()));
  ts_parser_delete(parser);
  ts_tree_delete(edited);
  if (!tree) {
    return parse(text, previous.languageId());
  }

  auto result = std::shared_ptr<SyntaxTree>(new SyntaxTree());
  result->impl_->tree.reset(tree);
  result->impl_->text.assign(text.data(), text.size());
  result->impl_->languageId = previous.impl_->languageId;
  result->impl_->language = previous.impl_->language;
  return result;
}

const std::string& SyntaxTree::languageId() const { return impl_->languageId; }

const std::string& SyntaxTree::text() const { return impl_->text; }

std::string SyntaxTree::rootType() const {
  if (!impl_->tree) {
    return {};
  }
  const char* type = ts_node_type(ts_tree_root_node(impl_->tree.get()));
  return type ? std::string(type) : std::string();
}

size_t SyntaxTree::nodeCount() const {
  if (!impl_->tree) {
    return 0;
  }
  size_t count = 0;
  std::vector<TSNode> stack;
  stack.push_back(ts_tree_root_node(impl_->tree.get()));
  while (!stack.empty()) {
    const TSNode node = stack.back();
    stack.pop_back();
    ++count;
    const uint32_t children = ts_node_child_count(node);
    for (uint32_t index = 0; index < children; ++index) {
      stack.push_back(ts_node_child(node, index));
    }
    /* Defensive cap: a malformed tree must not exhaust the stack or memory. */
    if (count > 5'000'000) {
      break;
    }
  }
  return count;
}

bool SyntaxTree::hasErrors() const {
  if (!impl_->tree) {
    return false;
  }
  std::vector<TSNode> stack;
  stack.push_back(ts_tree_root_node(impl_->tree.get()));
  while (!stack.empty()) {
    const TSNode node = stack.back();
    stack.pop_back();
    if (ts_node_is_error(node) || ts_node_is_missing(node)) {
      return true;
    }
    const uint32_t children = ts_node_child_count(node);
    for (uint32_t index = 0; index < children; ++index) {
      stack.push_back(ts_node_child(node, index));
    }
  }
  return false;
}

std::vector<SyntaxHighlight> SyntaxTree::highlights() const {
  std::vector<SyntaxHighlight> result;
  if (!impl_->tree || !impl_->language) {
    return result;
  }
  const GrammarEntry* grammar = findGrammar(impl_->languageId);
  if (!grammar || !grammar->query) {
    return result;
  }
  const char* source = grammar->query;

  uint32_t errorOffset = 0;
  TSQueryError errorType = TSQueryErrorNone;
  std::unique_ptr<TSQuery, TsQueryDeleter> query(
      ts_query_new(impl_->language, source, static_cast<uint32_t>(std::strlen(source)),
                   &errorOffset, &errorType));
  if (!query) {
    return result;
  }

  std::unique_ptr<TSQueryCursor, TsCursorDeleter> cursor(ts_query_cursor_new());
  if (!cursor) {
    return result;
  }
  ts_query_cursor_exec(cursor.get(), query.get(), ts_tree_root_node(impl_->tree.get()));

  TSQueryMatch match;
  while (ts_query_cursor_next_match(cursor.get(), &match)) {
    for (uint16_t index = 0; index < match.capture_count; ++index) {
      const TSQueryCapture& capture = match.captures[index];
      uint32_t length = 0;
      const char* name =
          ts_query_capture_name_for_id(query.get(), capture.index, &length);
      if (!name || length == 0) {
        continue;
      }
      const std::string_view captureName(name, length);
      const uint32_t start = ts_node_start_byte(capture.node);
      const uint32_t end = ts_node_end_byte(capture.node);
      if (end <= start) {
        continue;
      }
      SyntaxHighlight highlight;
      highlight.start = start;
      highlight.end = end;
      highlight.capture.assign(captureName);
      highlight.kind = kindForCapture(captureName);
      result.push_back(std::move(highlight));
    }
  }

  std::sort(result.begin(), result.end(),
            [](const SyntaxHighlight& left, const SyntaxHighlight& right) {
              if (left.start != right.start) {
                return left.start < right.start;
              }
              if (left.end != right.end) {
                return left.end < right.end;
              }
              return priorityForCapture(left.capture) > priorityForCapture(right.capture);
            });

  /* One span per byte range, most specific capture wins. */
  std::vector<SyntaxHighlight> deduped;
  deduped.reserve(result.size());
  for (SyntaxHighlight& highlight : result) {
    if (!deduped.empty() && deduped.back().start == highlight.start &&
        deduped.back().end == highlight.end) {
      continue;
    }
    deduped.push_back(std::move(highlight));
  }
  return deduped;
}

std::vector<SyntaxError> SyntaxTree::errors() const {
  std::vector<SyntaxError> result;
  if (!impl_->tree) {
    return result;
  }

  std::vector<TSNode> stack;
  stack.push_back(ts_tree_root_node(impl_->tree.get()));
  while (!stack.empty()) {
    const TSNode node = stack.back();
    stack.pop_back();

    const bool isError = ts_node_is_error(node);
    const bool isMissing = ts_node_is_missing(node);
    if (isError || isMissing) {
      const TSPoint start = ts_node_start_point(node);
      const TSPoint end = ts_node_end_point(node);
      SyntaxError error;
      error.start = ts_node_start_byte(node);
      error.end = ts_node_end_byte(node);
      error.startPoint = SyntaxPoint{start.row, start.column};
      error.endPoint = SyntaxPoint{end.row, end.column};
      error.missing = isMissing;
      const char* type = ts_node_type(node);
      error.nodeType = type ? type : "";
      result.push_back(std::move(error));
      if (result.size() >= 500) {
        break;
      }
      if (isError && !isMissing) {
        /* ERROR nodes contain arbitrary children; the parent range is enough. */
        continue;
      }
    }

    const uint32_t children = ts_node_child_count(node);
    for (uint32_t index = 0; index < children; ++index) {
      stack.push_back(ts_node_child(node, index));
    }
  }

  std::sort(result.begin(), result.end(), [](const SyntaxError& left, const SyntaxError& right) {
    return left.start < right.start;
  });
  return result;
}

bool SyntaxTree::nodeAt(size_t offset, std::string* outType, size_t* outStart,
                        size_t* outEnd) const {
  if (!impl_->tree || offset > impl_->text.size()) {
    return false;
  }
  const uint32_t byte = static_cast<uint32_t>(offset);
  TSNode node =
      ts_node_descendant_for_byte_range(ts_tree_root_node(impl_->tree.get()), byte, byte);
  while (!ts_node_is_null(node) && !ts_node_is_named(node)) {
    node = ts_node_parent(node);
  }
  if (ts_node_is_null(node)) {
    return false;
  }
  if (outType) {
    const char* type = ts_node_type(node);
    *outType = type ? type : "";
  }
  if (outStart) {
    *outStart = ts_node_start_byte(node);
  }
  if (outEnd) {
    *outEnd = ts_node_end_byte(node);
  }
  return true;
}

}  // namespace codecore
