#include <cstdio>
#include <cstring>
#include <string>

#include "codecore/codecore.h"
#include "core/syntax/syntax.h"

namespace {

int checks = 0;
int failures = 0;

#define CHECK(condition)                                                       \
  do {                                                                         \
    ++checks;                                                                  \
    if (!(condition)) {                                                        \
      ++failures;                                                              \
      std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #condition);         \
    }                                                                          \
  } while (0)

bool hasCapture(const std::vector<codecore::SyntaxHighlight>& highlights,
                const std::string& capture) {
  for (const auto& highlight : highlights) {
    if (highlight.capture == capture) {
      return true;
    }
  }
  return false;
}

void testParse() {
  const std::string code =
      "def add(a, b):\n"
      "    return a + b\n"
      "\n"
      "print(add(1, 2))\n";
  auto tree = codecore::SyntaxTree::parse(code, "python");
  CHECK(tree != nullptr);
  if (!tree) {
    return;
  }

  CHECK(tree->languageId() == "python");
  CHECK(tree->text() == code);
  CHECK(tree->rootType() == "module");
  CHECK(!tree->hasErrors());
  CHECK(tree->nodeCount() > 10);

  const auto highlights = tree->highlights();
  CHECK(!highlights.empty());
  CHECK(hasCapture(highlights, "keyword"));
  CHECK(hasCapture(highlights, "function"));
  CHECK(hasCapture(highlights, "number"));

  for (size_t index = 1; index < highlights.size(); ++index) {
    CHECK(highlights[index - 1].start <= highlights[index].start);
  }
  for (const auto& highlight : highlights) {
    CHECK(highlight.start < highlight.end);
    CHECK(highlight.end <= code.size());
  }
}

void testErrors() {
  auto broken = codecore::SyntaxTree::parse("def broken(:\n    pass\n", "python");
  CHECK(broken != nullptr);
  if (broken) {
    CHECK(broken->hasErrors());
    const auto errors = broken->errors();
    CHECK(!errors.empty());
    if (!errors.empty()) {
      CHECK(errors.front().end <= broken->text().size());
      CHECK(errors.front().missing || errors.front().nodeType == "ERROR");
    }
  }

  auto healthy = codecore::SyntaxTree::parse("value = 1\n", "python");
  CHECK(healthy != nullptr);
  if (healthy) {
    CHECK(!healthy->hasErrors());
    CHECK(healthy->errors().empty());
  }
}

void testIncrementalReparse() {
  const std::string first =
      "def greet(name):\n"
      "    return 'hello ' + name\n";
  auto original = codecore::SyntaxTree::parse(first, "python");
  CHECK(original != nullptr);
  if (!original) {
    return;
  }

  const std::string second =
      "def greet(name):\n"
      "    message = 'hello ' + name\n"
      "    return message\n";
  auto updated = codecore::SyntaxTree::reparse(*original, second);
  CHECK(updated != nullptr);
  if (!updated) {
    return;
  }

  CHECK(updated->text() == second);
  CHECK(updated->rootType() == "module");
  CHECK(!updated->hasErrors());
  CHECK(updated->highlights().size() > original->highlights().size());

  /* The previous tree is immutable and must still describe the old text. */
  CHECK(original->text() == first);
  CHECK(!original->hasErrors());

  const std::string third = "def greet(name):\n";
  auto shrunk = codecore::SyntaxTree::reparse(*updated, third);
  CHECK(shrunk != nullptr);
  if (shrunk) {
    CHECK(shrunk->text() == third);
    CHECK(shrunk->rootType() == "module");
    CHECK(!shrunk->hasErrors());
  }

  auto unchanged = codecore::SyntaxTree::reparse(*shrunk, third);
  CHECK(unchanged != nullptr);
  if (unchanged) {
    CHECK(unchanged->text() == third);
    CHECK(!unchanged->hasErrors());
  }
}

void testNodeAt() {
  const std::string code = "def total(a, b):\n    return a + b\n";
  auto tree = codecore::SyntaxTree::parse(code, "python");
  CHECK(tree != nullptr);
  if (!tree) {
    return;
  }

  std::string type;
  size_t start = 0;
  size_t end = 0;
  CHECK(tree->nodeAt(4, &type, &start, &end));
  CHECK(type == "identifier");
  CHECK(start == 4);
  CHECK(end == 9);

  CHECK(tree->nodeAt(code.size(), &type, &start, &end));
}

void testCAbi() {
  CHECK(cc_abi_version() == 5);
  CHECK(cc_syntax_language_supported("python") == 1);
  CHECK(cc_syntax_language_supported("javascript") == 1);
  CHECK(cc_syntax_language_supported("js") == 1);
  CHECK(cc_syntax_language_supported("rust") == 0);

  cc_engine* engine = cc_engine_create(nullptr);
  CHECK(engine != nullptr);
  if (!engine) {
    return;
  }
  const int32_t python = cc_engine_find_language(engine, "python");
  CHECK(python >= 0);

  cc_document_options options{};
  options.struct_size = sizeof(options);
  options.language_id = python;
  cc_document* document = cc_document_create(engine, &options);
  CHECK(document != nullptr);
  if (!document) {
    cc_engine_destroy(engine);
    return;
  }

  const char* code =
      "def add(a, b):\n"
      "    return a + b\n";
  CHECK(cc_document_set_text(document, code, std::strlen(code)) == CC_STATUS_OK);

  cc_syntax_tree* tree = cc_document_syntax_tree(document);
  CHECK(tree != nullptr);
  if (tree) {
    CHECK(std::strcmp(cc_syntax_tree_language(tree), "python") == 0);
    CHECK(std::strcmp(cc_syntax_tree_root_type(tree), "module") == 0);
    CHECK(cc_syntax_tree_node_count(tree) > 10);
    CHECK(cc_syntax_tree_has_errors(tree) == 0);
    CHECK(cc_syntax_tree_highlight_count(tree) > 0);
    CHECK(cc_syntax_tree_error_count(tree) == 0);

    bool sawKeyword = false;
    for (size_t index = 0; index < cc_syntax_tree_highlight_count(tree); ++index) {
      cc_syntax_highlight highlight{};
      CHECK(cc_syntax_tree_highlight_at(tree, index, &highlight) == 1);
      if (std::strcmp(highlight.capture, "keyword") == 0) {
        sawKeyword = true;
      }
    }
    CHECK(sawKeyword);
    cc_syntax_tree_destroy(tree);
  }

  cc_document_insert(document, 0, "# heading\n", 10);
  cc_syntax_tree* updated = cc_document_syntax_tree(document);
  CHECK(updated != nullptr);
  if (updated) {
    CHECK(std::strcmp(cc_syntax_tree_root_type(updated), "module") == 0);
    CHECK(cc_syntax_tree_highlight_count(updated) > 0);
    cc_syntax_tree_destroy(updated);
  }

  /* The same document path works for the second grammar. */
  const int32_t javascript = cc_engine_find_language(engine, "javascript");
  CHECK(javascript >= 0);
  CHECK(cc_document_set_language(document, javascript) == CC_STATUS_OK);
  const char* jsCode =
      "function sum(a, b) {\n"
      "  return a + b;\n"
      "}\n";
  CHECK(cc_document_set_text(document, jsCode, std::strlen(jsCode)) == CC_STATUS_OK);
  cc_syntax_tree* jsTree = cc_document_syntax_tree(document);
  CHECK(jsTree != nullptr);
  if (jsTree) {
    CHECK(std::strcmp(cc_syntax_tree_language(jsTree), "javascript") == 0);
    CHECK(std::strcmp(cc_syntax_tree_root_type(jsTree), "program") == 0);
    CHECK(cc_syntax_tree_has_errors(jsTree) == 0);
    CHECK(cc_syntax_tree_highlight_count(jsTree) > 0);
    cc_syntax_tree_destroy(jsTree);
  }

  /* A language without a bundled grammar must not fabricate a tree. */
  CHECK(cc_document_set_language(document, cc_engine_find_language(engine, "rust")) ==
        CC_STATUS_OK);
  cc_syntax_tree* unsupported = cc_document_syntax_tree(document);
  CHECK(unsupported == nullptr);

  cc_document_destroy(document);
  cc_engine_destroy(engine);
}

void testJavaScript() {
  const std::string code =
      "// greeting\n"
      "function greet(name) {\n"
      "  const message = `hello ${name}`;\n"
      "  return message;\n"
      "}\n"
      "greet('world');\n";
  auto tree = codecore::SyntaxTree::parse(code, "javascript");
  CHECK(tree != nullptr);
  if (!tree) {
    return;
  }

  CHECK(tree->languageId() == "javascript");
  CHECK(tree->rootType() == "program");
  CHECK(!tree->hasErrors());
  CHECK(tree->nodeCount() > 20);

  const auto highlights = tree->highlights();
  CHECK(!highlights.empty());
  CHECK(hasCapture(highlights, "comment"));
  CHECK(hasCapture(highlights, "keyword"));
  CHECK(hasCapture(highlights, "function"));
  CHECK(hasCapture(highlights, "string"));
  for (const auto& highlight : highlights) {
    CHECK(highlight.start < highlight.end);
    CHECK(highlight.end <= code.size());
  }

  /* Aliases resolve to the canonical grammar id. */
  auto alias = codecore::SyntaxTree::parse(code, "js");
  CHECK(alias != nullptr);
  if (alias) {
    CHECK(alias->languageId() == "javascript");
    CHECK(!alias->highlights().empty());
  }

  /* JSX is part of the JavaScript grammar, so tags parse without errors. */
  auto jsx = codecore::SyntaxTree::parse("const el = <div className=\"a\">hi</div>;\n", "jsx");
  CHECK(jsx != nullptr);
  if (jsx) {
    CHECK(!jsx->hasErrors());
    CHECK(hasCapture(jsx->highlights(), "property"));
  }

  auto broken = codecore::SyntaxTree::parse("function () {\n", "javascript");
  CHECK(broken != nullptr);
  if (broken) {
    CHECK(broken->hasErrors());
    CHECK(!broken->errors().empty());
  }

  const std::string next =
      "// greeting\n"
      "function greet(name) {\n"
      "  const message = `hello ${name}`;\n"
      "  console.log(message);\n"
      "  return message;\n"
      "}\n"
      "greet('world');\n";
  auto updated = codecore::SyntaxTree::reparse(*tree, next);
  CHECK(updated != nullptr);
  if (updated) {
    CHECK(updated->text() == next);
    CHECK(!updated->hasErrors());
    CHECK(updated->nodeCount() > tree->nodeCount());
    CHECK(tree->text() == code);
  }
}

void testUnsupportedLanguage() {
  CHECK(!codecore::syntaxLanguageSupported("rust"));
  CHECK(!codecore::syntaxLanguageSupported(""));
  CHECK(codecore::syntaxLanguageSupported("python"));
  CHECK(codecore::syntaxLanguageSupported("javascript"));
  CHECK(codecore::syntaxLanguageSupported("js"));
  CHECK(codecore::SyntaxTree::parse("fn main() {}\n", "rust") == nullptr);
}

}  // namespace

int main() {
  testParse();
  testErrors();
  testIncrementalReparse();
  testNodeAt();
  testJavaScript();
  testUnsupportedLanguage();
  testCAbi();

  std::printf("Syntax checks: %d, failures: %d\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
