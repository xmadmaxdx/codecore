#include "codecore/codecore.h"

#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "core/document/document.h"
#include "core/engine/engine.h"
#include "core/language/language.h"

struct cc_engine {
  codecore::Engine* engine = nullptr;
  std::vector<cc_language_info> languages;
};

struct cc_document {
  codecore::Document* document = nullptr;
  std::vector<std::shared_ptr<const codecore::Language>> languages;
  cc_change_listener listener = nullptr;
  void* user_data = nullptr;
};

struct cc_snapshot {
  std::shared_ptr<codecore::Snapshot> snapshot;
};

namespace {

char* duplicateString(const std::string& value) {
  char* memory = static_cast<char*>(cc_alloc(value.size() + 1));
  if (!memory) {
    return nullptr;
  }
  std::memcpy(memory, value.c_str(), value.size() + 1);
  return memory;
}

cc_status mapStatus(codecore::Status status) {
  return static_cast<cc_status>(static_cast<int32_t>(status));
}

const codecore::Language* findLanguage(
    const std::vector<std::shared_ptr<const codecore::Language>>& languages, int32_t id) {
  for (const auto& language : languages) {
    if (language->id == id) {
      return language.get();
    }
  }
  return nullptr;
}

cc_token* allocateTokens(const std::vector<codecore::Token>& tokens) {
  if (tokens.empty()) {
    return nullptr;
  }
  auto* output = static_cast<cc_token*>(cc_alloc(tokens.size() * sizeof(cc_token)));
  if (!output) {
    return nullptr;
  }
  for (size_t index = 0; index < tokens.size(); ++index) {
    output[index].start = tokens[index].start;
    output[index].end = tokens[index].end;
    output[index].kind = static_cast<uint32_t>(tokens[index].kind);
  }
  return output;
}

cc_completion_item* allocateCompletions(const std::vector<codecore::CompletionItem>& items) {
  if (items.empty()) {
    return nullptr;
  }
  auto* output = static_cast<cc_completion_item*>(
      cc_alloc(items.size() * sizeof(cc_completion_item)));
  if (!output) {
    return nullptr;
  }
  for (size_t index = 0; index < items.size(); ++index) {
    output[index].label = duplicateString(items[index].label);
    output[index].insert_text = duplicateString(items[index].insertText);
    output[index].detail = duplicateString(items[index].detail);
    output[index].kind = static_cast<uint32_t>(items[index].kind);
    output[index].score = items[index].score;
    if (!output[index].label || !output[index].insert_text || !output[index].detail) {
      return output;
    }
  }
  return output;
}

cc_diagnostic* allocateDiagnostics(const std::vector<codecore::Diagnostic>& diagnostics) {
  if (diagnostics.empty()) {
    return nullptr;
  }
  auto* output = static_cast<cc_diagnostic*>(
      cc_alloc(diagnostics.size() * sizeof(cc_diagnostic)));
  if (!output) {
    return nullptr;
  }
  for (size_t index = 0; index < diagnostics.size(); ++index) {
    output[index].start = diagnostics[index].start;
    output[index].end = diagnostics[index].end;
    output[index].severity = diagnostics[index].severity;
    output[index].message = duplicateString(diagnostics[index].message);
    output[index].source = duplicateString(diagnostics[index].source);
  }
  return output;
}

cc_folding_range* allocateFoldingRanges(const std::vector<codecore::FoldingRange>& ranges) {
  if (ranges.empty()) {
    return nullptr;
  }
  auto* output = static_cast<cc_folding_range*>(
      cc_alloc(ranges.size() * sizeof(cc_folding_range)));
  if (!output) {
    return nullptr;
  }
  for (size_t index = 0; index < ranges.size(); ++index) {
    output[index].start_line = ranges[index].startLine;
    output[index].end_line = ranges[index].endLine;
    output[index].kind = ranges[index].kind;
  }
  return output;
}

cc_range* allocateRanges(const std::vector<codecore::Range>& ranges) {
  if (ranges.empty()) {
    return nullptr;
  }
  auto* output = static_cast<cc_range*>(cc_alloc(ranges.size() * sizeof(cc_range)));
  if (!output) {
    return nullptr;
  }
  for (size_t index = 0; index < ranges.size(); ++index) {
    output[index].start = ranges[index].start;
    output[index].end = ranges[index].end;
  }
  return output;
}

cc_diff_line* allocateDiffLines(const std::vector<codecore::DiffLine>& lines) {
  if (lines.empty()) {
    return nullptr;
  }
  auto* output = static_cast<cc_diff_line*>(cc_alloc(lines.size() * sizeof(cc_diff_line)));
  if (!output) {
    return nullptr;
  }
  for (size_t index = 0; index < lines.size(); ++index) {
    output[index].kind = static_cast<uint32_t>(lines[index].kind);
    output[index].old_line = lines[index].oldLine;
    output[index].new_line = lines[index].newLine;
    output[index].text = duplicateString(lines[index].text);
    if (!output[index].text) {
      for (size_t previous = 0; previous < index; ++previous) {
        cc_free(const_cast<char*>(output[previous].text));
      }
      cc_free(output);
      return nullptr;
    }
  }
  return output;
}

void documentChangeThunk(void* user_data, const codecore::ChangeEvent* event) {
  auto* wrapper = static_cast<cc_document*>(user_data);
  if (!wrapper || !wrapper->listener) {
    return;
  }
  cc_change_event publicEvent{};
  publicEvent.version = event->version;
  publicEvent.kind = event->kind;
  publicEvent.start = event->start;
  publicEvent.removed_length = event->removedLength;
  publicEvent.inserted_length = event->insertedLength;
  publicEvent.origin = event->origin;
  wrapper->listener(wrapper->user_data, &publicEvent);
}

}  // namespace

