#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>

#include "core/analysis/analysis.h"
#include "core/snapshot/snapshot.h"

namespace codecore {

struct BackgroundAnalysisResult {
  uint64_t version = 0;
  std::shared_ptr<Snapshot> snapshot;
  std::shared_ptr<const Analysis> analysis;
};

/*
 * Single-worker analysis queue.
 *
 * The latest submitted snapshot wins: older pending work is replaced, and a
 * result that becomes stale while it is being computed is discarded. This keeps
 * the UI thread from ever waiting for tokenization and prevents an old result
 * from overwriting newer editor state.
 *
 * The worker computes tokens, diagnostics and folding ranges before publishing.
 * Callers poll with takeLatest() and must compare the document version if edits
 * may have occurred after the result was produced.
 */
class BackgroundAnalyzer {
 public:
  BackgroundAnalyzer();
  ~BackgroundAnalyzer();

  BackgroundAnalyzer(const BackgroundAnalyzer&) = delete;
  BackgroundAnalyzer& operator=(const BackgroundAnalyzer&) = delete;

  void submit(std::shared_ptr<Snapshot> snapshot);
  bool takeLatest(BackgroundAnalysisResult* out);
  void waitIdle();
  void cancel();

  size_t droppedCount() const;
  size_t completedCount() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace codecore
