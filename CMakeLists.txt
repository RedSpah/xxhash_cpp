cmake_minimum_required(VERSION 3.8)
project(xxhash_cpp LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(XXH_BUILD_TESTS "Run compatibility tests between this port and the original." OFF)

include_directories("${PROJECT_SOURCE_DIR}/include")

if(XXH_BUILD_TESTS)
    add_executable(xxh_tests ${CMAKE_CURRENT_SOURCE_DIR}/test/test_main.cpp)
    target_link_libraries(xxh_tests xxhash_cpp)
endif()
