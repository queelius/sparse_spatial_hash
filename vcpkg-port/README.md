# vcpkg Port for sparse-spatial-hash

This directory contains the vcpkg port files for submitting `sparse-spatial-hash` to the official vcpkg registry.

## Files

- **portfile.cmake**: Build instructions for vcpkg
- **vcpkg.json**: Port manifest with metadata and dependencies
- **usage**: Instructions shown to users after installation

## Submitting to vcpkg

### Step 1: Fork and Clone vcpkg

```bash
# Fork https://github.com/microsoft/vcpkg on GitHub
git clone https://github.com/YOUR_USERNAME/vcpkg.git
cd vcpkg
git remote add upstream https://github.com/microsoft/vcpkg.git
```

### Step 2: Create Topic Branch

```bash
git checkout -b sparse-spatial-hash
```

### Step 3: Copy Port Files

```bash
mkdir -p ports/sparse-spatial-hash
cp /path/to/sparse_spatial_hash/vcpkg-port/* ports/sparse-spatial-hash/
```

### Step 4: Test Locally

```bash
./vcpkg install sparse-spatial-hash
```

### Step 5: Add Version Entry

```bash
./vcpkg x-add-version sparse-spatial-hash
git add versions
```

### Step 6: Commit and Push

```bash
git add ports/sparse-spatial-hash
git commit -m "[sparse-spatial-hash] New port

sparse-spatial-hash is a generic N-dimensional sparse spatial hash grid
for high-performance spatial indexing and neighbor queries.

Features:
- Header-only C++20 library
- Zero-overhead abstractions
- N-dimensional support (2D, 3D, 4D+)
- Multiple topologies (bounded, toroidal, infinite)
- STL-compatible

Version: 2.0.0
License: BSL-1.0
"

git push origin sparse-spatial-hash
```

### Step 7: Create Pull Request

1. Go to https://github.com/YOUR_USERNAME/vcpkg
2. Click "Compare & pull request"
3. Title: `[sparse-spatial-hash] New port`
4. Fill in PR template
5. Submit as Draft PR initially for CI feedback

## Port Details

- **Name**: sparse-spatial-hash
- **Version**: 2.0.0
- **Type**: Header-only library
- **License**: Boost Software License 1.0 (BSL-1.0)
- **C++ Standard**: C++20
- **Dependencies**: None (stdlib only)
- **Platforms**: Cross-platform (!uwp)

## Testing

After submission, vcpkg CI will test on:
- Windows (x86, x64, arm64)
- Linux (x64, arm64)
- macOS (x64, arm64)

Each for 15 main triplets.

## References

- [vcpkg Contributing Guide](https://learn.microsoft.com/en-us/vcpkg/contributing/maintainer-guide)
- [Add a Port Tutorial](https://learn.microsoft.com/en-us/vcpkg/get_started/get-started-adding-to-registry)
- [Project Repository](https://github.com/queelius/sparse_spatial_hash)
