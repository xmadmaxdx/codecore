/*
 * JNI bridge for the Kotlin SDK.
 *
 * All offsets crossing this boundary are UTF-8 byte offsets, matching the C ABI.
 * Strings are converted explicitly because JNI's NewStringUTF expects modified
 * UTF-8, not standard UTF-8.
 */

#include <jni.h>

#include <cstdint>
#include <mutex>
#include <new>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "codecore/codecore.h"

namespace {

constexpr char kFieldSeparator = '\x1F';

cc_engine* engineInstance() {
  static cc_engine* engine = cc_engine_create(nullptr);
  return engine;
}

cc_document* documentFrom(jlong handle) {
  return reinterpret_cast<cc_document*>(static_cast<intptr_t>(handle));
}

cc_snapshot* snapshotFrom(jlong handle) {
  return reinterpret_cast<cc_snapshot*>(static_cast<intptr_t>(handle));
}

cc_syntax_tree* syntaxTreeFrom(jlong handle) {
  return reinterpret_cast<cc_syntax_tree*>(static_cast<intptr_t>(handle));
}

cc_lsp_client* lspFrom(jlong handle) {
  return reinterpret_cast<cc_lsp_client*>(static_cast<intptr_t>(handle));
}

struct JniListener {
  JavaVM* vm = nullptr;
  jobject target = nullptr;
  jmethodID method = nullptr;
};

std::mutex listenerMutex;
std::unordered_map<cc_document*, JniListener*> listeners;

void releaseListener(JNIEnv* env, JniListener* listener) {
  if (!listener) {
    return;
  }
  if (env && listener->target) {
    env->DeleteGlobalRef(listener->target);
  }
  delete listener;
}

void jniChangeThunk(void* userData, const cc_change_event* event) {
  auto* listener = static_cast<JniListener*>(userData);
  if (!listener || !listener->vm || !listener->target || !listener->method) {
    return;
  }
  JNIEnv* env = nullptr;
  bool attached = false;
  if (listener->vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED) {
#if defined(__ANDROID__)
    if (listener->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) {
      return;
    }
#else
    if (listener->vm->AttachCurrentThread(reinterpret_cast<void**>(&env), nullptr) != JNI_OK) {
      return;
    }
#endif
    attached = true;
  }
  if (env) {
    env->CallVoidMethod(listener->target, listener->method, static_cast<jlong>(event->version),
                        static_cast<jint>(event->kind), static_cast<jint>(event->start),
                        static_cast<jint>(event->removed_length),
                        static_cast<jint>(event->inserted_length),
                        static_cast<jint>(event->origin));
    if (env->ExceptionCheck()) {
      env->ExceptionClear();
    }
  }
  if (attached) {
    listener->vm->DetachCurrentThread();
  }
}

void removeListener(JNIEnv* env, cc_document* document) {
  JniListener* previous = nullptr;
  {
    std::lock_guard<std::mutex> guard(listenerMutex);
    const auto found = listeners.find(document);
    if (found != listeners.end()) {
      previous = found->second;
      listeners.erase(found);
    }
  }
  if (previous) {
    cc_document_set_change_listener(document, nullptr, nullptr);
    releaseListener(env, previous);
  }
}

void appendUtf8(std::string& out, uint32_t codePoint) {
  if (codePoint <= 0x7F) {
    out.push_back(static_cast<char>(codePoint));
  } else if (codePoint <= 0x7FF) {
    out.push_back(static_cast<char>(0xC0 | (codePoint >> 6)));
    out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
  } else if (codePoint <= 0xFFFF) {
    out.push_back(static_cast<char>(0xE0 | (codePoint >> 12)));
    out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
  } else {
    out.push_back(static_cast<char>(0xF0 | (codePoint >> 18)));
    out.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codePoint & 0x3F)));
  }
}

std::string javaStringToUtf8(JNIEnv* env, jstring value) {
  if (!value) {
    return {};
  }
  const jsize length = env->GetStringLength(value);
  const jchar* characters = env->GetStringChars(value, nullptr);
  if (!characters) {
    return {};
  }
  std::string result;
  result.reserve(static_cast<size_t>(length) * 2);
  for (jsize index = 0; index < length; ++index) {
    uint32_t codePoint = characters[index];
    if (codePoint >= 0xD800 && codePoint <= 0xDBFF && index + 1 < length) {
      const uint32_t low = characters[index + 1];
      if (low >= 0xDC00 && low <= 0xDFFF) {
        codePoint = 0x10000 + ((codePoint - 0xD800) << 10) + (low - 0xDC00);
        ++index;
      }
    }
    appendUtf8(result, codePoint);
  }
  env->ReleaseStringChars(value, characters);
  return result;
}

