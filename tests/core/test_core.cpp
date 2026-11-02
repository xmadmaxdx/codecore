#include "codecore/codecore.h"

#include <cstdio>
#include <cstring>
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

std::string documentText(cc_document* document) {
  size_t length = 0;
  char* text = cc_document_get_text(document, &length);
  std::string result(text ? text : "", length);
  cc_string_free(text);
  return result;
}

std::vector<cc_token> tokensOf(cc_document* document, size_t start, size_t end) {
  cc_token* tokens = nullptr;
  const size_t count = cc_document_tokenize(document, start, end, &tokens);
  std::vector<cc_token> result(tokens, tokens + count);
  cc_tokens_free(tokens);
  return result;
}

std::vector<std::string> completionLabels(cc_document* document, size_t offset) {
  cc_completion_item* items = nullptr;
  const size_t count = cc_document_complete(document, offset, &items);
  std::vector<std::string> labels;
  for (size_t index = 0; index < count; ++index) {
    labels.emplace_back(items[index].label ? items[index].label : "");
  }
  cc_completions_free(items, count);
  return labels;
}

bool contains(const std::vector<std::string>& values, const std::string& needle) {
  for (const std::string& value : values) {
    if (value == needle) {
      return true;
    }
  }
  return false;
}

bool hasToken(const std::vector<cc_token>& tokens, uint32_t kind) {
  for (const cc_token& token : tokens) {
    if (token.kind == kind) {
      return true;
    }
  }
  return false;
}

uint32_t tokenKindAt(cc_document* document, size_t offset) {
  const std::vector<cc_token> tokens = tokensOf(document, 0, cc_document_length(document));
  for (const cc_token& token : tokens) {
    if (token.start <= offset && offset < token.end) {
      return token.kind;
    }
  }
  return CC_TOKEN_PLAIN;
}

cc_document* makeDocument(cc_engine* engine, int32_t languageId = 0) {
  cc_document_options options{};
  options.struct_size = sizeof(cc_document_options);
  options.language_id = languageId;
  options.tab_size = 4;
  options.insert_spaces = 1;
  return cc_document_create(engine, &options);
}

void testVersionAndLanguages(cc_engine* engine) {
  CHECK(cc_abi_version() == CODECORE_ABI_VERSION);
  CHECK(cc_version_string() != nullptr);
  CHECK(std::strcmp(cc_version_string(), "0.1.0") == 0);
  CHECK(cc_engine_language_count(engine) == 23);
  CHECK(cc_engine_find_language(engine, "python") == 0);
  CHECK(cc_engine_find_language(engine, "kotlin") == 1);
  CHECK(cc_engine_find_language(engine, "not-a-language") == -1);
  CHECK(cc_engine_language_at(engine, 1000) == nullptr);

  bool pythonSeen = false;
  bool extensionsSeen = false;
  for (size_t index = 0; index < cc_engine_language_count(engine); ++index) {
    const cc_language_info* info = cc_engine_language_at(engine, index);
    CHECK(info != nullptr);
    if (!info) {
      continue;
    }
    CHECK(info->id == static_cast<int32_t>(index));
    CHECK(info->id_string != nullptr);
    CHECK(info->display_name != nullptr);
    CHECK(info->extensions != nullptr);
    CHECK((info->capabilities & CC_LANGUAGE_CAP_SYNTAX) != 0);
    if (std::strcmp(info->id_string, "python") == 0) {
      pythonSeen = true;
      extensionsSeen = std::strstr(info->extensions, ".py") != nullptr;
    }
  }
  CHECK(pythonSeen);
  CHECK(extensionsSeen);
}