extern "C" {

uint32_t cc_abi_version(void) { return CODECORE_ABI_VERSION; }

const char* cc_version_string(void) { return "0.3.0"; }

void* cc_alloc(size_t size) { return std::malloc(size == 0 ? 1 : size); }

void cc_free(void* pointer) { std::free(pointer); }

void cc_string_free(char* string) { std::free(string); }

cc_engine* cc_engine_create(const cc_engine_config* config) {
  codecore::EngineConfig engineConfig;
  if (config && config->max_document_bytes > 0) {
    engineConfig.maxDocumentBytes = config->max_document_bytes;
  }
  auto* wrapper = new (std::nothrow) cc_engine();
  if (!wrapper) {
    return nullptr;
  }
  wrapper->engine = new (std::nothrow) codecore::Engine(engineConfig);
  if (!wrapper->engine) {
    delete wrapper;
    return nullptr;
  }
  const size_t count = wrapper->engine->languageCount();
  wrapper->languages.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    const codecore::Language* language = wrapper->engine->languageAt(index);
    if (!language) {
      continue;
    }
    cc_language_info info{};
    info.id = language->id;
    info.id_string = language->idString.c_str();
    info.display_name = language->displayName.c_str();
    info.extensions = language->extensions.c_str();
    info.capabilities = language->capabilities;
    wrapper->languages.push_back(info);
  }
  return wrapper;
}

void cc_engine_destroy(cc_engine* engine) {
  if (!engine) {
    return;
  }
  delete engine->engine;
  delete engine;
}

size_t cc_engine_language_count(const cc_engine* engine) {
  return engine ? engine->languages.size() : 0;
}

const cc_language_info* cc_engine_language_at(const cc_engine* engine, size_t index) {
  if (!engine || index >= engine->languages.size()) {
    return nullptr;
  }
  return &engine->languages[index];
}

int32_t cc_engine_find_language(const cc_engine* engine, const char* language_id) {
  if (!engine || !engine->engine || !language_id) {
    return -1;
  }
  return engine->engine->findLanguage(language_id);
}

cc_document* cc_document_create(cc_engine* engine, const cc_document_options* options) {
  if (!engine || !engine->engine) {
    return nullptr;
  }
  codecore::DocumentOptions documentOptions;
  if (options) {
    documentOptions.languageId = options->language_id;
    documentOptions.tabSize = options->tab_size == 0 ? 4 : options->tab_size;
    documentOptions.insertSpaces = options->insert_spaces != 0;
    documentOptions.readOnly = options->read_only != 0;
  }
  std::unique_ptr<codecore::Document> document;
  try {
    document = engine->engine->createDocument(documentOptions);
  } catch (const std::bad_alloc&) {
    return nullptr;
  }
  auto* wrapper = new (std::nothrow) cc_document();
  if (!wrapper) {
    return nullptr;
  }
  wrapper->document = document.release();
  for (const cc_language_info& info : engine->languages) {
    auto language = engine->engine->languageById(info.id);
    if (language) {
      wrapper->languages.push_back(std::move(language));
    }
  }
  return wrapper;
}

