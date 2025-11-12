# Package Managers

Multiple installation methods are available for the sparse_spatial_hash library.

## vcpkg (Recommended)

The library is available on [vcpkg](https://github.com/microsoft/vcpkg), Microsoft's C++ package manager.

### Installation

```bash
vcpkg install sparse-spatial-hash
```

### CMake Integration

```cmake
find_package(sparse_spatial_hash CONFIG REQUIRED)
target_link_libraries(your_target PRIVATE sparse_spatial_hash::sparse_spatial_hash)
```

### Status

- ✅ **PR Submitted**: [microsoft/vcpkg#48244](https://github.com/microsoft/vcpkg/pull/48244)
- 🔄 **Status**: Under review
- 📦 **Estimated availability**: 1-2 weeks

## CMake FetchContent

Fetch directly from GitHub in your CMakeLists.txt:

```cmake
include(FetchContent)

FetchContent_Declare(
  sparse_spatial_hash
  GIT_REPOSITORY https://github.com/queelius/sparse_spatial_hash.git
  GIT_TAG        v2.0.0  # Use latest release tag
)

FetchContent_MakeAvailable(sparse_spatial_hash)

target_link_libraries(your_target PRIVATE sparse_spatial_hash::sparse_spatial_hash)
```

**Advantages**:
- Always up-to-date
- No external package manager needed
- Version pinning via Git tags

## Manual Installation

Copy the header directly to your project:

```bash
# Clone repository
git clone https://github.com/queelius/sparse_spatial_hash.git

# Copy header
cp sparse_spatial_hash/include/spatial/sparse_spatial_hash.hpp \
   /your/project/include/spatial/
```

Then include in your code:

```cpp
#include <spatial/sparse_spatial_hash.hpp>
```

## System-Wide Installation

Install to system directories:

```bash
git clone https://github.com/queelius/sparse_spatial_hash.git
cd sparse_spatial_hash
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
sudo cmake --install .
```

Then use with `find_package`:

```cmake
find_package(sparse_spatial_hash REQUIRED)
target_link_libraries(your_target PRIVATE sparse_spatial_hash::sparse_spatial_hash)
```

## Comparison

| Method | Ease | Updates | Version Control |
|--------|------|---------|-----------------|
| **vcpkg** | ⭐⭐⭐⭐⭐ | Managed | Automatic |
| **FetchContent** | ⭐⭐⭐⭐ | Manual (Git tags) | Excellent |
| **Manual** | ⭐⭐⭐ | Manual | Manual |
| **System Install** | ⭐⭐ | Manual | Manual |

## See Also

- [Installation Guide](../getting-started/installation.md) - Detailed installation instructions
- [Quick Start](../getting-started/quick-start.md) - Get coding immediately
- [GitHub Releases](https://github.com/queelius/sparse_spatial_hash/releases) - Download specific versions
