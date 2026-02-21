cmake_minimum_required(VERSION 3.14)
project(OrderMatchingEngine CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Release build: -O2 with debug symbols for profiling
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS_RELEASE "-O2 -DNDEBUG")
set(CMAKE_CXX_FLAGS_DEBUG   "-g -O0 -fsanitize=address,undefined")

# ─── Library (shared between main and tests) ──────────────────────────────────
add_library(engine_lib STATIC
    src/order_book.cpp
    src/engine.cpp
)
target_include_directories(engine_lib PUBLIC src)

# ─── Main Executable ──────────────────────────────────────────────────────────
add_executable(matching_engine src/main.cpp)
target_link_libraries(matching_engine PRIVATE engine_lib)

# ─── Google Test ──────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
# Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()
add_subdirectory(tests)