void cc_document_destroy(cc_document* document) {
  if (!document) {
    return;
  }
  delete document->document;
  delete document;
}

cc_status cc_document_set_text(cc_document* document, const char* utf8, size_t length) {
  if (!document || !document->document || (!utf8 && length > 0)) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->setText(
      std::string_view(utf8 ? utf8 : "", length)));
}

char* cc_document_get_text(const cc_document* document, size_t* out_length) {
  if (out_length) {
    *out_length = 0;
  }
  if (!document || !document->document) {
    return nullptr;
  }
  const std::string text = document->document->text();
  char* copy = static_cast<char*>(cc_alloc(text.size() + 1));
  if (!copy) {
    return nullptr;
  }
  std::memcpy(copy, text.data(), text.size());
  copy[text.size()] = '\0';
  if (out_length) {
    *out_length = text.size();
  }
  return copy;
}

size_t cc_document_length(const cc_document* document) {
  return document && document->document ? document->document->length() : 0;
}

uint64_t cc_document_version(const cc_document* document) {
  return document && document->document ? document->document->version() : 0;
}

int32_t cc_document_language(const cc_document* document) {
  return document && document->document ? document->document->languageId() : -1;
}

cc_status cc_document_set_language(cc_document* document, int32_t language_id) {
  if (!document || !document->document) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  const codecore::Language* language = findLanguage(document->languages, language_id);
  if (!language) {
    return CC_STATUS_UNKNOWN_LANGUAGE;
  }
  for (const auto& candidate : document->languages) {
    if (candidate->id == language_id) {
      return mapStatus(document->document->setLanguage(candidate));
    }
  }
  return CC_STATUS_UNKNOWN_LANGUAGE;
}

uint8_t cc_document_is_read_only(const cc_document* document) {
  return document && document->document && document->document->readOnly() ? 1 : 0;
}

cc_status cc_document_set_read_only(cc_document* document, uint8_t read_only) {
  if (!document || !document->document) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->setReadOnly(read_only != 0));
}

uint32_t cc_document_tab_size(const cc_document* document) {
  return document && document->document ? document->document->tabSize() : 0;
}

cc_status cc_document_set_editable(cc_document* document, uint8_t editable) {
  if (!document || !document->document) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->setReadOnly(editable == 0));
}

uint32_t cc_document_edit_origin(const cc_document* document) {
  return document && document->document
             ? static_cast<uint32_t>(document->document->editOrigin())
             : 0;
}

cc_status cc_document_set_edit_origin(cc_document* document, uint32_t origin) {
  if (!document || !document->document) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->setEditOrigin(
      static_cast<codecore::EditOrigin>(origin)));
}

uint8_t cc_document_insert_spaces(const cc_document* document) {
  return document && document->document && document->document->insertSpaces() ? 1 : 0;
}

const char* cc_document_last_error(const cc_document* document) {
  return document && document->document ? document->document->lastError() : "invalid document";
}

cc_status cc_document_insert(cc_document* document, size_t offset, const char* utf8,
                             size_t length) {
  if (!document || !document->document || (!utf8 && length > 0)) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->insert(
      offset, std::string_view(utf8 ? utf8 : "", length)));
}

cc_status cc_document_erase(cc_document* document, size_t start, size_t end) {
  if (!document || !document->document) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->erase(start, end));
}

cc_status cc_document_replace(cc_document* document, size_t start, size_t end, const char* utf8,
                              size_t length) {
  if (!document || !document->document || (!utf8 && length > 0)) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->replace(
      start, end, std::string_view(utf8 ? utf8 : "", length)));
}

uint8_t cc_document_can_undo(const cc_document* document) {
  return document && document->document && document->document->canUndo() ? 1 : 0;
}

uint8_t cc_document_can_redo(const cc_document* document) {
  return document && document->document && document->document->canRedo() ? 1 : 0;
}

cc_status cc_document_undo(cc_document* document) {
  if (!document || !document->document) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->undo());
}

cc_status cc_document_redo(cc_document* document) {
  if (!document || !document->document) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->redo());
}

