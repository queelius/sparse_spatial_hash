# Development Guide

Information for contributors and developers working on the library itself.

## Contents

- **[Building](building.md)** - Building tests, examples, and benchmarks
- **[Testing](testing.md)** - Test strategy and running tests
- **[Contributing](contributing.md)** - How to contribute
- **[Design Decisions](design-decisions.md)** - Architecture and rationale

## Quick Start for Contributors

```bash
# Clone repository
git clone https://github.com/queelius/sparse_spatial_hash.git
cd sparse_spatial_hash

# Build everything
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON -DBUILD_BENCHMARKS=ON
make -j

# Run tests
ctest --output-on-failure

# Run benchmarks
./benchmark/benchmark_sparse_hash
```

[Contributing Guidelines](contributing.md){ .md-button .md-button--primary }
