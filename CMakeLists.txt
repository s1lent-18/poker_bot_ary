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

add_executable(test_cards tests/test_cards.cpp
  src/eval/hand_evaluator.h)
target_link_libraries(test_cards PRIVATE poker_core GTest::gtest_main)
gtest_discover_tests(test_cards)

add_library(poker_eval STATIC src/eval/hand_evaluator.cpp)
target_link_libraries(poker_eval PUBLIC poker_core)

add_executable(test_evaluator tests/test_evaluator.cpp)
target_link_libraries(test_evaluator PRIVATE poker_eval GTest::gtest_main)
gtest_discover_tests(test_evaluator)

add_library(poker_engine STATIC src/engine/game_state.cpp)
target_link_libraries(poker_engine PUBLIC poker_eval)

add_executable(test_game_state tests/test_game_state.cpp)
target_link_libraries(test_game_state PRIVATE poker_engine GTest::gtest_main)
gtest_discover_tests(test_game_state)

add_library(poker_agents INTERFACE)
target_link_libraries(poker_agents INTERFACE poker_engine)

add_executable(poker_cli src/app/main.cpp)
target_link_libraries(poker_cli PRIVATE poker_agents)

add_executable(test_agents tests/test_agents.cpp)
target_link_libraries(test_agents PRIVATE poker_agents GTest::gtest_main)
gtest_discover_tests(test_agents)