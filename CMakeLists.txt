cmake_minimum_required(VERSION 3.16)
project(poker CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Header-only core library
add_library(poker_core INTERFACE)
target_include_directories(poker_core INTERFACE ${CMAKE_SOURCE_DIR}/src)

# GoogleTest
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)  # needed on Windows/MSVC
FetchContent_MakeAvailable(googletest)

enable_testing()
include(GoogleTest)

add_executable(test_cards tests/test_cards.cpp)
target_link_libraries(test_cards PRIVATE poker_core GTest::gtest_main)
gtest_discover_tests(test_cards)