void cc_document_clear_history(cc_document* document) {
  if (document && document->document) {
    document->document->clearHistory();
  }
}

void cc_document_set_history_limit(cc_document* document, size_t limit) {
  if (document && document->document) {
    document->document->setHistoryLimit(limit);
  }
}

size_t cc_document_line_count(const cc_document* document) {
  return document && document->document ? document->document->lineCount() : 0;
}

cc_status cc_document_line_at(const cc_document* document, size_t line_index,
                              cc_line_info* out_info) {
  if (!document || !document->document || !out_info) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  codecore::LineInfo info;
  const cc_status status = mapStatus(document->document->lineAt(line_index, &info));
  if (status == CC_STATUS_OK) {
    out_info->start = static_cast<uint32_t>(info.start);
    out_info->content_end = static_cast<uint32_t>(info.contentEnd);
    out_info->end = static_cast<uint32_t>(info.end);
  }
  return status;
}

cc_status cc_document_offset_to_position(const cc_document* document, size_t offset,
                                         cc_position* out_position) {
  if (!document || !document->document || !out_position) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  codecore::Position position;
  const cc_status status = mapStatus(document->document->offsetToPosition(offset, &position));
  if (status == CC_STATUS_OK) {
    out_position->line = position.line;
    out_position->column = position.column;
  }
  return status;
}

cc_status cc_document_position_to_offset(const cc_document* document, uint32_t line,
                                         uint32_t column, size_t* out_offset) {
  if (!document || !document->document || !out_offset) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->positionToOffset(line, column, out_offset));
}

cc_selection cc_document_get_selection(const cc_document* document) {
  cc_selection selection{};
  if (document && document->document) {
    const codecore::Selection value = document->document->selection();
    selection.anchor = value.anchor;
    selection.focus = value.focus;
  }
  return selection;
}

cc_status cc_document_set_selection(cc_document* document, uint32_t anchor, uint32_t focus) {
  if (!document || !document->document) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->setSelection(anchor, focus));
}

cc_status cc_document_move_cursor(cc_document* document, uint32_t move,
                                  uint8_t extend_selection) {
  if (!document || !document->document || move > 9) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->moveCursor(static_cast<codecore::CursorMove>(move),
                                                  extend_selection != 0));
}

cc_status cc_document_match_bracket(const cc_document* document, size_t offset,
                                    size_t* out_match) {
  if (!document || !document->document || !out_match) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  size_t match = 0;
  const cc_status status = mapStatus(document->document->matchBracket(offset, &match));
  if (status == CC_STATUS_OK) {
    *out_match = match;
  }
  return status;
}

size_t cc_document_tokenize(const cc_document* document, size_t start, size_t end,
                            cc_token** out_tokens) {
  if (out_tokens) {
    *out_tokens = nullptr;
  }
  if (!document || !document->document || !out_tokens) {
    return 0;
  }
  const std::vector<codecore::Token> tokens = document->document->tokenize(start, end);
  if (tokens.empty()) {
    return 0;
  }
  auto* output = static_cast<cc_token*>(cc_alloc(tokens.size() * sizeof(cc_token)));
  if (!output) {
    return 0;
  }
  for (size_t index = 0; index < tokens.size(); ++index) {
    output[index].start = tokens[index].start;
    output[index].end = tokens[index].end;
    output[index].kind = static_cast<uint32_t>(tokens[index].kind);
  }
  *out_tokens = output;
  return tokens.size();
}

void cc_tokens_free(cc_token* tokens) { cc_free(tokens); }

size_t cc_document_complete(const cc_document* document, size_t offset,
                            cc_completion_item** out_items) {
  if (out_items) {
    *out_items = nullptr;
  }
  if (!document || !document->document || !out_items) {
    return 0;
  }
  const std::vector<codecore::CompletionItem> items = document->document->complete(offset);
  if (items.empty()) {
    return 0;
  }
  auto* output = static_cast<cc_completion_item*>(
      cc_alloc(items.size() * sizeof(cc_completion_item)));
  if (!output) {
    return 0;
  }
  size_t created = 0;
  for (; created < items.size(); ++created) {
    const codecore::CompletionItem& item = items[created];
    output[created].label = duplicateString(item.label);
    output[created].insert_text = duplicateString(item.insertText);
    output[created].detail = duplicateString(item.detail);
    output[created].kind = static_cast<uint32_t>(item.kind);
    output[created].score = item.score;
    if (!output[created].label || !output[created].insert_text || !output[created].detail) {
      break;
    }
  }
  if (created < items.size()) {
    for (size_t index = 0; index <= created && index < items.size(); ++index) {
      cc_free(const_cast<char*>(output[index].label));
      cc_free(const_cast<char*>(output[index].insert_text));
      cc_free(const_cast<char*>(output[index].detail));
    }
    cc_free(output);
    return 0;
  }
  *out_items = output;
  return items.size();
}