std::u16string utf8ToUtf16(std::string_view input) {
  std::u16string result;
  result.reserve(input.size());
  size_t index = 0;
  while (index < input.size()) {
    const unsigned char lead = static_cast<unsigned char>(input[index]);
    uint32_t codePoint = 0xFFFD;
    size_t width = 1;
    if (lead < 0x80) {
      codePoint = lead;
    } else if ((lead & 0xE0u) == 0xC0u && index + 1 < input.size()) {
      codePoint = (lead & 0x1Fu) << 6;
      codePoint |= static_cast<unsigned char>(input[index + 1]) & 0x3Fu;
      width = 2;
    } else if ((lead & 0xF0u) == 0xE0u && index + 2 < input.size()) {
      codePoint = (lead & 0x0Fu) << 12;
      codePoint |= (static_cast<unsigned char>(input[index + 1]) & 0x3Fu) << 6;
      codePoint |= static_cast<unsigned char>(input[index + 2]) & 0x3Fu;
      width = 3;
    } else if ((lead & 0xF8u) == 0xF0u && index + 3 < input.size()) {
      codePoint = (lead & 0x07u) << 18;
      codePoint |= (static_cast<unsigned char>(input[index + 1]) & 0x3Fu) << 12;
      codePoint |= (static_cast<unsigned char>(input[index + 2]) & 0x3Fu) << 6;
      codePoint |= static_cast<unsigned char>(input[index + 3]) & 0x3Fu;
      width = 4;
    }
    if (codePoint > 0x10FFFF) {
      codePoint = 0xFFFD;
    }
    if (codePoint <= 0xFFFF) {
      result.push_back(static_cast<char16_t>(codePoint));
    } else {
      codePoint -= 0x10000;
      result.push_back(static_cast<char16_t>(0xD800 + (codePoint >> 10)));
      result.push_back(static_cast<char16_t>(0xDC00 + (codePoint & 0x3FF)));
    }
    index += width;
  }
  return result;
}

jstring toJavaString(JNIEnv* env, const std::string& value) {
  const std::u16string utf16 = utf8ToUtf16(value);
  return env->NewString(reinterpret_cast<const jchar*>(utf16.data()),
                        static_cast<jsize>(utf16.size()));
}

std::string completionLine(const cc_completion_item& item) {
  std::string line = item.label ? item.label : "";
  line.push_back(kFieldSeparator);
  line += item.insert_text ? item.insert_text : "";
  line.push_back(kFieldSeparator);
  line += item.detail ? item.detail : "";
  line.push_back(kFieldSeparator);
  line += std::to_string(item.kind);
  line.push_back(kFieldSeparator);
  line += std::to_string(item.score);
  return line;
}

std::string diffLine(const cc_diff_line& line) {
  std::string value = std::to_string(line.kind);
  value.push_back(kFieldSeparator);
  value += std::to_string(line.old_line);
  value.push_back(kFieldSeparator);
  value += std::to_string(line.new_line);
  value.push_back(kFieldSeparator);
  value += line.text ? line.text : "";
  return value;
}

std::string diagnosticLine(const cc_diagnostic& diagnostic) {
  std::string line = std::to_string(diagnostic.start);
  line.push_back(kFieldSeparator);
  line += std::to_string(diagnostic.end);
  line.push_back(kFieldSeparator);
  line += std::to_string(diagnostic.severity);
  line.push_back(kFieldSeparator);
  line += diagnostic.message ? diagnostic.message : "";
  line.push_back(kFieldSeparator);
  line += diagnostic.source ? diagnostic.source : "";
  return line;
}

std::string syntaxHighlightLine(const cc_syntax_highlight& highlight) {
  std::string line = std::to_string(highlight.start);
  line.push_back(kFieldSeparator);
  line += std::to_string(highlight.end);
  line.push_back(kFieldSeparator);
  line += std::to_string(highlight.kind);
  line.push_back(kFieldSeparator);
  line += highlight.capture ? highlight.capture : "";
  return line;
}

std::string syntaxErrorLine(const cc_syntax_error& error) {
  std::string line = std::to_string(error.start);
  line.push_back(kFieldSeparator);
  line += std::to_string(error.end);
  line.push_back(kFieldSeparator);
  line += std::to_string(error.start_row);
  line.push_back(kFieldSeparator);
  line += std::to_string(error.start_column);
  line.push_back(kFieldSeparator);
  line += std::to_string(error.end_row);
  line.push_back(kFieldSeparator);
  line += std::to_string(error.end_column);
  line.push_back(kFieldSeparator);
  line += error.missing ? "1" : "0";
  line.push_back(kFieldSeparator);
  line += error.node_type ? error.node_type : "";
  return line;
}