void testBasics(cc_engine* engine) {
  cc_document* document = makeDocument(engine);
  CHECK(document != nullptr);
  const std::string source = "alpha\nbeta\n";
  CHECK(cc_document_set_text(document, source.data(), source.size()) == CC_STATUS_OK);
  CHECK(cc_document_length(document) == source.size());
  CHECK(documentText(document) == source);
  CHECK(cc_document_line_count(document) == 3);

  cc_line_info line{};
  CHECK(cc_document_line_at(document, 0, &line) == CC_STATUS_OK);
  CHECK(line.start == 0 && line.content_end == 5 && line.end == 6);
  CHECK(cc_document_line_at(document, 1, &line) == CC_STATUS_OK);
  CHECK(line.start == 6 && line.content_end == 10 && line.end == 11);
  CHECK(cc_document_line_at(document, 2, &line) == CC_STATUS_OK);
  CHECK(line.start == 11 && line.content_end == 11 && line.end == 11);
  CHECK(cc_document_line_at(document, 3, &line) == CC_STATUS_OUT_OF_RANGE);

  cc_position position{};
  CHECK(cc_document_offset_to_position(document, 6, &position) == CC_STATUS_OK);
  CHECK(position.line == 1 && position.column == 0);
  CHECK(cc_document_offset_to_position(document, 8, &position) == CC_STATUS_OK);
  CHECK(position.line == 1 && position.column == 2);

  size_t offset = 999;
  CHECK(cc_document_position_to_offset(document, 1, 2, &offset) == CC_STATUS_OK);
  CHECK(offset == 8);
  CHECK(cc_document_position_to_offset(document, 1, 999, &offset) == CC_STATUS_OK);
  CHECK(offset == 10);
  CHECK(cc_document_position_to_offset(document, 99, 0, &offset) == CC_STATUS_OUT_OF_RANGE);

  cc_selection selection = cc_document_get_selection(document);
  CHECK(selection.anchor == 0 && selection.focus == 0);
  CHECK(cc_document_set_selection(document, 2, 5) == CC_STATUS_OK);
  CHECK(cc_document_move_cursor(document, CC_MOVE_LEFT, 0) == CC_STATUS_OK);
  selection = cc_document_get_selection(document);
  CHECK(selection.anchor == 2 && selection.focus == 2);
  CHECK(cc_document_set_selection(document, 2, 5) == CC_STATUS_OK);
  CHECK(cc_document_move_cursor(document, CC_MOVE_LEFT, 1) == CC_STATUS_OK);
  selection = cc_document_get_selection(document);
  CHECK(selection.anchor == 2 && selection.focus == 4);
  CHECK(cc_document_move_cursor(document, CC_MOVE_DOC_END, 0) == CC_STATUS_OK);
  selection = cc_document_get_selection(document);
  CHECK(selection.focus == source.size());
  CHECK(cc_document_set_selection(document, 99, 0) == CC_STATUS_OUT_OF_RANGE);

  cc_document_destroy(document);
}