void cc_completions_free(cc_completion_item* items, size_t count) {
  if (!items) {
    return;
  }
  for (size_t index = 0; index < count; ++index) {
    cc_free(const_cast<char*>(items[index].label));
    cc_free(const_cast<char*>(items[index].insert_text));
    cc_free(const_cast<char*>(items[index].detail));
  }
  cc_free(items);
}

size_t cc_document_diagnose(const cc_document* document, cc_diagnostic** out_diagnostics) {
  if (out_diagnostics) {
    *out_diagnostics = nullptr;
  }
  if (!document || !document->document || !out_diagnostics) {
    return 0;
  }
  const std::vector<codecore::Diagnostic> diagnostics = document->document->diagnose();
  if (diagnostics.empty()) {
    return 0;
  }
  auto* output = static_cast<cc_diagnostic*>(cc_alloc(diagnostics.size() * sizeof(cc_diagnostic)));
  if (!output) {
    return 0;
  }
  size_t created = 0;
  for (; created < diagnostics.size(); ++created) {
    const codecore::Diagnostic& diagnostic = diagnostics[created];
    output[created].start = diagnostic.start;
    output[created].end = diagnostic.end;
    output[created].severity = diagnostic.severity;
    output[created].message = duplicateString(diagnostic.message);
    output[created].source = duplicateString(diagnostic.source);
    if (!output[created].message || !output[created].source) {
      break;
    }
  }
  if (created < diagnostics.size()) {
    for (size_t index = 0; index <= created && index < diagnostics.size(); ++index) {
      cc_free(const_cast<char*>(output[index].message));
      cc_free(const_cast<char*>(output[index].source));
    }
    cc_free(output);
    return 0;
  }
  *out_diagnostics = output;
  return diagnostics.size();
}

void cc_diagnostics_free(cc_diagnostic* diagnostics, size_t count) {
  if (!diagnostics) {
    return;
  }
  for (size_t index = 0; index < count; ++index) {
    cc_free(const_cast<char*>(diagnostics[index].message));
    cc_free(const_cast<char*>(diagnostics[index].source));
  }
  cc_free(diagnostics);
}

size_t cc_document_folding_ranges(const cc_document* document, cc_folding_range** out_ranges) {
  if (out_ranges) {
    *out_ranges = nullptr;
  }
  if (!document || !document->document || !out_ranges) {
    return 0;
  }
  const std::vector<codecore::FoldingRange> ranges = document->document->foldingRanges();
  if (ranges.empty()) {
    return 0;
  }
  auto* output =
      static_cast<cc_folding_range*>(cc_alloc(ranges.size() * sizeof(cc_folding_range)));
  if (!output) {
    return 0;
  }
  for (size_t index = 0; index < ranges.size(); ++index) {
    output[index].start_line = ranges[index].startLine;
    output[index].end_line = ranges[index].endLine;
    output[index].kind = ranges[index].kind;
  }
  *out_ranges = output;
  return ranges.size();
}

void cc_folding_ranges_free(cc_folding_range* ranges) { cc_free(ranges); }

size_t cc_document_find(const cc_document* document, const char* query, size_t query_length,
                        const cc_search_options* options, cc_range** out_ranges) {
  if (out_ranges) {
    *out_ranges = nullptr;
  }
  if (!document || !document->document || !query || !out_ranges) {
    return 0;
  }
  codecore::SearchOptions searchOptions;
  if (options) {
    searchOptions.caseSensitive = options->case_sensitive != 0;
    searchOptions.wholeWord = options->whole_word != 0;
  }
  const std::vector<codecore::Range> ranges =
      document->document->find(std::string_view(query, query_length), searchOptions);
  if (ranges.empty()) {
    return 0;
  }
  auto* output = static_cast<cc_range*>(cc_alloc(ranges.size() * sizeof(cc_range)));
  if (!output) {
    return 0;
  }
  for (size_t index = 0; index < ranges.size(); ++index) {
    output[index].start = ranges[index].start;
    output[index].end = ranges[index].end;
  }
  *out_ranges = output;
  return ranges.size();
}

