#include "core/engine/engine.h"

namespace codecore {

Engine::Engine(const EngineConfig& config) : config_(config) {
  if (config_.maxDocumentBytes == 0) {
    config_.maxDocumentBytes = 64u * 1024u * 1024u;
  }
}

size_t Engine::languageCount() const { return registry_.count(); }

const Language* Engine::languageAt(size_t index) const { return registry_.at(index); }

int32_t Engine::findLanguage(const std::string& id) const { return registry_.findId(id); }

std::shared_ptr<const Language> Engine::languageById(int32_t id) const {
  return registry_.sharedById(id);
}

std::shared_ptr<const Language> Engine::languageByIdString(const std::string& id) const {
  return registry_.sharedByIdString(id);
}

std::unique_ptr<Document> Engine::createDocument(const DocumentOptions& options) const {
  std::shared_ptr<const Language> language = registry_.sharedById(options.languageId);
  if (!language) {
    language = registry_.sharedById(0);
  }
  return std::make_unique<Document>(std::move(language), options, config_.maxDocumentBytes);
}

}  // namespace codecore
