# Installation

Boost.Spatial - Sparse Spatial Hash is a **header-only library** requiring no compilation or linking. Simply include the header and start using it!

## Requirements

### Compiler Support

- **GCC**: 10.0 or later
- **Clang**: 12.0 or later
- **MSVC**: Visual Studio 2019 (19.28) or later
- **C++ Standard**: C++20

### Dependencies

- **Runtime**: None (standard library only)
- **Testing** (optional): Catch2 v3 (auto-downloaded via CMake)
- **Benchmarks** (optional): Google Benchmark (auto-downloaded via CMake)

## Installation Methods

### Method 1: Direct Header Copy (Simplest)

Copy the header file to your project:

```bash
# Clone the repository
git clone https://github.com/spinoza/sparse_spatial_hash.git

# Copy header to your include path
cp sparse_spatial_hash/include/boost/spatial/sparse_spatial_hash.hpp \
   /your/project/include/boost/spatial/
```

Then in your code:

```cpp
#include <boost/spatial/sparse_spatial_hash.hpp>
```

### Method 2: CMake FetchContent (Recommended)

Add to your `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.15)
project(MyProject CXX)

include(FetchContent)

FetchContent_Declare(
    sparse_spatial_hash
    GIT_REPOSITORY https://github.com/spinoza/sparse_spatial_hash.git
    GIT_TAG        main  # or specific version tag
)

FetchContent_MakeAvailable(sparse_spatial_hash)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE sparse_spatial_hash::sparse_spatial_hash)
target_compile_features(my_app PRIVATE cxx_std_20)
```

### Method 3: CMake find_package

If library is installed system-wide:

```cmake
find_package(sparse_spatial_hash REQUIRED)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE sparse_spatial_hash::sparse_spatial_hash)
target_compile_features(my_app PRIVATE cxx_std_20)
```

### Method 4: Git Submodule

```bash
# Add as submodule
git submodule add https://github.com/spinoza/sparse_spatial_hash.git external/sparse_spatial_hash
git submodule update --init --recursive
```

In `CMakeLists.txt`:

```cmake
add_subdirectory(external/sparse_spatial_hash)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE sparse_spatial_hash::sparse_spatial_hash)
```

### Method 5: System-Wide Installation

```bash
# Clone and install
git clone https://github.com/spinoza/sparse_spatial_hash.git
cd sparse_spatial_hash
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
sudo make install
```

Then use with `find_package` (Method 3).

## Verification

### Quick Compilation Test

Create `test.cpp`:

```cpp
#include <boost/spatial/sparse_spatial_hash.hpp>
#include <iostream>
#include <vector>

struct Point {
    std::array<float, 2> coords;
};

int main() {
    using namespace boost::spatial;

    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Point, 2> grid(cfg);

    std::vector<Point> points = {
        {{25.0f, 25.0f}},
        {{75.0f, 75.0f}}
    };

    grid.rebuild(points);

    std::cout << "Grid created with " << grid.entity_count()
              << " entities in " << grid.cell_count()
              << " cells\n";

    return 0;
}
```

Compile and run:

```bash
# GCC
g++ -std=c++20 -I/path/to/include -O2 test.cpp -o test
./test

# Clang
clang++ -std=c++20 -I/path/to/include -O2 test.cpp -o test
./test

# MSVC
cl /std:c++20 /EHsc /I\path\to\include /O2 test.cpp
test.exe
```

Expected output:
```
Grid created with 2 entities in 2 cells
```

## Package Managers

### vcpkg (Coming Soon)

```bash
vcpkg install sparse-spatial-hash
```

### Conan (Coming Soon)

```ini
[requires]
sparse-spatial-hash/1.0.0
```

## Building Examples

To build and run the example programs:

```bash
git clone https://github.com/spinoza/sparse_spatial_hash.git
cd sparse_spatial_hash
mkdir build && cd build
cmake .. -DBUILD_EXAMPLES=ON
make -j
./examples/collision_2d
./examples/molecular_dynamics_3d
```

## Building Tests

To run the test suite:

```bash
cd sparse_spatial_hash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON
make -j
ctest --output-on-failure
```

Expected result: All 31 tests should pass.

## Building Benchmarks

To run performance benchmarks:

```bash
cd sparse_spatial_hash
mkdir build && cd build
cmake .. -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
make -j
./benchmark/benchmark_sparse_hash
```

!!! warning "Always Use Release Mode for Benchmarks"
    Debug builds can be 10-100x slower. Always use `-DCMAKE_BUILD_TYPE=Release` for meaningful performance measurements.

## Troubleshooting

### Compiler Errors

**"concept not satisfied" or "requires clause"**

→ Your compiler doesn't support C++20. Upgrade to GCC 10+, Clang 12+, or MSVC 2019+.

**"std::ranges not found"**

→ You're compiling with C++17 or earlier. Add `-std=c++20` or `/std:c++20`.

### Linker Errors

**"undefined reference"**

→ This is a header-only library. Make sure you're including the header, not trying to link against it.

### CMake Errors

**"Could not find sparse_spatial_hash"**

→ Use `FetchContent` instead of `find_package`, or install the library system-wide first.

## IDE Setup

### Visual Studio Code

Add to `.vscode/c_cpp_properties.json`:

```json
{
    "configurations": [
        {
            "name": "Linux",
            "includePath": [
                "${workspaceFolder}/**",
                "/path/to/sparse_spatial_hash/include"
            ],
            "compilerPath": "/usr/bin/g++",
            "cStandard": "c17",
            "cppStandard": "c++20",
            "intelliSenseMode": "linux-gcc-x64"
        }
    ]
}
```

### CLion

CMake automatically handles includes. Just open the project or use `FetchContent`.

### Visual Studio

Add include path in project properties:
```
Configuration Properties → C/C++ → General → Additional Include Directories
→ Add: C:\path\to\sparse_spatial_hash\include
```

## Next Steps

✓ Installation complete!

Now proceed to:

- [Quick Start Guide](quick-start.md) - Get coding in 5 minutes
- [Tutorial](tutorial.md) - Comprehensive guide
- [API Reference](../api-reference/index.md) - Full API documentation

## Support

Having installation issues?

- Check [GitHub Issues](https://github.com/spinoza/sparse_spatial_hash/issues)
- Ask in [Discussions](https://github.com/spinoza/sparse_spatial_hash/discussions)
- Review [Troubleshooting Guide](../development/building.md#troubleshooting)
