#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "core/document/document.h"
#include "core/language/language.h"

namespace codecore {

struct EngineConfig {
  size_t maxDocumentBytes = 64u * 1024u * 1024u;
};

class Engine {
 public:
  explicit Engine(const EngineConfig& config = EngineConfig());

  size_t languageCount() const;
  const Language* languageAt(size_t index) const;
  int32_t findLanguage(const std::string& id) const;
  std::shared_ptr<const Language> languageById(int32_t id) const;
  std::shared_ptr<const Language> languageByIdString(const std::string& id) const;

  std::unique_ptr<Document> createDocument(const DocumentOptions& options) const;

  const EngineConfig& config() const { return config_; }

 private:
  LanguageRegistry registry_;
  EngineConfig config_;
};

}  // namespace codecore
