cmake_minimum_required(VERSION 3.14)
project(OrderMatchingEngine VERSION 1.0 LANGUAGES CXX)

# C++17 required
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type-specific flags
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O2 -g")

# Warning flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")

# Include directories
include_directories(${PROJECT_SOURCE_DIR}/include)

# Source files
set(OME_SOURCES
    src/order_book.cpp
    src/matching_engine.cpp
    src/wal.cpp
    src/allocator.cpp
)

# Library
add_library(ome_lib STATIC ${OME_SOURCES})

# CLI executable
add_executable(ome_cli src/cli.cpp)
target_link_libraries(ome_cli ome_lib)

# Benchmark executable
add_executable(ome_bench bench/benchmark.cpp)
target_link_libraries(ome_bench ome_lib)

# Testing
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    enable_testing()
    
    # Google Test
    include(FetchContent)
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
    )
    # For Windows: Prevent overriding the parent project's compiler/linker settings
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
    
    add_executable(ome_tests tests/test_ome.cpp)
    target_link_libraries(ome_tests ome_lib gtest gtest_main)
    
    add_test(NAME ome_tests COMMAND ome_tests)
endif()

# Installation
install(TARGETS ome_cli ome_bench
    RUNTIME DESTINATION bin
)

install(DIRECTORY include/
    DESTINATION include/ome
    FILES_MATCHING PATTERN "*.h"
)

# Print build configuration
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ compiler: ${CMAKE_CXX_COMPILER}")
message(STATUS "C++ flags: ${CMAKE_CXX_FLAGS}")
message(STATUS "C++ flags (Release): ${CMAKE_CXX_FLAGS_RELEASE}")