void testEditing(cc_engine* engine) {
  cc_document* document = makeDocument(engine);
  CHECK(cc_document_set_text(document, "hello world", 11) == CC_STATUS_OK);
  const uint64_t versionBefore = cc_document_version(document);
  CHECK(cc_document_insert(document, 5, ",", 1) == CC_STATUS_OK);
  CHECK(documentText(document) == "hello, world");
  CHECK(cc_document_version(document) > versionBefore);
  CHECK(cc_document_erase(document, 5, 6) == CC_STATUS_OK);
  CHECK(documentText(document) == "hello world");
  CHECK(cc_document_replace(document, 6, 11, "there", 5) == CC_STATUS_OK);
  CHECK(documentText(document) == "hello there");
  CHECK(cc_document_replace(document, 0, 5, "hi", 2) == CC_STATUS_OK);
  CHECK(documentText(document) == "hi there");

  CHECK(cc_document_can_undo(document) == 1);
  CHECK(cc_document_undo(document) == CC_STATUS_OK);
  CHECK(documentText(document) == "hello there");
  CHECK(cc_document_undo(document) == CC_STATUS_OK);
  CHECK(documentText(document) == "hello world");
  CHECK(cc_document_can_redo(document) == 1);
  CHECK(cc_document_redo(document) == CC_STATUS_OK);
  CHECK(documentText(document) == "hello there");
  cc_document_clear_history(document);
  CHECK(cc_document_can_undo(document) == 0);
  CHECK(cc_document_can_redo(document) == 0);
  cc_document_destroy(document);

  // Consecutive typing coalesces into one undo step.
  document = makeDocument(engine);
  CHECK(cc_document_insert(document, 0, "a", 1) == CC_STATUS_OK);
  CHECK(cc_document_insert(document, 1, "b", 1) == CC_STATUS_OK);
  CHECK(cc_document_insert(document, 2, "c", 1) == CC_STATUS_OK);
  CHECK(documentText(document) == "abc");
  CHECK(cc_document_undo(document) == CC_STATUS_OK);
  CHECK(documentText(document) == "");
  CHECK(cc_document_can_undo(document) == 0);
  cc_document_destroy(document);

  // History trimming: erases are discrete undo steps.
  document = makeDocument(engine);
  CHECK(cc_document_set_text(document, "abc", 3) == CC_STATUS_OK);
  cc_document_set_history_limit(document, 1);
  CHECK(cc_document_erase(document, 0, 1) == CC_STATUS_OK);
  CHECK(cc_document_erase(document, 0, 1) == CC_STATUS_OK);
  CHECK(documentText(document) == "c");
  CHECK(cc_document_undo(document) == CC_STATUS_OK);
  CHECK(documentText(document) == "bc");
  CHECK(cc_document_can_undo(document) == 0);
  cc_document_destroy(document);

  // Read-only and validation.
  document = makeDocument(engine);
  CHECK(cc_document_set_text(document, "abc", 3) == CC_STATUS_OK);
  CHECK(cc_document_set_read_only(document, 1) == CC_STATUS_OK);
  CHECK(cc_document_is_read_only(document) == 1);
  CHECK(cc_document_insert(document, 0, "x", 1) == CC_STATUS_READ_ONLY);
  CHECK(cc_document_erase(document, 0, 1) == CC_STATUS_READ_ONLY);
  CHECK(cc_document_undo(document) == CC_STATUS_READ_ONLY);
  CHECK(cc_document_set_read_only(document, 0) == CC_STATUS_OK);

  const char invalidUtf8[] = {'a', static_cast<char>(0xFF), 'b'};
  CHECK(cc_document_insert(document, 0, invalidUtf8, 3) == CC_STATUS_INVALID_ARGUMENT);
  CHECK(cc_document_insert(document, 99, "x", 1) == CC_STATUS_OUT_OF_RANGE);
  CHECK(cc_document_erase(document, 2, 1) == CC_STATUS_OUT_OF_RANGE);
  CHECK(cc_document_insert(document, 0, nullptr, 1) == CC_STATUS_INVALID_ARGUMENT);
  cc_document_destroy(document);

  // Multi-byte boundary validation.
  document = makeDocument(engine);
  const char accented[] = "\xC3\xA9";  // é
  CHECK(cc_document_set_text(document, accented, 2) == CC_STATUS_OK);
  CHECK(cc_document_insert(document, 1, "x", 1) == CC_STATUS_INVALID_ARGUMENT);
  CHECK(cc_document_insert(document, 2, "x", 1) == CC_STATUS_OK);
  CHECK(documentText(document) == "\xC3\xA9x");
  cc_document_destroy(document);
}

void testUnicode(cc_engine* engine) {
  cc_document* document = makeDocument(engine);
  const std::string source = "h\xC3\xA9llo \xF0\x9F\x98\x80\nsecond";
  CHECK(cc_document_set_text(document, source.data(), source.size()) == CC_STATUS_OK);
  CHECK(cc_document_length(document) == source.size());
  CHECK(cc_document_line_count(document) == 2);

  // "h"=1, "é"=2, "llo "=4, emoji=4 -> newline at byte 12.
  cc_position position{};
  CHECK(cc_document_offset_to_position(document, 3, &position) == CC_STATUS_OK);
  CHECK(position.line == 0 && position.column == 2);  // after h + é
  CHECK(cc_document_offset_to_position(document, 7, &position) == CC_STATUS_OK);
  CHECK(position.line == 0 && position.column == 6);  // before emoji
  CHECK(cc_document_offset_to_position(document, 11, &position) == CC_STATUS_OK);
  CHECK(position.line == 0 && position.column == 7);  // after emoji

  size_t offset = 0;
  CHECK(cc_document_position_to_offset(document, 0, 5, &offset) == CC_STATUS_OK);
  CHECK(offset == 6);
  CHECK(cc_document_position_to_offset(document, 0, 7, &offset) == CC_STATUS_OK);
  CHECK(offset == 11);
  CHECK(cc_document_position_to_offset(document, 1, 6, &offset) == CC_STATUS_OK);
  CHECK(offset == source.size());

  // Cursor movement respects code point boundaries.
  CHECK(cc_document_set_selection(document, 7, 7) == CC_STATUS_OK);
  CHECK(cc_document_move_cursor(document, CC_MOVE_RIGHT, 0) == CC_STATUS_OK);
  cc_selection selection = cc_document_get_selection(document);
  CHECK(selection.focus == 11);
  CHECK(cc_document_move_cursor(document, CC_MOVE_LEFT, 0) == CC_STATUS_OK);
  selection = cc_document_get_selection(document);
  CHECK(selection.focus == 7);
  cc_document_destroy(document);
}

