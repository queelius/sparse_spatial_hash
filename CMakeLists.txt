cmake_minimum_required(VERSION 3.15)
project(sparse_spatial_hash VERSION 1.2.0 LANGUAGES CXX)

# C++20 required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only library
add_library(sparse_spatial_hash INTERFACE)
add_library(sparse_spatial_hash::sparse_spatial_hash ALIAS sparse_spatial_hash)

target_include_directories(sparse_spatial_hash INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Require C++20
target_compile_features(sparse_spatial_hash INTERFACE cxx_std_20)

# Installation
include(GNUInstallDirs)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(TARGETS sparse_spatial_hash
    EXPORT sparse_spatial_hash-targets
)

install(EXPORT sparse_spatial_hash-targets
    FILE sparse_spatial_hash-targets.cmake
    NAMESPACE sparse_spatial_hash::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sparse_spatial_hash
)

# Generate and install config files
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/sparse_spatial_hash-config-version.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/sparse_spatial_hash-config.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/sparse_spatial_hash-config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sparse_spatial_hash
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/sparse_spatial_hash-config.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/sparse_spatial_hash-config-version.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/sparse_spatial_hash
)

# Optional: Build tests
option(BUILD_TESTS "Build tests" OFF)
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# Optional: Build examples
option(BUILD_EXAMPLES "Build examples" OFF)
if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Optional: Build benchmarks
option(BUILD_BENCHMARKS "Build benchmarks" OFF)
if(BUILD_BENCHMARKS)
    add_subdirectory(benchmark)
endif()
