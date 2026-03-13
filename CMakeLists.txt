cmake_minimum_required(VERSION 3.14)
project(statechart_test)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.14.0
)
set(gtest_force_shared_crt ON)
FetchContent_MakeAvailable(googletest)

enable_testing()

add_executable(statechart_test statechart_test.cc)
target_link_libraries(statechart_test gtest_main)
include(GoogleTest)
gtest_discover_tests(statechart_test)
