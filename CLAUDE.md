# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a **header-only C++20 library** providing a generic N-dimensional sparse spatial hash grid for high-performance spatial indexing and neighbor queries. The library is designed to stdlib quality standards and is publicly released.

**Key Design Goals:**
- Zero-overhead abstractions through generic programming
- STL-compatible (works with ranges, concepts, standard algorithms)
- Fills genuine gap in C++ ecosystem (no existing hash-based sparse spatial grids)
- Production-tested (extracted from DigiStar physics engine handling 10M+ particles)

**Current Status:** v2.0.0 - Public Release
- GitHub: https://github.com/queelius/sparse_spatial_hash
- vcpkg: PR #48244 pending (all 18 CI platforms passing)
- 54 comprehensive tests, 326 assertions, 100% pass rate
- Exception safety guaranteed and tested
- Small vector optimization: 5-40% performance improvement

## Build System Commands

### Building and Running Tests
```bash
# From project root
mkdir -p build && cd build
cmake .. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
make -j
./test/test_sparse_hash  # Direct execution (fastest)

# Or use CTest
ctest --output-on-failure
```

**Testing Framework**: Catch2 v3 (auto-downloaded via CMake FetchContent)

**Running Specific Tests (Catch2 CLI)**:
```bash
# Run tests matching a pattern
./test/test_sparse_hash "Grid*"

# Run tests with specific tags
./test/test_sparse_hash "[basic]"
./test/test_sparse_hash "[edge]"
./test/test_sparse_hash "[exception]"

# List all tests
./test/test_sparse_hash --list-tests

# Compact output
./test/test_sparse_hash --reporter compact
```

### Building Benchmarks
```bash
# IMPORTANT: Always use Release build for accurate benchmarks
mkdir -p build && cd build
cmake .. -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
make -j
./benchmark/benchmark_sparse_hash

# Filter specific benchmarks
./benchmark/benchmark_sparse_hash --benchmark_filter=BM_Update
```

### Building Everything
```bash
mkdir -p build && cd build
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON -DBUILD_BENCHMARKS=ON
make -j
```

### Strict Warnings Build
```bash
# To catch all potential issues
cmake .. -DBUILD_TESTS=ON -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic"
make -j
```

## Architecture and Design Patterns

### Core Design Philosophy

**Header-Only Template Library**: The entire library is in `include/spatial/sparse_spatial_hash.hpp` (~970 lines). This is intentional:
- Template-heavy code benefits from cross-translation-unit inlining
- Zero-overhead generic programming requires visibility at instantiation
- No linking required for users
- Compiler can optimize for specific instantiations

**Small Vector Optimization**: Custom `small_vector<T, N>` for inline storage (lines 50-190)
- Stores ≤N elements on stack, avoids heap allocation for small cells
- 80-90% of cells contain ≤16 entities, so default N=16 is optimal
- Provides 5-40% speedup (40% on rebuild, 5-11% on queries/updates)
- Configurable via `SmallCellSize` template parameter (default=16, set to 0 to disable)
- Memory cost: +8*SmallCellSize bytes per cell (~256 bytes with default)

**Customization Points over Inheritance**: Uses trait-based customization (`position_accessor`) rather than virtual functions:
- Non-intrusive (works with any type, including POD structs)
- Compile-time polymorphism (zero runtime overhead)
- Users specialize template for their types
- Default implementation for array-like types

**Topology as First-Class Concept**: Three topology types:
- `bounded`: Clamp to grid edges (traditional games)
- `toroidal`: Periodic wraparound (N-body simulations, Pac-Man physics)
- `infinite`: Unbounded growth (procedural worlds)

### Key Algorithmic Innovations

1. **Morton Encoding (Z-Order Curve)**: Cell coordinates use Morton code hashing for spatial locality
   - Nearby cells in space = nearby in hash table
   - Improves cache efficiency during neighborhood queries
   - **Coordinate limits**: 2D: ±2^31 cells, 3D: ±2^21 cells, 4D+: further reduced
   - Documented in header comments (lines 180-186)

2. **Incremental Updates**: `update()` method tracks which entities changed cells
   - Only updates moved entities (typically <5% per frame)
   - ~40x faster than `rebuild()` when few entities move
   - Uses `entity_cells_` tracking vector for O(k) updates where k = moved entities
   - Swap-and-pop optimization for O(1) amortized removal

3. **Generic N-Dimensional Iteration**: Template recursion for neighborhood queries
   - `iterate_recursive<Dim>()` with compile-time unrolling
   - Works for 2D, 3D, 4D, or any dimension count

