# Delegates to the Android NDK build definition kept with the native sources.
cmake_minimum_required(VERSION 3.22.1)
project(codecore_android_wrapper LANGUAGES CXX)
add_subdirectory(
  "${CMAKE_CURRENT_LIST_DIR}/../../native/android"
  "${CMAKE_CURRENT_BINARY_DIR}/codecore_native"
)
