cmake_minimum_required(VERSION 3.15)

# Download and configure Catch2
include(FetchContent)

FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.5.2  # Latest stable as of 2024
)

FetchContent_MakeAvailable(Catch2)

# Unit tests
add_executable(test_sparse_hash
    test_basic.cpp
    test_topology.cpp
    test_queries.cpp
    test_correctness.cpp
    test_types_and_precision.cpp
    test_advanced.cpp
    test_edge_cases.cpp
    test_exception_safety.cpp
    test_limits.cpp
)

target_link_libraries(test_sparse_hash PRIVATE
    boost_sparse_spatial_hash::boost_sparse_spatial_hash
    Catch2::Catch2WithMain
)

# Add to CTest - Catch2 provides automatic test discovery
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
include(CTest)
include(Catch)
catch_discover_tests(test_sparse_hash)