void testTokens(cc_engine* engine) {
  cc_document* python = makeDocument(engine, 0);
  const std::string code =
      "# comment\n"
      "def calculate_total(price, quantity):\n"
      "    \"\"\"documentation\"\"\"\n"
      "    return price * quantity\n"
      "\n"
      "print(calculate_total(2, 3))\n";
  CHECK(cc_document_set_text(python, code.data(), code.size()) == CC_STATUS_OK);
  const std::vector<cc_token> tokens = tokensOf(python, 0, code.size());
  CHECK(!tokens.empty());
  CHECK(hasToken(tokens, CC_TOKEN_COMMENT));
  CHECK(hasToken(tokens, CC_TOKEN_KEYWORD));
  CHECK(hasToken(tokens, CC_TOKEN_FUNCTION));
  CHECK(hasToken(tokens, CC_TOKEN_STRING));
  CHECK(hasToken(tokens, CC_TOKEN_NUMBER));
  CHECK(tokenKindAt(python, code.find("def")) == CC_TOKEN_KEYWORD);
  CHECK(tokenKindAt(python, code.find("calculate_total")) == CC_TOKEN_FUNCTION);
  CHECK(tokenKindAt(python, code.find("\"\"\"")) == CC_TOKEN_STRING);
  CHECK(tokenKindAt(python, code.find("return")) == CC_TOKEN_KEYWORD);

  // Range filtering.
  const size_t commentStart = 0;
  const std::vector<cc_token> ranged = tokensOf(python, 0, 10);
  CHECK(!ranged.empty());
  CHECK(ranged.front().start == commentStart);

  // HTML is tag/attribute/string aware.
  cc_document* html = makeDocument(engine, 14);
  const std::string markup = "<div class=\"card\" data-id='7'>Hello</div>";
  CHECK(cc_document_set_text(html, markup.data(), markup.size()) == CC_STATUS_OK);
  const std::vector<cc_token> htmlTokens = tokensOf(html, 0, markup.size());
  CHECK(hasToken(htmlTokens, CC_TOKEN_TAG));
  CHECK(hasToken(htmlTokens, CC_TOKEN_ATTRIBUTE));
  CHECK(hasToken(htmlTokens, CC_TOKEN_STRING));
  cc_document_destroy(html);

  // SQL keyword lookup is case-insensitive.
  cc_document* sql = makeDocument(engine, 17);
  const std::string query = "select * from users where id = 1\n";
  CHECK(cc_document_set_text(sql, query.data(), query.size()) == CC_STATUS_OK);
  CHECK(tokenKindAt(sql, 0) == CC_TOKEN_KEYWORD);
  CHECK(tokenKindAt(sql, query.find("from")) == CC_TOKEN_KEYWORD);
  cc_document_destroy(sql);

  // CSS property detection.
  cc_document* css = makeDocument(engine, 15);
  const std::string style = ".card {\n  color: red;\n}\n";
  CHECK(cc_document_set_text(css, style.data(), style.size()) == CC_STATUS_OK);
  CHECK(tokenKindAt(css, style.find("color")) == CC_TOKEN_PROPERTY);
  cc_document_destroy(css);

  cc_document_destroy(python);
}

void testCompletion(cc_engine* engine) {
  cc_document* document = makeDocument(engine, 0);
  const std::string code =
      "def make_widget(name):\n"
      "    return name\n"
      "\n"
      "make_";
  CHECK(cc_document_set_text(document, code.data(), code.size()) == CC_STATUS_OK);
  const std::vector<std::string> labels = completionLabels(document, code.size());
  CHECK(!labels.empty());
  CHECK(contains(labels, "make_widget"));
  CHECK(labels.size() <= 100);

  const std::string stringCode = "value = \"pri";
  CHECK(cc_document_set_text(document, stringCode.data(), stringCode.size()) == CC_STATUS_OK);
  CHECK(completionLabels(document, stringCode.size()).empty());

  CHECK(cc_document_set_text(document, "pri", 3) == CC_STATUS_OK);
  const std::vector<std::string> keywordLabels = completionLabels(document, 3);
  CHECK(contains(keywordLabels, "print"));
  cc_document_destroy(document);
}