void cc_search_ranges_free(cc_range* ranges) { cc_free(ranges); }

cc_status cc_document_set_proposed_text(cc_document* document, const char* utf8,
                                        size_t length, uint32_t origin) {
  if (!document || !document->document || (!utf8 && length > 0)) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->setProposedText(
      std::string_view(utf8 ? utf8 : "", length),
      static_cast<codecore::EditOrigin>(origin)));
}

uint8_t cc_document_has_proposal(const cc_document* document) {
  return document && document->document && document->document->hasProposal() ? 1 : 0;
}

char* cc_document_get_proposed_text(const cc_document* document, size_t* out_length) {
  if (out_length) {
    *out_length = 0;
  }
  if (!document || !document->document) {
    return nullptr;
  }
  const std::string text = document->document->proposedText();
  char* copy = duplicateString(text);
  if (copy && out_length) {
    *out_length = text.size();
  }
  return copy;
}

size_t cc_document_proposal_diff(const cc_document* document, cc_diff_line** out_lines) {
  if (out_lines) {
    *out_lines = nullptr;
  }
  if (!document || !document->document || !out_lines) {
    return 0;
  }
  const std::vector<codecore::DiffLine> lines = document->document->proposalDiff();
  *out_lines = allocateDiffLines(lines);
  return lines.size();
}

void cc_diff_lines_free(cc_diff_line* lines, size_t count) {
  if (!lines) {
    return;
  }
  for (size_t index = 0; index < count; ++index) {
    cc_free(const_cast<char*>(lines[index].text));
  }
  cc_free(lines);
}

cc_status cc_document_accept_proposal(cc_document* document) {
  if (!document || !document->document) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->acceptProposal());
}

cc_status cc_document_reject_proposal(cc_document* document) {
  if (!document || !document->document) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->rejectProposal());
}

cc_status cc_document_set_ghost_text(cc_document* document, size_t offset, const char* utf8,
                                     size_t length, uint32_t origin) {
  if (!document || !document->document || (!utf8 && length > 0)) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->setGhostText(
      offset, std::string_view(utf8 ? utf8 : "", length),
      static_cast<codecore::EditOrigin>(origin)));
}

char* cc_document_get_ghost_text(const cc_document* document, size_t* out_offset,
                                 size_t* out_length, uint32_t* out_origin) {
  if (out_offset) {
    *out_offset = 0;
  }
  if (out_length) {
    *out_length = 0;
  }
  if (out_origin) {
    *out_origin = CC_EDIT_ORIGIN_AI;
  }
  if (!document || !document->document) {
    return nullptr;
  }
  const codecore::GhostTextState ghost = document->document->ghostText();
  if (!ghost.active) {
    return nullptr;
  }
  char* copy = duplicateString(ghost.text);
  if (copy) {
    if (out_offset) {
      *out_offset = ghost.offset;
    }
    if (out_length) {
      *out_length = ghost.text.size();
    }
    if (out_origin) {
      *out_origin = static_cast<uint32_t>(ghost.origin);
    }
  }
  return copy;
}

cc_status cc_document_accept_ghost_text(cc_document* document, size_t max_code_points) {
  if (!document || !document->document) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->acceptGhostText(max_code_points));
}

cc_status cc_document_dismiss_ghost_text(cc_document* document) {
  if (!document || !document->document) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->dismissGhostText());
}

cc_snapshot* cc_document_create_snapshot(const cc_document* document) {
  if (!document || !document->document) {
    return nullptr;
  }
  auto snapshot = document->document->createSnapshot();
  if (!snapshot) {
    return nullptr;
  }
  auto* wrapper = new (std::nothrow) cc_snapshot();
  if (!wrapper) {
    return nullptr;
  }
  wrapper->snapshot = std::move(snapshot);
  return wrapper;
}

void cc_snapshot_destroy(cc_snapshot* snapshot) {
  delete snapshot;
}

uint64_t cc_snapshot_version(const cc_snapshot* snapshot) {
  return snapshot && snapshot->snapshot ? snapshot->snapshot->version() : 0;
}

