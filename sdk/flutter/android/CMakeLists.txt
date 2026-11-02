# Android NDK build for the Dart FFI package.
#
# The source tree is shared with the Kotlin SDK; there is no second engine
# implementation. This target produces libcodecore.so, which Gradle packages
# into the plugin AAR and the Dart loader opens by name.

cmake_minimum_required(VERSION 3.22.1)

project(codecore_flutter LANGUAGES CXX)

set(CODECORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../..")

add_library(codecore SHARED
  "${CODECORE_ROOT}/core/text_buffer/text_buffer.cpp"
  "${CODECORE_ROOT}/core/language/language_registry.cpp"
  "${CODECORE_ROOT}/core/language/lexer.cpp"
  "${CODECORE_ROOT}/core/document/line_index.cpp"
  "${CODECORE_ROOT}/core/document/document.cpp"
  "${CODECORE_ROOT}/core/analysis/analysis.cpp"
  "${CODECORE_ROOT}/core/diff/diff.cpp"
  "${CODECORE_ROOT}/core/snapshot/snapshot.cpp"
  "${CODECORE_ROOT}/core/engine/engine.cpp"
  "${CODECORE_ROOT}/native/src/codecore_api.cpp"
)

target_include_directories(codecore PRIVATE
  "${CODECORE_ROOT}"
  "${CODECORE_ROOT}/native/include"
)

target_compile_features(codecore PRIVATE cxx_std_17)
target_compile_options(codecore PRIVATE -Wall -Wextra)

set_target_properties(codecore PROPERTIES OUTPUT_NAME codecore)
