cmake_minimum_required(VERSION 3.15)

# Download and configure Google Benchmark
include(FetchContent)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "Disable benchmark tests" FORCE)
set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "Disable gtest in benchmark" FORCE)

FetchContent_Declare(
    googlebenchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3  # Latest stable as of 2024
)

FetchContent_MakeAvailable(googlebenchmark)

# Create benchmark executable
add_executable(benchmark_sparse_hash benchmark_main.cpp)

target_link_libraries(benchmark_sparse_hash PRIVATE
    boost_sparse_spatial_hash::boost_sparse_spatial_hash
    benchmark::benchmark
)

# Enable optimizations for benchmarks
target_compile_options(benchmark_sparse_hash PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-O3 -march=native>
    $<$<CXX_COMPILER_ID:MSVC>:/O2>
)