void testDiagnostics(cc_engine* engine) {
  cc_document* document = makeDocument(engine, 0);
  const std::string valid = "def f():\n    return 1\n";
  CHECK(cc_document_set_text(document, valid.data(), valid.size()) == CC_STATUS_OK);
  cc_diagnostic* diagnostics = nullptr;
  size_t count = cc_document_diagnose(document, &diagnostics);
  CHECK(count == 0);
  cc_diagnostics_free(diagnostics, count);

  const std::string broken = "def f(:\n    pass\n";
  CHECK(cc_document_set_text(document, broken.data(), broken.size()) == CC_STATUS_OK);
  count = cc_document_diagnose(document, &diagnostics);
  CHECK(count >= 1);
  bool unclosed = false;
  for (size_t index = 0; index < count; ++index) {
    if (std::strstr(diagnostics[index].message, "Unclosed") != nullptr) {
      unclosed = true;
    }
    CHECK(diagnostics[index].severity == CC_SEVERITY_ERROR);
    CHECK(diagnostics[index].source != nullptr);
  }
  CHECK(unclosed);
  cc_diagnostics_free(diagnostics, count);

  const std::string unterminated = "value = \"abc\n";
  CHECK(cc_document_set_text(document, unterminated.data(), unterminated.size()) ==
        CC_STATUS_OK);
  count = cc_document_diagnose(document, &diagnostics);
  CHECK(count >= 1);
  cc_diagnostics_free(diagnostics, count);
  cc_document_destroy(document);
}

void testFolding(cc_engine* engine) {
  cc_document* document = makeDocument(engine, 3);
  const std::string code =
      "function outer() {\n"
      "  if (ready) {\n"
      "    work();\n"
      "  }\n"
      "}\n";
  CHECK(cc_document_set_text(document, code.data(), code.size()) == CC_STATUS_OK);
  cc_folding_range* ranges = nullptr;
  const size_t count = cc_document_folding_ranges(document, &ranges);
  CHECK(count >= 2);
  for (size_t index = 0; index < count; ++index) {
    CHECK(ranges[index].end_line > ranges[index].start_line);
    CHECK(ranges[index].kind == CC_FOLDING_BRACE);
  }
  cc_folding_ranges_free(ranges);
  cc_document_destroy(document);
}

void testSearch(cc_engine* engine) {
  cc_document* document = makeDocument(engine);
  const std::string text = "Hello hello world\nhell\n";
  CHECK(cc_document_set_text(document, text.data(), text.size()) == CC_STATUS_OK);
  cc_range* ranges = nullptr;
  cc_search_options options{};
  options.struct_size = sizeof(cc_search_options);

  size_t count = cc_document_find(document, "hello", 5, &options, &ranges);
  CHECK(count == 2);
  cc_search_ranges_free(ranges);

  options.case_sensitive = 1;
  count = cc_document_find(document, "hello", 5, &options, &ranges);
  CHECK(count == 1);
  if (count == 1) {
    CHECK(ranges[0].start == 6);
    CHECK(ranges[0].end == 11);
  }
  cc_search_ranges_free(ranges);

  options.case_sensitive = 0;
  options.whole_word = 1;
  count = cc_document_find(document, "hell", 4, &options, &ranges);
  CHECK(count == 1);
  cc_search_ranges_free(ranges);

  count = cc_document_find(document, "", 0, &options, &ranges);
  CHECK(count == 0);
  cc_document_destroy(document);
}

void testBrackets(cc_engine* engine) {
  cc_document* document = makeDocument(engine, 3);
  const std::string code = "foo(bar[1])";
  CHECK(cc_document_set_text(document, code.data(), code.size()) == CC_STATUS_OK);
  size_t match = 0;
  CHECK(cc_document_match_bracket(document, 3, &match) == CC_STATUS_OK);
  CHECK(match == 10);
  CHECK(cc_document_match_bracket(document, 7, &match) == CC_STATUS_OK);
  CHECK(match == 9);
  // A caret immediately after a bracket resolves that bracket.
  CHECK(cc_document_match_bracket(document, 4, &match) == CC_STATUS_OK);
  CHECK(match == 10);

  const std::string quoted = "s = \"(x)\"";
  CHECK(cc_document_set_text(document, quoted.data(), quoted.size()) == CC_STATUS_OK);
  CHECK(cc_document_match_bracket(document, 5, &match) == CC_STATUS_UNSUPPORTED);
  cc_document_destroy(document);
}

struct EventState {
  int count = 0;
  cc_change_event last{};
};

void changeListener(void* userData, const cc_change_event* event) {
  auto* state = static_cast<EventState*>(userData);
  state->count += 1;
  state->last = *event;
}

