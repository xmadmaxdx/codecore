# Android NDK build for the Kotlin SDK.
#
# Builds the shared engine and JNI bridge into libcodecore_jni.so. JNI headers
# are supplied by the NDK sysroot, so no find_package(JNI) is used here.

cmake_minimum_required(VERSION 3.22.1)

project(codecore_android LANGUAGES CXX)

set(CODECORE_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")

add_library(codecore_jni SHARED
  "${CODECORE_ROOT}/core/text_buffer/text_buffer.cpp"
  "${CODECORE_ROOT}/core/language/language_registry.cpp"
  "${CODECORE_ROOT}/core/language/lexer.cpp"
  "${CODECORE_ROOT}/core/document/document.cpp"
  "${CODECORE_ROOT}/core/engine/engine.cpp"
  "${CODECORE_ROOT}/native/src/codecore_api.cpp"
  "${CODECORE_ROOT}/native/jni/codecore_jni.cpp"
)

target_include_directories(codecore_jni PRIVATE
  "${CODECORE_ROOT}"
  "${CODECORE_ROOT}/native/include"
)

target_compile_features(codecore_jni PRIVATE cxx_std_17)
target_compile_options(codecore_jni PRIVATE -Wall -Wextra)

set_target_properties(codecore_jni PROPERTIES
  OUTPUT_NAME codecore_jni
  CXX_VISIBILITY_PRESET hidden
  VISIBILITY_INLINES_HIDDEN ON
)
