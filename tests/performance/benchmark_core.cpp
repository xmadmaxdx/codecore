// Manual CodeCore benchmark. Not registered with CTest.
//
// Build:  cmake --build build/native --target codecore_benchmark
// Run:    ./build/native/codecore_benchmark.exe

#include "codecore/codecore.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

double milliseconds(Clock::duration duration) {
  return std::chrono::duration<double, std::milli>(duration).count();
}

double microseconds(Clock::duration duration) {
  return std::chrono::duration<double, std::micro>(duration).count();
}

void freeTokens(cc_token* tokens) { cc_tokens_free(tokens); }

}  // namespace

int main() {
  cc_engine* engine = cc_engine_create(nullptr);
  if (!engine) {
    std::printf("engine creation failed\n");
    return 1;
  }
  cc_document* document = cc_document_create(engine, nullptr);
  if (!document) {
    std::printf("document creation failed\n");
    cc_engine_destroy(engine);
    return 1;
  }

  std::string source;
  source.reserve(1100000);
  int functionIndex = 0;
  while (source.size() < 1000000) {
    source += "def calculate_total_";
    source += std::to_string(functionIndex);
    source += "(price, quantity):\n";
    source += "    \"\"\"Return price times quantity.\"\"\"\n";
    source += "    return price * quantity  # total\n\n";
    ++functionIndex;
  }
  const auto setStart = Clock::now();
  if (cc_document_set_text(document, source.data(), source.size()) != CC_STATUS_OK) {
    std::printf("set text failed\n");
    return 1;
  }
  const auto setEnd = Clock::now();

  cc_token* tokens = nullptr;
  const auto firstTokenStart = Clock::now();
  size_t count = cc_document_tokenize(document, 0, cc_document_length(document), &tokens);
  const auto firstTokenEnd = Clock::now();
  freeTokens(tokens);

  constexpr int kRepeat = 30;
  const auto repeatStart = Clock::now();
  for (int index = 0; index < kRepeat; ++index) {
    tokens = nullptr;
    cc_document_tokenize(document, 0, cc_document_length(document), &tokens);
    freeTokens(tokens);
  }
  const auto repeatEnd = Clock::now();

  // A single keystroke at the start of the 1 MB document exercises the
  // incremental line index and analysis invalidation.
  const auto editStart = Clock::now();
  for (int index = 0; index < 50; ++index) {
    cc_document_insert(document, 0, "x", 1);
  }
  const auto editEnd = Clock::now();

  const auto snapshotStart = Clock::now();
  cc_snapshot* snapshot = cc_document_create_snapshot(document);
  const auto snapshotEnd = Clock::now();
  if (!snapshot) {
    std::printf("snapshot creation failed\n");
    return 1;
  }

  const auto snapshotTokenStart = Clock::now();
  tokens = nullptr;
  cc_snapshot_tokenize(snapshot, 0, cc_snapshot_length(snapshot), &tokens);
  const auto snapshotTokenEnd = Clock::now();
  freeTokens(tokens);
  tokens = nullptr;
  cc_snapshot_tokenize(snapshot, 0, cc_snapshot_length(snapshot), &tokens);
  freeTokens(tokens);
  const auto snapshotCachedEnd = Clock::now();

  std::printf("document_bytes=%zu lines=%zu tokens=%zu\n", cc_document_length(document),
              cc_document_line_count(document), count);
  std::printf("set_text_ms=%.2f\n", milliseconds(setEnd - setStart));
  std::printf("first_tokenize_ms=%.2f\n", milliseconds(firstTokenEnd - firstTokenStart));
  std::printf("cached_tokenize_avg_us=%.1f\n",
              microseconds(repeatEnd - repeatStart) / kRepeat);
  std::printf("incremental_insert_avg_us=%.1f\n",
              microseconds(editEnd - editStart) / 50.0);
  std::printf("snapshot_create_ms=%.2f\n", milliseconds(snapshotEnd - snapshotStart));
  std::printf("snapshot_first_tokenize_ms=%.2f\n",
              milliseconds(snapshotTokenEnd - snapshotTokenStart));
  std::printf("snapshot_cached_tokenize_us=%.2f\n",
              microseconds(snapshotCachedEnd - snapshotTokenEnd));

  cc_snapshot_destroy(snapshot);
  cc_document_destroy(document);
  cc_engine_destroy(engine);
  return 0;
}