void testChangeEvents(cc_engine* engine) {
  cc_document* document = makeDocument(engine);
  EventState state;
  cc_document_set_change_listener(document, changeListener, &state);
  CHECK(cc_document_set_text(document, "abc", 3) == CC_STATUS_OK);
  CHECK(state.count == 1);
  CHECK(state.last.kind == CC_CHANGE_SET_TEXT);
  CHECK(state.last.inserted_length == 3);
  CHECK(cc_document_insert(document, 3, "d", 1) == CC_STATUS_OK);
  CHECK(state.count == 2);
  CHECK(state.last.kind == CC_CHANGE_INSERT);
  CHECK(state.last.start == 3);
  CHECK(state.last.inserted_length == 1);
  CHECK(cc_document_undo(document) == CC_STATUS_OK);
  CHECK(state.count == 3);
  CHECK(state.last.kind == CC_CHANGE_UNDO);
  cc_document_set_change_listener(document, nullptr, nullptr);
  CHECK(cc_document_insert(document, 0, "x", 1) == CC_STATUS_OK);
  CHECK(state.count == 3);
  cc_document_destroy(document);
}

void testAllLanguages(cc_engine* engine) {
  for (size_t index = 0; index < cc_engine_language_count(engine); ++index) {
    const cc_language_info* info = cc_engine_language_at(engine, index);
    cc_document* document = makeDocument(engine, info->id);
    CHECK(document != nullptr);
    const std::string sample =
        (std::strcmp(info->id_string, "html") == 0 ||
         std::strcmp(info->id_string, "xml") == 0)
            ? "<div class=\"card\">text</div>\n"
            : "// comment\nclass Sample {\n  value = 1;\n}\n";
    CHECK(cc_document_set_text(document, sample.data(), sample.size()) == CC_STATUS_OK);
    const std::vector<cc_token> tokens = tokensOf(document, 0, sample.size());
    CHECK(!tokens.empty());
    cc_completion_item* items = nullptr;
    const size_t completions = cc_document_complete(document, sample.size(), &items);
    CHECK(completions <= 100);
    cc_completions_free(items, completions);
    cc_diagnostic* diagnostics = nullptr;
    const size_t diagnosticCount = cc_document_diagnose(document, &diagnostics);
    cc_diagnostics_free(diagnostics, diagnosticCount);
    cc_folding_range* ranges = nullptr;
    const size_t rangeCount = cc_document_folding_ranges(document, &ranges);
    cc_folding_ranges_free(ranges);
    CHECK(rangeCount <= 500);
    cc_document_destroy(document);
  }
}

void testLargeDocument(cc_engine* engine) {
  cc_document* document = makeDocument(engine, 0);
  std::string source;
  source.reserve(400000);
  for (int index = 0; index < 20000; ++index) {
    source += "value_";
    source += std::to_string(index);
    source += " = ";
    source += std::to_string(index);
    source += "\n";
  }
  CHECK(cc_document_set_text(document, source.data(), source.size()) == CC_STATUS_OK);
  CHECK(cc_document_length(document) == source.size());
  CHECK(cc_document_line_count(document) == 20001);  // trailing newline
  CHECK(cc_document_insert(document, 0, "# header\n", 9) == CC_STATUS_OK);
  CHECK(cc_document_insert(document, cc_document_length(document), "# footer\n", 9) ==
        CC_STATUS_OK);
  CHECK(cc_document_line_count(document) == 20003);
  const std::vector<cc_token> tokens = tokensOf(document, 0, cc_document_length(document));
  CHECK(!tokens.empty());
  CHECK(documentText(document).size() == source.size() + 18);
  cc_document_destroy(document);
}

}  // namespace

int main() {
  cc_engine_config config{};
  config.struct_size = sizeof(cc_engine_config);
  config.max_document_bytes = 8u * 1024u * 1024u;
  cc_engine* engine = cc_engine_create(&config);
  if (!engine) {
    std::printf("FATAL: engine creation failed\n");
    return 1;
  }

  testVersionAndLanguages(engine);
  testBasics(engine);
  testEditing(engine);
  testUnicode(engine);
  testTokens(engine);
  testCompletion(engine);
  testDiagnostics(engine);
  testFolding(engine);
  testSearch(engine);
  testBrackets(engine);
  testChangeEvents(engine);
  testAllLanguages(engine);
  testLargeDocument(engine);

  cc_engine_destroy(engine);

  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