size_t cc_snapshot_length(const cc_snapshot* snapshot) {
  return snapshot && snapshot->snapshot ? snapshot->snapshot->length() : 0;
}

int32_t cc_snapshot_language(const cc_snapshot* snapshot) {
  return snapshot && snapshot->snapshot ? snapshot->snapshot->languageId() : -1;
}

char* cc_snapshot_get_text(const cc_snapshot* snapshot, size_t* out_length) {
  if (out_length) {
    *out_length = 0;
  }
  if (!snapshot || !snapshot->snapshot) {
    return nullptr;
  }
  const std::string& text = snapshot->snapshot->text();
  char* copy = static_cast<char*>(cc_alloc(text.size() + 1));
  if (!copy) {
    return nullptr;
  }
  std::memcpy(copy, text.data(), text.size());
  copy[text.size()] = '\0';
  if (out_length) {
    *out_length = text.size();
  }
  return copy;
}

size_t cc_snapshot_tokenize(const cc_snapshot* snapshot, size_t start, size_t end,
                            cc_token** out_tokens) {
  if (out_tokens) {
    *out_tokens = nullptr;
  }
  if (!snapshot || !snapshot->snapshot || !out_tokens) {
    return 0;
  }
  const std::vector<codecore::Token> tokens = snapshot->snapshot->tokenize(start, end);
  *out_tokens = allocateTokens(tokens);
  return tokens.size();
}

size_t cc_snapshot_complete(const cc_snapshot* snapshot, size_t offset,
                            cc_completion_item** out_items) {
  if (out_items) {
    *out_items = nullptr;
  }
  if (!snapshot || !snapshot->snapshot || !out_items) {
    return 0;
  }
  const std::vector<codecore::CompletionItem> items = snapshot->snapshot->complete(offset);
  *out_items = allocateCompletions(items);
  return items.size();
}

size_t cc_snapshot_diagnose(const cc_snapshot* snapshot, cc_diagnostic** out_diagnostics) {
  if (out_diagnostics) {
    *out_diagnostics = nullptr;
  }
  if (!snapshot || !snapshot->snapshot || !out_diagnostics) {
    return 0;
  }
  const std::vector<codecore::Diagnostic> diagnostics = snapshot->snapshot->diagnose();
  *out_diagnostics = allocateDiagnostics(diagnostics);
  return diagnostics.size();
}

size_t cc_snapshot_folding_ranges(const cc_snapshot* snapshot,
                                  cc_folding_range** out_ranges) {
  if (out_ranges) {
    *out_ranges = nullptr;
  }
  if (!snapshot || !snapshot->snapshot || !out_ranges) {
    return 0;
  }
  const std::vector<codecore::FoldingRange> ranges = snapshot->snapshot->foldingRanges();
  *out_ranges = allocateFoldingRanges(ranges);
  return ranges.size();
}

size_t cc_snapshot_find(const cc_snapshot* snapshot, const char* query, size_t query_length,
                        const cc_search_options* options, cc_range** out_ranges) {
  if (out_ranges) {
    *out_ranges = nullptr;
  }
  if (!snapshot || !snapshot->snapshot || !query || !out_ranges) {
    return 0;
  }
  codecore::SearchOptions searchOptions;
  if (options) {
    searchOptions.caseSensitive = options->case_sensitive != 0;
    searchOptions.wholeWord = options->whole_word != 0;
  }
  const std::vector<codecore::Range> ranges =
      snapshot->snapshot->find(std::string_view(query, query_length), searchOptions);
  *out_ranges = allocateRanges(ranges);
  return ranges.size();
}

cc_status cc_document_restore_snapshot(cc_document* document, const cc_snapshot* snapshot) {
  if (!document || !document->document || !snapshot || !snapshot->snapshot) {
    return CC_STATUS_INVALID_ARGUMENT;
  }
  return mapStatus(document->document->restoreSnapshot(*snapshot->snapshot));
}

void cc_document_set_change_listener(cc_document* document, cc_change_listener listener,
                                     void* user_data) {
  if (!document || !document->document) {
    return;
  }
  document->listener = listener;
  document->user_data = user_data;
  document->document->setChangeListener(listener ? documentChangeThunk : nullptr, document);
}

}  // extern "C"