jobjectArray stringArray(JNIEnv* env, const std::vector<std::string>& values) {
  jclass stringClass = env->FindClass("java/lang/String");
  if (!stringClass) {
    return nullptr;
  }
  jobjectArray array = env->NewObjectArray(static_cast<jsize>(values.size()), stringClass,
                                           nullptr);
  if (!array) {
    return nullptr;
  }
  for (size_t index = 0; index < values.size(); ++index) {
    jstring value = toJavaString(env, values[index]);
    if (!value) {
      return array;
    }
    env->SetObjectArrayElement(array, static_cast<jsize>(index), value);
    env->DeleteLocalRef(value);
  }
  return array;
}

jintArray intArray(JNIEnv* env, const std::vector<jint>& values) {
  jintArray array = env->NewIntArray(static_cast<jsize>(values.size()));
  if (!array) {
    return nullptr;
  }
  env->SetIntArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
  return array;
}

jlongArray longArray(JNIEnv* env, const std::vector<jlong>& values) {
  jlongArray array = env->NewLongArray(static_cast<jsize>(values.size()));
  if (!array) {
    return nullptr;
  }
  env->SetLongArrayRegion(array, 0, static_cast<jsize>(values.size()), values.data());
  return array;
}

}  // namespace

extern "C" {

JNIEXPORT jstring JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeVersion(JNIEnv* env, jclass) {
  return toJavaString(env, cc_version_string());
}

JNIEXPORT jlong JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeCreate(JNIEnv* env, jclass,
                                                           jstring language,
                                                           jint tabSize,
                                                           jboolean insertSpaces) {
  const std::string languageId = javaStringToUtf8(env, language);
  const int32_t id = cc_engine_find_language(engineInstance(), languageId.c_str());
  if (id < 0) {
    return 0;
  }
  cc_document_options options{};
  options.struct_size = sizeof(cc_document_options);
  options.language_id = id;
  options.tab_size = tabSize > 0 ? static_cast<uint32_t>(tabSize) : 4;
  options.insert_spaces = insertSpaces ? 1 : 0;
  return static_cast<jlong>(
      reinterpret_cast<intptr_t>(cc_document_create(engineInstance(), &options)));
}

JNIEXPORT void JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeDestroy(JNIEnv* env, jclass, jlong handle) {
  cc_document* document = documentFrom(handle);
  if (document) {
    removeListener(env, document);
  }
  cc_document_destroy(document);
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSetText(JNIEnv* env, jclass, jlong handle,
                                                            jstring text) {
  cc_document* document = documentFrom(handle);
  const std::string value = javaStringToUtf8(env, text);
  return static_cast<jint>(cc_document_set_text(document, value.data(), value.size()));
}

JNIEXPORT jstring JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeGetText(JNIEnv* env, jclass,
                                                            jlong handle) {
  cc_document* document = documentFrom(handle);
  size_t length = 0;
  char* text = cc_document_get_text(document, &length);
  const std::string value(text ? text : "", length);
  cc_string_free(text);
  return toJavaString(env, value);
}

JNIEXPORT jlong JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLength(JNIEnv*, jclass, jlong handle) {
  return static_cast<jlong>(cc_document_length(documentFrom(handle)));
}

JNIEXPORT jlong JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeDocumentVersion(JNIEnv*, jclass,
                                                                   jlong handle) {
  return static_cast<jlong>(cc_document_version(documentFrom(handle)));
}

JNIEXPORT jlong JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLineCount(JNIEnv*, jclass, jlong handle) {
  return static_cast<jlong>(cc_document_line_count(documentFrom(handle)));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLanguageId(JNIEnv*, jclass, jlong handle) {
  return static_cast<jint>(cc_document_language(documentFrom(handle)));
}

JNIEXPORT jstring JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLastError(JNIEnv* env, jclass,
                                                              jlong handle) {
  return toJavaString(env, cc_document_last_error(documentFrom(handle)));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeInsert(JNIEnv* env, jclass, jlong handle,
                                                           jint offset, jstring text) {
  cc_document* document = documentFrom(handle);
  const std::string value = javaStringToUtf8(env, text);
  return static_cast<jint>(cc_document_insert(document, static_cast<size_t>(offset),
                                              value.data(), value.size()));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeErase(JNIEnv*, jclass, jlong handle,
                                                          jint start, jint end) {
  return static_cast<jint>(cc_document_erase(documentFrom(handle), static_cast<size_t>(start),
                                             static_cast<size_t>(end)));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeReplace(JNIEnv* env, jclass, jlong handle,
                                                            jint start, jint end,
                                                            jstring text) {
  cc_document* document = documentFrom(handle);
  const std::string value = javaStringToUtf8(env, text);
  return static_cast<jint>(cc_document_replace(document, static_cast<size_t>(start),
                                               static_cast<size_t>(end), value.data(),
                                               value.size()));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeUndo(JNIEnv*, jclass, jlong handle) {
  return static_cast<jint>(cc_document_undo(documentFrom(handle)));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeRedo(JNIEnv*, jclass, jlong handle) {
  return static_cast<jint>(cc_document_redo(documentFrom(handle)));
}

JNIEXPORT jboolean JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeCanUndo(JNIEnv*, jclass, jlong handle) {
  return cc_document_can_undo(documentFrom(handle)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jboolean JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeCanRedo(JNIEnv*, jclass, jlong handle) {
  return cc_document_can_redo(documentFrom(handle)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeClearHistory(JNIEnv*, jclass,
                                                                 jlong handle) {
  cc_document_clear_history(documentFrom(handle));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSetReadOnly(JNIEnv*, jclass, jlong handle,
                                                                jboolean readOnly) {
  return static_cast<jint>(
      cc_document_set_read_only(documentFrom(handle), readOnly ? 1 : 0));
}

JNIEXPORT jboolean JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeIsReadOnly(JNIEnv*, jclass, jlong handle) {
  return cc_document_is_read_only(documentFrom(handle)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jintArray JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeTokenize(JNIEnv* env, jclass, jlong handle,
                                                             jint start, jint end) {
  cc_token* tokens = nullptr;
  const size_t count = cc_document_tokenize(documentFrom(handle), static_cast<size_t>(start),
                                             static_cast<size_t>(end), &tokens);
  std::vector<jint> packed;
  packed.reserve(count * 3);
  for (size_t index = 0; index < count; ++index) {
    packed.push_back(static_cast<jint>(tokens[index].start));
    packed.push_back(static_cast<jint>(tokens[index].end));
    packed.push_back(static_cast<jint>(tokens[index].kind));
  }
  cc_tokens_free(tokens);
  return intArray(env, packed);
}

JNIEXPORT jboolean JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSyntaxLanguageSupported(JNIEnv* env, jclass,
                                                                         jstring language) {
  const std::string value = javaStringToUtf8(env, language);
  return cc_syntax_language_supported(value.c_str()) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jlong JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSyntaxTree(JNIEnv*, jclass, jlong handle) {
  return static_cast<jlong>(
      reinterpret_cast<intptr_t>(cc_document_syntax_tree(documentFrom(handle))));
}

JNIEXPORT void JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSyntaxTreeDestroy(JNIEnv*, jclass,
                                                                     jlong tree) {
  cc_syntax_tree_destroy(syntaxTreeFrom(tree));
}

JNIEXPORT jstring JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSyntaxLanguage(JNIEnv* env, jclass,
                                                                  jlong tree) {
  return toJavaString(env, cc_syntax_tree_language(syntaxTreeFrom(tree)));
}

JNIEXPORT jstring JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSyntaxRootType(JNIEnv* env, jclass,
                                                                  jlong tree) {
  return toJavaString(env, cc_syntax_tree_root_type(syntaxTreeFrom(tree)));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSyntaxNodeCount(JNIEnv*, jclass,
                                                                   jlong tree) {
  return static_cast<jint>(cc_syntax_tree_node_count(syntaxTreeFrom(tree)));
}

JNIEXPORT jboolean JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSyntaxHasErrors(JNIEnv*, jclass,
                                                                   jlong tree) {
  return cc_syntax_tree_has_errors(syntaxTreeFrom(tree)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jobjectArray JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSyntaxHighlights(JNIEnv* env, jclass,
                                                                    jlong tree) {
  cc_syntax_tree* syntax = syntaxTreeFrom(tree);
  const size_t count = cc_syntax_tree_highlight_count(syntax);
  std::vector<std::string> lines;
  lines.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    cc_syntax_highlight highlight{};
    if (!cc_syntax_tree_highlight_at(syntax, index, &highlight)) {
      continue;
    }
    lines.push_back(syntaxHighlightLine(highlight));
  }
  return stringArray(env, lines);
}

JNIEXPORT jobjectArray JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSyntaxErrors(JNIEnv* env, jclass,
                                                                jlong tree) {
  cc_syntax_tree* syntax = syntaxTreeFrom(tree);
  const size_t count = cc_syntax_tree_error_count(syntax);
  std::vector<std::string> lines;
  lines.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    cc_syntax_error error{};
    if (!cc_syntax_tree_error_at(syntax, index, &error)) {
      continue;
    }
    lines.push_back(syntaxErrorLine(error));
  }
  return stringArray(env, lines);
}

JNIEXPORT jobjectArray JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeComplete(JNIEnv* env, jclass, jlong handle,
                                                             jint offset) {
  cc_completion_item* items = nullptr;
  const size_t count = cc_document_complete(documentFrom(handle), static_cast<size_t>(offset),
                                             &items);
  std::vector<std::string> lines;
  lines.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    lines.push_back(completionLine(items[index]));
  }
  cc_completions_free(items, count);
  return stringArray(env, lines);
}

JNIEXPORT jobjectArray JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeDiagnose(JNIEnv* env, jclass, jlong handle) {
  cc_diagnostic* diagnostics = nullptr;
  const size_t count = cc_document_diagnose(documentFrom(handle), &diagnostics);
  std::vector<std::string> lines;
  lines.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    lines.push_back(diagnosticLine(diagnostics[index]));
  }
  cc_diagnostics_free(diagnostics, count);
  return stringArray(env, lines);
}

JNIEXPORT jlongArray JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeFind(JNIEnv* env, jclass, jlong handle,
                                                          jstring query, jboolean caseSensitive,
                                                          jboolean wholeWord, jboolean regex) {
  const std::string value = javaStringToUtf8(env, query);
  cc_search_options options{};
  options.struct_size = sizeof(cc_search_options);
  options.case_sensitive = caseSensitive ? 1 : 0;
  options.whole_word = wholeWord ? 1 : 0;
  options.regex = regex ? 1 : 0;
  cc_range* ranges = nullptr;
  size_t count = 0;
  const cc_status status = cc_document_search(documentFrom(handle), value.data(), value.size(),
                                               &options, &ranges, &count);
  std::vector<jlong> packed;
  packed.reserve(1 + count * 2);
  packed.push_back(static_cast<jlong>(status));
  if (status == CC_STATUS_OK) {
    for (size_t index = 0; index < count; ++index) {
      packed.push_back(static_cast<jlong>(ranges[index].start));
      packed.push_back(static_cast<jlong>(ranges[index].end));
    }
  }
  cc_search_ranges_free(ranges);
  return longArray(env, packed);
}

JNIEXPORT jintArray JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeFoldingRanges(JNIEnv* env, jclass,
                                                                  jlong handle) {
  cc_folding_range* ranges = nullptr;
  const size_t count = cc_document_folding_ranges(documentFrom(handle), &ranges);
  std::vector<jint> packed;
  packed.reserve(count * 3);
  for (size_t index = 0; index < count; ++index) {
    packed.push_back(static_cast<jint>(ranges[index].start_line));
    packed.push_back(static_cast<jint>(ranges[index].end_line));
    packed.push_back(static_cast<jint>(ranges[index].kind));
  }
  cc_folding_ranges_free(ranges);
  return intArray(env, packed);
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeMatchBracket(JNIEnv*, jclass, jlong handle,
                                                                 jint offset) {
  size_t match = 0;
  const cc_status status =
      cc_document_match_bracket(documentFrom(handle), static_cast<size_t>(offset), &match);
  return status == CC_STATUS_OK ? static_cast<jint>(match) : -1;
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSetEditable(JNIEnv*, jclass, jlong handle,
                                                                jboolean editable) {
  return static_cast<jint>(
      cc_document_set_editable(documentFrom(handle), editable ? 1 : 0));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeEditOrigin(JNIEnv*, jclass, jlong handle) {
  return static_cast<jint>(cc_document_edit_origin(documentFrom(handle)));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSetEditOrigin(JNIEnv*, jclass, jlong handle,
                                                                  jint origin) {
  return static_cast<jint>(
      cc_document_set_edit_origin(documentFrom(handle), static_cast<uint32_t>(origin)));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSetProposal(JNIEnv* env, jclass,
                                                                jlong handle, jstring text,
                                                                jint origin) {
  const std::string value = javaStringToUtf8(env, text);
  return static_cast<jint>(cc_document_set_proposed_text(
      documentFrom(handle), value.data(), value.size(), static_cast<uint32_t>(origin)));
}

JNIEXPORT jboolean JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeHasProposal(JNIEnv*, jclass, jlong handle) {
  return cc_document_has_proposal(documentFrom(handle)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeProposal(JNIEnv* env, jclass,
                                                             jlong handle) {
  size_t length = 0;
  char* text = cc_document_get_proposed_text(documentFrom(handle), &length);
  if (!text) {
    return nullptr;
  }
  const std::string value(text, length);
  cc_string_free(text);
  return toJavaString(env, value);
}

JNIEXPORT jobjectArray JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeProposalDiff(JNIEnv* env, jclass,
                                                                 jlong handle) {
  cc_diff_line* lines = nullptr;
  const size_t count = cc_document_proposal_diff(documentFrom(handle), &lines);
  std::vector<std::string> values;
  values.reserve(count);
  for (size_t index = 0; index < count; ++index) {
    values.push_back(diffLine(lines[index]));
  }
  cc_diff_lines_free(lines, count);
  return stringArray(env, values);
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeAcceptProposal(JNIEnv*, jclass,
                                                                   jlong handle) {
  return static_cast<jint>(cc_document_accept_proposal(documentFrom(handle)));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeRejectProposal(JNIEnv*, jclass,
                                                                   jlong handle) {
  return static_cast<jint>(cc_document_reject_proposal(documentFrom(handle)));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSetGhostText(JNIEnv* env, jclass,
                                                                 jlong handle, jint offset,
                                                                 jstring text, jint origin) {
  const std::string value = javaStringToUtf8(env, text);
  return static_cast<jint>(cc_document_set_ghost_text(
      documentFrom(handle), static_cast<size_t>(offset), value.data(), value.size(),
      static_cast<uint32_t>(origin)));
}

JNIEXPORT jstring JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeGhostText(JNIEnv* env, jclass,
                                                              jlong handle) {
  size_t offset = 0;
  size_t length = 0;
  uint32_t origin = CC_EDIT_ORIGIN_AI;
  char* text =
      cc_document_get_ghost_text(documentFrom(handle), &offset, &length, &origin);
  if (!text) {
    return nullptr;
  }
  std::string value = std::to_string(offset);
  value.push_back(kFieldSeparator);
  value += std::to_string(origin);
  value.push_back(kFieldSeparator);
  value.append(text, length);
  cc_string_free(text);
  return toJavaString(env, value);
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeAcceptGhostText(JNIEnv*, jclass,
                                                                    jlong handle,
                                                                    jint maxCodePoints) {
  return static_cast<jint>(cc_document_accept_ghost_text(
      documentFrom(handle), static_cast<size_t>(maxCodePoints)));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeDismissGhostText(JNIEnv*, jclass,
                                                                     jlong handle) {
  return static_cast<jint>(cc_document_dismiss_ghost_text(documentFrom(handle)));
}

JNIEXPORT jlong JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeCreateSnapshot(JNIEnv*, jclass,
                                                                   jlong handle) {
  return static_cast<jlong>(
      reinterpret_cast<intptr_t>(cc_document_create_snapshot(documentFrom(handle))));
}

JNIEXPORT void JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSnapshotDestroy(JNIEnv*, jclass,
                                                                    jlong handle) {
  cc_snapshot_destroy(snapshotFrom(handle));
}

JNIEXPORT jlong JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSnapshotVersion(JNIEnv*, jclass,
                                                                    jlong handle) {
  return static_cast<jlong>(cc_snapshot_version(snapshotFrom(handle)));
}

JNIEXPORT jlong JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSnapshotLength(JNIEnv*, jclass,
                                                                   jlong handle) {
  return static_cast<jlong>(cc_snapshot_length(snapshotFrom(handle)));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSnapshotLanguage(JNIEnv*, jclass,
                                                                     jlong handle) {
  return static_cast<jint>(cc_snapshot_language(snapshotFrom(handle)));
}

JNIEXPORT jstring JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSnapshotText(JNIEnv* env, jclass,
                                                                 jlong handle) {
  size_t length = 0;
  char* text = cc_snapshot_get_text(snapshotFrom(handle), &length);
  const std::string value(text ? text : "", length);
  cc_string_free(text);
  return toJavaString(env, value);
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeRestoreSnapshot(JNIEnv*, jclass,
                                                                    jlong document,
                                                                    jlong snapshot) {
  return static_cast<jint>(cc_document_restore_snapshot(
      documentFrom(document), snapshotFrom(snapshot)));
}

JNIEXPORT jlong JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeGetSelection(JNIEnv*, jclass,
                                                                 jlong handle) {
  const cc_selection selection = cc_document_get_selection(documentFrom(handle));
  return static_cast<jlong>((static_cast<uint64_t>(selection.anchor) << 32) |
                            static_cast<uint64_t>(selection.focus));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSetSelection(JNIEnv*, jclass, jlong handle,
                                                                 jint anchor, jint focus) {
  return static_cast<jint>(cc_document_set_selection(
      documentFrom(handle), static_cast<uint32_t>(anchor), static_cast<uint32_t>(focus)));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeMoveCursor(JNIEnv*, jclass, jlong handle,
                                                               jint move, jboolean extend) {
  return static_cast<jint>(cc_document_move_cursor(
      documentFrom(handle), static_cast<uint32_t>(move), extend ? 1 : 0));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeCursorCount(JNIEnv*, jclass, jlong handle) {
  return static_cast<jint>(cc_document_cursor_count(documentFrom(handle)));
}

JNIEXPORT jlong JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeCursorAt(JNIEnv*, jclass, jlong handle,
                                                             jint index) {
  cc_selection selection{};
  if (cc_document_cursor_at(documentFrom(handle), static_cast<size_t>(index), &selection) !=
      CC_STATUS_OK) {
    return -1;
  }
  return static_cast<jlong>((static_cast<uint64_t>(selection.anchor) << 32) |
                            static_cast<uint64_t>(selection.focus));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeAddCursor(JNIEnv*, jclass, jlong handle,
                                                              jint anchor, jint focus) {
  return static_cast<jint>(cc_document_add_cursor(
      documentFrom(handle), static_cast<uint32_t>(anchor), static_cast<uint32_t>(focus)));
}

JNIEXPORT void JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeClearCursors(JNIEnv*, jclass, jlong handle) {
  cc_document_clear_cursors(documentFrom(handle));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeMoveCursors(JNIEnv*, jclass, jlong handle,
                                                                jint move, jboolean extend) {
  return static_cast<jint>(cc_document_move_cursors(
      documentFrom(handle), static_cast<uint32_t>(move), extend ? 1 : 0));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeMultiInsert(JNIEnv* env, jclass,
                                                                jlong handle, jstring text) {
  const std::string value = javaStringToUtf8(env, text);
  return static_cast<jint>(
      cc_document_multi_insert(documentFrom(handle), value.data(), value.size()));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeMultiErase(JNIEnv*, jclass, jlong handle,
                                                               jboolean backward) {
  return static_cast<jint>(cc_document_multi_erase(documentFrom(handle), backward ? 1 : 0));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSetLanguage(JNIEnv*, jclass, jlong handle,
                                                                jint languageId) {
  return static_cast<jint>(cc_document_set_language(documentFrom(handle), languageId));
}

JNIEXPORT jlong JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspCreate(JNIEnv*, jclass) {
  return static_cast<jlong>(reinterpret_cast<intptr_t>(cc_lsp_create()));
}

JNIEXPORT void JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspDestroy(JNIEnv*, jclass, jlong handle) {
  cc_lsp_destroy(lspFrom(handle));
}

JNIEXPORT void JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspInitialize(JNIEnv* env, jclass,
                                                                 jlong handle, jstring rootUri) {
  const std::string root = javaStringToUtf8(env, rootUri);
  cc_lsp_initialize(lspFrom(handle), root.c_str());
}

JNIEXPORT void JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspFeed(JNIEnv* env, jclass, jlong handle,
                                                            jstring data) {
  const std::string chunk = javaStringToUtf8(env, data);
  cc_lsp_feed(lspFrom(handle), chunk.data(), chunk.size());
}

JNIEXPORT jstring JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspTakeOutgoing(JNIEnv* env, jclass,
                                                                    jlong handle) {
  size_t length = 0;
  char* frame = cc_lsp_take_outgoing(lspFrom(handle), &length);
  if (!frame) {
    return nullptr;
  }
  jstring result = toJavaString(env, std::string(frame, length));
  cc_string_free(frame);
  return result;
}

JNIEXPORT jobjectArray JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspTakeResult(JNIEnv* env, jclass,
                                                                  jlong handle) {
  int64_t id = 0;
  char* method = nullptr;
  char* payload = nullptr;
  uint8_t isError = 0;
  if (!cc_lsp_take_result(lspFrom(handle), &id, &method, &payload, &isError)) {
    return nullptr;
  }
  std::vector<std::string> fields;
  fields.push_back(std::to_string(id));
  fields.push_back(method ? method : "");
  fields.push_back(payload ? payload : "");
  fields.push_back(isError ? "1" : "0");
  cc_string_free(method);
  cc_string_free(payload);
  return stringArray(env, fields);
}

JNIEXPORT jobjectArray JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspTakeDiagnostics(JNIEnv* env, jclass,
                                                                       jlong handle) {
  char* uri = nullptr;
  char* payload = nullptr;
  if (!cc_lsp_take_diagnostics(lspFrom(handle), &uri, &payload)) {
    return nullptr;
  }
  std::vector<std::string> fields;
  fields.push_back(uri ? uri : "");
  fields.push_back(payload ? payload : "");
  cc_string_free(uri);
  cc_string_free(payload);
  return stringArray(env, fields);
}

JNIEXPORT jboolean JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspIsInitialized(JNIEnv*, jclass,
                                                                     jlong handle) {
  return cc_lsp_is_initialized(lspFrom(handle)) ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT jstring JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspServerCapabilities(JNIEnv* env, jclass,
                                                                          jlong handle) {
  return toJavaString(env, cc_lsp_server_capabilities(lspFrom(handle)));
}

JNIEXPORT jstring JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspLastError(JNIEnv* env, jclass,
                                                                 jlong handle) {
  return toJavaString(env, cc_lsp_last_error(lspFrom(handle)));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspDidOpen(
    JNIEnv* env, jclass, jlong handle, jstring uri, jstring languageId, jint version,
    jstring text) {
  const std::string uriValue = javaStringToUtf8(env, uri);
  const std::string language = javaStringToUtf8(env, languageId);
  const std::string source = javaStringToUtf8(env, text);
  return static_cast<jint>(cc_lsp_did_open(lspFrom(handle), uriValue.c_str(),
                                           language.c_str(), version, source.data(),
                                           source.size()));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspDidChange(JNIEnv* env, jclass,
                                                                 jlong handle, jstring uri,
                                                                 jint version,
                                                                 jstring text) {
  const std::string uriValue = javaStringToUtf8(env, uri);
  const std::string source = javaStringToUtf8(env, text);
  return static_cast<jint>(cc_lsp_did_change(lspFrom(handle), uriValue.c_str(), version,
                                             source.data(), source.size()));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspDidClose(JNIEnv* env, jclass,
                                                                jlong handle, jstring uri) {
  const std::string uriValue = javaStringToUtf8(env, uri);
  return static_cast<jint>(cc_lsp_did_close(lspFrom(handle), uriValue.c_str()));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspDidSave(
    JNIEnv* env, jclass, jlong handle, jstring uri, jstring text, jboolean includeText) {
  const std::string uriValue = javaStringToUtf8(env, uri);
  const std::string source = javaStringToUtf8(env, text);
  return static_cast<jint>(cc_lsp_did_save(lspFrom(handle), uriValue.c_str(), source.data(),
                                           source.size(), includeText ? 1 : 0));
}

JNIEXPORT jlong JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspRequest(JNIEnv* env, jclass,
                                                               jlong handle, jstring method,
                                                               jstring params) {
  const std::string methodValue = javaStringToUtf8(env, method);
  const std::string paramsValue = javaStringToUtf8(env, params);
  return static_cast<jlong>(cc_lsp_request(lspFrom(handle), methodValue.c_str(),
                                           paramsValue.data(), paramsValue.size()));
}

JNIEXPORT jint JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspNotify(JNIEnv* env, jclass,
                                                              jlong handle, jstring method,
                                                              jstring params) {
  const std::string methodValue = javaStringToUtf8(env, method);
  const std::string paramsValue = javaStringToUtf8(env, params);
  return static_cast<jint>(cc_lsp_notify(lspFrom(handle), methodValue.c_str(),
                                         paramsValue.data(), paramsValue.size()));
}

JNIEXPORT void JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspCancel(JNIEnv*, jclass, jlong handle,
                                                              jlong id) {
  cc_lsp_cancel(lspFrom(handle), id);
}

JNIEXPORT void JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspShutdown(JNIEnv*, jclass, jlong handle) {
  cc_lsp_shutdown(lspFrom(handle));
}

JNIEXPORT void JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeLspExit(JNIEnv*, jclass, jlong handle) {
  cc_lsp_exit(lspFrom(handle));
}

JNIEXPORT void JNICALL
Java_dev_codecore_nativebridge_NativeCodeCore_nativeSetChangeListener(JNIEnv* env, jclass,
                                                                      jlong handle,
                                                                      jobject listener) {
  cc_document* document = documentFrom(handle);
  if (!document) {
    return;
  }

  JniListener* replacement = nullptr;
  if (listener) {
    jclass listenerClass = env->GetObjectClass(listener);
    if (!listenerClass) {
      return;
    }
    const jmethodID method = env->GetMethodID(listenerClass, "onChange", "(JIIIII)V");
    env->DeleteLocalRef(listenerClass);
    if (!method) {
      env->ExceptionClear();
      return;
    }
    replacement = new (std::nothrow) JniListener();
    if (!replacement) {
      return;
    }
    env->GetJavaVM(&replacement->vm);
    replacement->target = env->NewGlobalRef(listener);
    replacement->method = method;
    if (!replacement->target) {
      delete replacement;
      return;
    }
  }

  JniListener* previous = nullptr;
  {
    std::lock_guard<std::mutex> guard(listenerMutex);
    const auto found = listeners.find(document);
    if (found != listeners.end()) {
      previous = found->second;
      listeners.erase(found);
    }
    if (replacement) {
      listeners.emplace(document, replacement);
    }
  }

  cc_document_set_change_listener(document, replacement ? jniChangeThunk : nullptr, replacement);
  releaseListener(env, previous);
}

}  // extern "C"