4. **Small Vector Optimization**: Inline storage for small cells eliminates malloc overhead
   - Typical cells (≤16 entities) stored inline on stack
   - Falls back to heap for larger cells transparently
   - 40% faster rebuild, 5-11% faster updates/queries
   - Tunable via `SmallCellSize` template parameter

### Memory Layout

```
sparse_spatial_hash<Entity, Dims, FloatT, IndexT, SmallCellSize>
├── config_              : grid_config<Dims> (cell sizes, world size, topology)
├── resolution_          : std::array<int, Dims> (cells per dimension)
├── cell_size_inv_       : std::array<FloatT, Dims> (precomputed 1/cell_size)
├── cells_               : unordered_map<cell_coord, small_vector<IndexT, SmallCellSize>>
│                          ↑ Sparse storage: only occupied cells exist
│                          ↑ small_vector: inline storage for ≤SmallCellSize entities
└── entity_cells_        : vector<cell_coord> (tracks current cell per entity)
                           ↑ Auto-tracked by rebuild()/update()
```

**Memory Efficiency**: For 10M particles in 10000³ world:
- Dense grid: 6 TB (stores every cell)
- Sparse hash: 100 MB (only occupied cells)
- **60,000x reduction**

## Testing Strategy

### Test Structure (54 tests, 326 assertions)

- `test/test_basic.cpp`: Construction, rebuild, queries, statistics (7 tests)
- `test/test_topology.cpp`: Bounded, toroidal, infinite topologies (5 tests)
- `test/test_queries.cpp`: Empty grid edge cases, radius queries, pair iteration (4 tests)
- `test/test_correctness.cpp`: Morton encoding, pair correctness, incremental updates (4 tests)
- `test/test_types_and_precision.cpp`: Type system, precision, large-scale (5 tests)
- `test/test_advanced.cpp`: STL compat, move/copy semantics, statistics (6 tests)
- `test/test_edge_cases.cpp`: Boundary conditions, overflow protection (5 tests)
- `test/test_exception_safety.cpp`: Exception guarantees verified (11 tests)
- `test/test_limits.cpp`: Morton encoding limits at boundaries (7 tests)

### Custom Position Accessor Pattern

When writing tests for new entity types, always specialize `position_accessor`:

```cpp
struct MyEntity { glm::vec3 pos; };

template<>
struct spatial::position_accessor<MyEntity, 3> {
    static float get(const MyEntity& e, std::size_t dim) {
        return e.pos[dim];
    }
};
```

### Performance Verification

After changes, verify performance hasn't regressed:
```bash
cd build && ./benchmark/benchmark_sparse_hash
```

Expected results for 10,000 particles (2D, 1000×1000 world, SmallCellSize=16):
- Build: ~0.6-1 ms (40% faster than v1.0 due to small vector optimization)
- Update: ~0.2 ms/frame (40x faster than rebuild when <5% move)
- Query radius: <1 ms (5-7% faster than v1.0)
- Collision detection: ~5-10 ms

## Code Style and Standards

**Boost/Stdlib Conventions:**
- `snake_case` for all identifiers (not `camelCase`)
- Type aliases use `_type` suffix: `entity_type`, `index_type`
- Concepts use lowercase: `coordinate_type`, `has_position`
- Public interface documented with complexity guarantees and exception safety

**Exception Safety Guarantees** (must be maintained):
- Queries (`query_radius`, `cell_entities`, `stats`): **Nothrow guarantee**
- Copy operations: **Strong guarantee**
- Bulk operations (`rebuild`, `update`): **Basic guarantee**
- All guarantees verified in `test/test_exception_safety.cpp`

**C++20 Features Used:**
- Concepts (`requires` clauses)
- Ranges and views (`std::ranges::range`, `std::views::all`)
- Three-way comparison (`operator<=>`)
- Designated initializers (`.cell_size = {...}`)

## Common Development Tasks

### Adding a New Query Type
1. Add public method in "Queries" section (line 540+)
2. Document complexity and exception safety guarantees
3. Mark `const` and `noexcept` if appropriate
4. Add test in `test/test_queries.cpp`
5. Add example usage to `docs/getting-started/tutorial.md`

### Performance Optimization

**Lessons from v1.0 → v1.1 Optimization Experiments** (see `docs/performance/optimization-experiments.md`):
- **v2 (reverse mapping)**: FAILED - O(1) lookup slower than O(n) linear search for small n due to indirection overhead
- **v3 (parallel for_each_pair)**: FAILED - OpenMP overhead >> work done, atomic contention catastrophic
- **v4 (small vector)**: SUCCESS - Targeted common case (≤16 entities/cell), eliminated malloc bottleneck

