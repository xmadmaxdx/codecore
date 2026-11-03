#include "codecore/codecore.h"

#include <cstdio>
#include <string>

#include "core/analysis/background.h"
#include "core/document/document.h"
#include "core/engine/engine.h"
#include "core/viewport/viewport.h"

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

std::string makeSource(int lines) {
  std::string source;
  source.reserve(static_cast<size_t>(lines) * 16);
  for (int index = 0; index < lines; ++index) {
    source += "value_";
    source += std::to_string(index);
    source += " = ";
    source += std::to_string(index);
    source += "\n";
  }
  return source;
}

void testViewportMath() {
  const codecore::ViewportRange range =
      codecore::visibleLineRange(1000, 2000, 400, 20, 3);
  CHECK(range.firstLine == 97);
  CHECK(range.endLine == 123);

  const codecore::ViewportRange top = codecore::visibleLineRange(1000, 0, 400, 20, 3);
  CHECK(top.firstLine == 0);
  CHECK(top.endLine == 23);

  const codecore::ViewportRange bottom =
      codecore::visibleLineRange(1000, 19980, 400, 20, 3);
  CHECK(bottom.firstLine == 996);
  CHECK(bottom.endLine == 1000);

  const codecore::ViewportRange empty = codecore::visibleLineRange(0, 0, 400, 20, 3);
  CHECK(empty.firstLine == 0 && empty.endLine == 0);
  CHECK(codecore::lineTop(10, 20) == 200);
  CHECK(codecore::contentHeight(1000, 20) == 20000);

  cc_viewport_range cRange{};
  CHECK(cc_viewport_visible_range(1000, 2000, 400, 20, 3, &cRange) == CC_STATUS_OK);
  CHECK(cRange.first_line == 97 && cRange.end_line == 123);
  CHECK(cc_viewport_visible_range(1, 0, 1, 1, 0, nullptr) == CC_STATUS_INVALID_ARGUMENT);
}

void testBackgroundAnalysis() {
  codecore::Engine engine;
  const auto language = engine.languageByIdString("python");
  CHECK(language != nullptr);

  codecore::DocumentOptions options;
  options.tabSize = 4;
  options.insertSpaces = true;
  codecore::Document document(language, options, 8u * 1024u * 1024u);
  CHECK(document.setText(makeSource(5000)) == codecore::Status::Ok);

  const auto first = document.createSnapshot();
  CHECK(first != nullptr);

  codecore::BackgroundAnalyzer analyzer;
  analyzer.submit(first);
  analyzer.waitIdle();

  codecore::BackgroundAnalysisResult result;
  CHECK(analyzer.takeLatest(&result));
  CHECK(result.version == first->version());
  CHECK(result.analysis != nullptr);
  CHECK(!result.analysis->tokenize(0, first->length()).empty());
  CHECK(!analyzer.takeLatest(&result));
  CHECK(analyzer.completedCount() == 1);

  CHECK(document.insert(0, "# header\n") == codecore::Status::Ok);
  const auto second = document.createSnapshot();
  CHECK(second->version() > first->version());

  analyzer.submit(first);
  analyzer.submit(second);
  analyzer.waitIdle();
  CHECK(analyzer.takeLatest(&result));
  CHECK(result.version == second->version());
  CHECK(analyzer.droppedCount() >= 1);

  analyzer.cancel();
  analyzer.submit(second);
  analyzer.cancel();
  analyzer.waitIdle();
  codecore::BackgroundAnalysisResult cancelled;
  CHECK(!analyzer.takeLatest(&cancelled));
}

void testCAnalyzer() {
  cc_engine* engine = cc_engine_create(nullptr);
  CHECK(engine != nullptr);
  cc_document* document = cc_document_create(engine, nullptr);
  CHECK(document != nullptr);
  const std::string source = makeSource(200);
  CHECK(cc_document_set_text(document, source.data(), source.size()) == CC_STATUS_OK);

  cc_analyzer* analyzer = cc_analyzer_create();
  CHECK(analyzer != nullptr);
  CHECK(cc_analyzer_submit(analyzer, document) == CC_STATUS_OK);
  cc_analyzer_wait(analyzer);
  cc_snapshot* snapshot = cc_analyzer_take_result(analyzer);
  CHECK(snapshot != nullptr);
  CHECK(cc_snapshot_version(snapshot) == cc_document_version(document));
  CHECK(cc_snapshot_length(snapshot) == source.size());
  cc_snapshot_destroy(snapshot);
  CHECK(cc_analyzer_completed(analyzer) == 1);
  cc_analyzer_destroy(analyzer);

  cc_document_destroy(document);
  cc_engine_destroy(engine);
}

}  // namespace

int main() {
  testViewportMath();
  testBackgroundAnalysis();
  testCAnalyzer();
  std::printf("%d checks, %d failures\n", checks, failures);
  return failures == 0 ? 0 : 1;
}
