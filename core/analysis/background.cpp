#include "core/analysis/background.h"

#include <condition_variable>
#include <mutex>
#include <thread>
#include <utility>

namespace codecore {

struct BackgroundAnalyzer::Impl {
  mutable std::mutex mutex;
  std::condition_variable idle;
  std::condition_variable wake;

  std::thread worker;
  std::shared_ptr<Snapshot> pending;
  BackgroundAnalysisResult published;

  uint64_t latestSubmitted = 0;
  uint64_t discardGeneration = 0;
  size_t dropped = 0;
  size_t completed = 0;
  bool working = false;
  bool stopping = false;

  Impl() {
    worker = std::thread([this] { run(); });
  }

  ~Impl() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      stopping = true;
      pending.reset();
    }
    wake.notify_all();
    if (worker.joinable()) {
      worker.join();
    }
  }

  void run() {
    for (;;) {
      std::shared_ptr<Snapshot> job;
      uint64_t generation = 0;
      {
        std::unique_lock<std::mutex> lock(mutex);
        wake.wait(lock, [this] { return stopping || pending != nullptr; });
        if (stopping) {
          return;
        }
        job = std::move(pending);
        pending.reset();
        working = true;
        generation = discardGeneration;
      }

      std::shared_ptr<const Analysis> analysis = job->analysis();
      // Force the work here rather than on the caller's thread.
      analysis->tokenize(0, job->text().size());
      analysis->diagnose();
      analysis->foldingRanges();

      {
        std::lock_guard<std::mutex> lock(mutex);
        if (generation != discardGeneration || job->version() < latestSubmitted) {
          ++dropped;
        } else {
          published.version = job->version();
          published.snapshot = job;
          published.analysis = analysis;
        }
        ++completed;
        working = false;
      }
      idle.notify_all();
    }
  }
};

BackgroundAnalyzer::BackgroundAnalyzer() : impl_(std::make_unique<Impl>()) {}

BackgroundAnalyzer::~BackgroundAnalyzer() = default;

void BackgroundAnalyzer::submit(std::shared_ptr<Snapshot> snapshot) {
  if (!snapshot) {
    return;
  }
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->pending) {
      ++impl_->dropped;
    }
    impl_->latestSubmitted = snapshot->version();
    impl_->pending = std::move(snapshot);
  }
  impl_->wake.notify_one();
}

bool BackgroundAnalyzer::takeLatest(BackgroundAnalysisResult* out) {
  if (!out) {
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (!impl_->published.snapshot) {
    return false;
  }
  *out = std::move(impl_->published);
  impl_->published = BackgroundAnalysisResult{};
  return true;
}

void BackgroundAnalyzer::waitIdle() {
  std::unique_lock<std::mutex> lock(impl_->mutex);
  impl_->idle.wait(lock, [this] { return !impl_->working && !impl_->pending; });
}

void BackgroundAnalyzer::cancel() {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  ++impl_->discardGeneration;
  impl_->pending.reset();
  impl_->published = BackgroundAnalysisResult{};
}

size_t BackgroundAnalyzer::droppedCount() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->dropped;
}

size_t BackgroundAnalyzer::completedCount() const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  return impl_->completed;
}

}  // namespace codecore