**Key Insights:**
- Asymptotic complexity doesn't predict real performance for small n
- Simple, cache-friendly code beats "clever" optimizations
- Parallelization only helps when work >> synchronization overhead
- Target the common case, not worst-case

When optimizing:
1. **Profile first**: Use benchmarks to identify bottlenecks
2. **Test in isolation**: One optimization at a time, compare against baseline
3. **Maintain genericity**: Don't hard-code dimensions or types
4. **Document trade-offs**: If specialization needed, explain why
5. **Benchmark after**: Verify improvement is measurable (not just theoretical)
6. **Check warnings**: Build with `-Wall -Wextra -Wpedantic`

### Adding Tests for New Features
Required test coverage:
- Basic functionality test
- Edge cases (empty grids, boundary conditions, extreme values)
- Exception safety (if modifying operation)
- Performance characteristics (if claiming speedup)
- All topologies (bounded, toroidal, infinite)
- Multiple dimensions (at least 2D and 3D)

## Important Constraints

**Do NOT:**
- Add external dependencies (keep header-only, stdlib-only)
- Use virtual functions or RTTI (zero-overhead requirement)
- Hard-code dimension count (must work for N dimensions)
- Break exception safety guarantees (nothrow queries!)
- Change public API without strong justification (Boost-quality stability)

**Do:**
- Use concepts for constraints (better error messages)
- Prefer compile-time computation (`constexpr`, templates)
- Maintain STL compatibility (iterators, ranges)
- Document complexity guarantees and exception safety
- Add comprehensive tests (basic + edge cases + exception safety)
- Mark methods `noexcept` where appropriate
- Use `[[nodiscard]]` for queries that return values

## Critical Implementation Details

### Morton Encoding Limits
- **2D**: ±2^31 cells per dimension (~2.1 billion)
- **3D**: ±2^21 cells per dimension (~2.1 million)
- **4D+**: Further reduced range
- For `infinite` topology, coordinates beyond these limits will have hash collisions
- Tested at actual boundaries in `test/test_limits.cpp`

### Coordinate System
World coordinates are centered at origin:
```
Position (x, y) → Cell coordinate:
  cell[d] = floor((pos[d] + world_size[d]/2) / cell_size[d])

Example: world_size=100, cell_size=10, pos=5
  cell = floor((5 + 50) / 10) = floor(5.5) = 5
```

## Distribution and Installation

This library is publicly released and available through multiple installation methods:

**CMake FetchContent (Recommended):**
```cmake
include(FetchContent)
FetchContent_Declare(
  sparse_spatial_hash
  GIT_REPOSITORY https://github.com/queelius/sparse_spatial_hash.git
  GIT_TAG        v2.0.0
)
FetchContent_MakeAvailable(sparse_spatial_hash)
target_link_libraries(your_target PRIVATE sparse_spatial_hash::sparse_spatial_hash)
```

**vcpkg (Pending - PR #48244):**
```bash
vcpkg install sparse-spatial-hash
```

**Manual Installation:**
Simply copy `include/spatial/` to your include path, or use CMake's install target.

## Performance Characteristics Reference

| Operation | Complexity | Typical Time (10K entities, v1.1.0) |
|-----------|-----------|---------------------------|
| `rebuild()` | O(n) | ~0.6-1 ms (40% faster than v1.0) |
| `update()` | O(k), k = moved entities | ~0.2 ms (40x faster than rebuild when k<0.05n) |
| `query_radius()` | O(m), m = entities in queried cells | <1 ms (5-7% faster than v1.0) |
| `for_each_pair()` | O(c × k²), c = cells, k̄ = avg per cell | ~5-10 ms |

Where:
- n = total entities
- k = entities that changed cells
- m = entities in queried cells
- c = occupied cells
- k̄ = average entities per cell

## Related Documentation

- `README.md`: User-facing documentation, quick start, examples
- `docs/`: MkDocs documentation site at https://queelius.github.io/sparse_spatial_hash/
  - `docs/getting-started/tutorial.md`: Step-by-step guide
  - `docs/api-reference/`: Complete API documentation
  - `docs/user-guide/`: In-depth usage guides
  - `docs/distribution/`: Installation and distribution information
- `docs/development/test-review.md`: Comprehensive test suite analysis (40 pages)
- `docs/development/test-summary.md`: Executive summary of test coverage
- `docs/development/test-strategy.md`: Comprehensive test strategy documentation
- `docs/development/project-summary.md`: Design decisions, extraction rationale, comparisons
- `docs/performance/optimization-experiments.md`: Detailed analysis of v2/v3/v4 optimization attempts
- `vcpkg-port/`: vcpkg port files (portfile.cmake, vcpkg.json, usage)
- Header comments: API reference with complexity guarantees
