# Android NDK build for the Kotlin SDK.
#
# Builds the shared engine and JNI bridge into libcodecore_jni.so. JNI headers
# are supplied by the NDK sysroot, so no find_package(JNI) is used here.

cmake_minimum_required(VERSION 3.22.1)

project(codecore_android LANGUAGES C CXX)

set(CODECORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
find_package(Threads REQUIRED)

add_library(codecore_jni SHARED
  "${CODECORE_ROOT}/core/text_buffer/text_buffer.cpp"
  "${CODECORE_ROOT}/core/language/language_registry.cpp"
  "${CODECORE_ROOT}/core/language/lexer.cpp"
  "${CODECORE_ROOT}/core/language/token_cache.cpp"
  "${CODECORE_ROOT}/core/document/line_index.cpp"
  "${CODECORE_ROOT}/core/document/document.cpp"
  "${CODECORE_ROOT}/core/syntax/syntax.cpp"
  "${CODECORE_ROOT}/core/exec/execution.cpp"
  "${CODECORE_ROOT}/core/term/terminal.cpp"
  "${CODECORE_ROOT}/core/runtime/runtime.cpp"
  "${CODECORE_ROOT}/core/analysis/analysis.cpp"
  "${CODECORE_ROOT}/core/analysis/background.cpp"
  "${CODECORE_ROOT}/core/viewport/viewport.cpp"
  "${CODECORE_ROOT}/core/diff/diff.cpp"
  "${CODECORE_ROOT}/core/snapshot/snapshot.cpp"
  "${CODECORE_ROOT}/core/engine/engine.cpp"
  "${CODECORE_ROOT}/core/lsp/json.cpp"
  "${CODECORE_ROOT}/core/lsp/protocol.cpp"
  "${CODECORE_ROOT}/core/lsp/client.cpp"
  "${CODECORE_ROOT}/native/src/codecore_api.cpp"
  "${CODECORE_ROOT}/native/src/codecore_lsp_api.cpp"
  "${CODECORE_ROOT}/native/jni/codecore_jni.cpp"
)

target_sources(codecore_jni PRIVATE
  "${CODECORE_ROOT}/third_party/tree-sitter/lib/src/lib.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-python/src/parser.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-python/src/scanner.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-javascript/src/parser.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-javascript/src/scanner.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-java/src/parser.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-go/src/parser.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-json/src/parser.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-c/src/parser.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-cpp/src/parser.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-cpp/src/scanner.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-rust/src/parser.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-rust/src/scanner.c"
)

set_source_files_properties(
  "${CODECORE_ROOT}/third_party/tree-sitter/lib/src/lib.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-python/src/parser.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-python/src/scanner.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-javascript/src/parser.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-javascript/src/scanner.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-java/src/parser.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-go/src/parser.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-json/src/parser.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-c/src/parser.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-cpp/src/parser.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-cpp/src/scanner.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-rust/src/parser.c"
  "${CODECORE_ROOT}/third_party/tree-sitter-rust/src/scanner.c"
  PROPERTIES COMPILE_OPTIONS "-w"
)

target_include_directories(codecore_jni PRIVATE
  "${CODECORE_ROOT}"
  "${CODECORE_ROOT}/native/include"
  "${CODECORE_ROOT}/third_party/tree-sitter/lib/include"
  "${CODECORE_ROOT}/third_party/tree-sitter/lib/src"
  "${CODECORE_ROOT}/third_party/tree-sitter-python/src"
  "${CODECORE_ROOT}/third_party/tree-sitter-javascript/src"
  "${CODECORE_ROOT}/third_party/tree-sitter-java/src"
  "${CODECORE_ROOT}/third_party/tree-sitter-go/src"
  "${CODECORE_ROOT}/third_party/tree-sitter-json/src"
  "${CODECORE_ROOT}/third_party/tree-sitter-c/src"
  "${CODECORE_ROOT}/third_party/tree-sitter-cpp/src"
  "${CODECORE_ROOT}/third_party/tree-sitter-rust/src"
)

target_compile_features(codecore_jni PRIVATE cxx_std_17)
target_compile_options(codecore_jni PRIVATE -Wall -Wextra)
target_link_libraries(codecore_jni PRIVATE Threads::Threads)

set_target_properties(codecore_jni PROPERTIES
  OUTPUT_NAME codecore_jni
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)
