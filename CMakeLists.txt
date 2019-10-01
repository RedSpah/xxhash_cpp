cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

project(xxhash)

option(XXH_BUILD_TESTS "run tests" ON)
set(CMAKE_CXX_FLAGS "-O3 -march=native -Wall -Wpedantic -Wextra")
set(CMAKE_CXX_STANDARD 14)

add_library(xxhash OBJECT ${CMAKE_CURRENT_SOURCE_DIR}/xxhash/xxhash.cpp)

if(XXH_BUILD_TESTS)
    add_executable(xxh_tests ${CMAKE_CURRENT_SOURCE_DIR}/xxhash/test_main.cpp)
    target_link_libraries(xxh_tests xxhash)
endif()
