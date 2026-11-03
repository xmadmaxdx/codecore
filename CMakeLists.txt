cmake_minimum_required(VERSION 3.20)

project(CodeCore
  VERSION 0.5.0
  DESCRIPTION "Embeddable code editor engine"
  LANGUAGES C CXX)

option(CODECORE_BUILD_TESTS "Build the native test suite" ON)
option(CODECORE_BUILD_JNI "Build the optional JNI bridge" OFF)
option(CODECORE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CODECORE_CORE_SOURCES
  core/text_buffer/text_buffer.cpp
  core/language/language_registry.cpp
  core/language/lexer.cpp
  core/document/line_index.cpp
  core/document/document.cpp
  core/syntax/syntax.cpp
  core/exec/execution.cpp
  core/analysis/analysis.cpp
  core/analysis/background.cpp
  core/viewport/viewport.cpp
  core/diff/diff.cpp
  core/lsp/json.cpp
  core/lsp/protocol.cpp
  core/lsp/client.cpp
  core/snapshot/snapshot.cpp
  core/engine/engine.cpp
  native/src/codecore_api.cpp
  native/src/codecore_lsp_api.cpp
)

find_package(Threads REQUIRED)

# Vendored Tree-sitter runtime and the bundled grammars. This target is built
# as C with warnings suppressed: parser.c is generated code. Each grammar adds
# exactly one parser row plus one scanner row.
add_library(codecore_tree_sitter STATIC
  third_party/tree-sitter/lib/src/lib.c
  third_party/tree-sitter-python/src/parser.c
  third_party/tree-sitter-python/src/scanner.c
  third_party/tree-sitter-javascript/src/parser.c
  third_party/tree-sitter-javascript/src/scanner.c
  third_party/tree-sitter-java/src/parser.c
  third_party/tree-sitter-go/src/parser.c
  third_party/tree-sitter-json/src/parser.c
  third_party/tree-sitter-c/src/parser.c
  third_party/tree-sitter-cpp/src/parser.c
  third_party/tree-sitter-cpp/src/scanner.c
  third_party/tree-sitter-rust/src/parser.c
  third_party/tree-sitter-rust/src/scanner.c
)
target_include_directories(codecore_tree_sitter
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter/lib/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter/lib/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter-python/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter-javascript/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter-java/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter-go/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter-json/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter-c/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter-cpp/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter-rust/src
)
set_target_properties(codecore_tree_sitter PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(MSVC)
  target_compile_options(codecore_tree_sitter PRIVATE /W0)
else()
  target_compile_options(codecore_tree_sitter PRIVATE -w)
endif()

target_include_directories(codecore_tree_sitter PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter-python/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter-javascript/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter-java/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter-go/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter-json/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter-c/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter-cpp/src
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/tree-sitter-rust/src)

add_library(codecore_objects OBJECT ${CODECORE_CORE_SOURCES})
set_target_properties(codecore_objects PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(codecore_objects
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/native/include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_link_libraries(codecore_objects PRIVATE codecore_tree_sitter)
# The objects feed both the static and shared library. Marking the C ABI as
# dllexport here is what makes libcodecore.dll usable through Dart FFI on
# Windows; without it the DLL exports no cc_* symbols.
if(WIN32)
  target_compile_definitions(codecore_objects PRIVATE CODECORE_BUILD_SHARED)
endif()

if(MSVC)
  target_compile_options(codecore_objects PRIVATE /W4 /permissive-)
  if(CODECORE_WARNINGS_AS_ERRORS)
    target_compile_options(codecore_objects PRIVATE /WX)
  endif()
else()
  target_compile_options(codecore_objects PRIVATE -Wall -Wextra -Wpedantic)
  if(CODECORE_WARNINGS_AS_ERRORS)
    target_compile_options(codecore_objects PRIVATE -Werror)
  endif()
endif()

# Static library: primary artifact for native embedders and tests.
add_library(codecore STATIC $<TARGET_OBJECTS:codecore_objects>)
target_include_directories(codecore INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/native/include)
target_link_libraries(codecore PUBLIC Threads::Threads codecore_tree_sitter)

# Shared library: FFI / JNI host artifact.
add_library(codecore_shared SHARED $<TARGET_OBJECTS:codecore_objects>)
target_link_libraries(codecore_shared PRIVATE Threads::Threads codecore_tree_sitter)
target_compile_definitions(codecore_shared PRIVATE CODECORE_BUILD_SHARED)
target_include_directories(codecore_shared INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/native/include)
set_target_properties(codecore_shared PROPERTIES
  OUTPUT_NAME codecore
  VERSION ${PROJECT_VERSION}
  SOVERSION 0
)
if(NOT MSVC)
  target_link_options(codecore_shared PRIVATE -Wl,--no-undefined)
endif()

if(CODECORE_BUILD_JNI)
  find_package(JNI REQUIRED)
  add_library(codecore_jni SHARED native/jni/codecore_jni.cpp $<TARGET_OBJECTS:codecore_objects>)
  target_include_directories(codecore_jni PRIVATE ${JNI_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/native/include)
  target_link_libraries(codecore_jni PRIVATE codecore_tree_sitter)
  # JNI entry points are provided by the host VM at runtime; linking jvm.lib is
  # unnecessary and breaks cross-compilation (including Android NDK builds).
  set_target_properties(codecore_jni PROPERTIES OUTPUT_NAME codecore_jni)
  if(WIN32)
    # System.loadLibrary("codecore_jni") expects codecore_jni.dll, not libcodecore_jni.dll.
    set_target_properties(codecore_jni PROPERTIES PREFIX "")
  endif()
endif()

if(CODECORE_BUILD_TESTS)
  enable_testing()
  add_executable(codecore_tests
    tests/core/test_core.cpp
  )
  target_link_libraries(codecore_tests PRIVATE codecore)
  target_include_directories(codecore_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/native/include)
  if(MSVC)
    target_compile_options(codecore_tests PRIVATE /W4)
  else()
    target_compile_options(codecore_tests PRIVATE -Wall -Wextra)
  endif()
  add_test(NAME codecore_core COMMAND codecore_tests)

  add_executable(codecore_lsp_tests
    tests/core/test_lsp.cpp
  )
  target_link_libraries(codecore_lsp_tests PRIVATE codecore)
  target_include_directories(codecore_lsp_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/native/include)
  if(MSVC)
    target_compile_options(codecore_lsp_tests PRIVATE /W4)
  else()
    target_compile_options(codecore_lsp_tests PRIVATE -Wall -Wextra)
  endif()
  add_test(NAME codecore_lsp COMMAND codecore_lsp_tests)

  add_executable(codecore_exec_helper
    tests/core/exec_helper.cpp
  )
  if(MSVC)
    target_compile_options(codecore_exec_helper PRIVATE /W4)
  else()
    target_compile_options(codecore_exec_helper PRIVATE -Wall -Wextra)
  endif()

  add_executable(codecore_execution_tests
    tests/core/test_execution.cpp
  )
  target_link_libraries(codecore_execution_tests PRIVATE codecore)
  target_include_directories(codecore_execution_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/native/include)
  add_dependencies(codecore_execution_tests codecore_exec_helper)
  if(MSVC)
    target_compile_options(codecore_execution_tests PRIVATE /W4)
  else()
    target_compile_options(codecore_execution_tests PRIVATE -Wall -Wextra)
  endif()
  add_test(NAME codecore_execution COMMAND codecore_execution_tests)

  add_executable(codecore_performance_tests
    tests/core/test_performance.cpp
  )
  target_link_libraries(codecore_performance_tests PRIVATE codecore)
  target_include_directories(codecore_performance_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/native/include)
  if(MSVC)
    target_compile_options(codecore_performance_tests PRIVATE /W4)
  else()
    target_compile_options(codecore_performance_tests PRIVATE -Wall -Wextra)
  endif()
  add_test(NAME codecore_performance COMMAND codecore_performance_tests)

  add_executable(codecore_syntax_tests
    tests/core/test_syntax.cpp
  )
  target_link_libraries(codecore_syntax_tests PRIVATE codecore)
  target_include_directories(codecore_syntax_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/native/include)
  if(MSVC)
    target_compile_options(codecore_syntax_tests PRIVATE /W4)
  else()
    target_compile_options(codecore_syntax_tests PRIVATE -Wall -Wextra)
  endif()
  add_test(NAME codecore_syntax COMMAND codecore_syntax_tests)

  # Manual benchmark; intentionally not registered with CTest because timings
  # are machine-dependent.
  add_executable(codecore_benchmark tests/performance/benchmark_core.cpp)
  target_link_libraries(codecore_benchmark PRIVATE codecore)
  target_include_directories(codecore_benchmark PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/native/include)
endif()

install(TARGETS codecore codecore_shared
  ARCHIVE DESTINATION lib
  LIBRARY DESTINATION lib
  RUNTIME DESTINATION bin)
install(DIRECTORY native/include/ DESTINATION include)
