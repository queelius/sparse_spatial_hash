# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a **header-only C++20 library** providing a generic N-dimensional sparse spatial hash grid for high-performance spatial indexing and neighbor queries. The library is designed to Boost/stdlib quality standards and aims for eventual inclusion in Boost or the C++ standard library.

**Key Design Goals:**
- Zero-overhead abstractions through generic programming
- STL-compatible (works with ranges, concepts, standard algorithms)
- Fills genuine gap in C++ ecosystem (no existing hash-based sparse spatial grids)
- Production-tested (extracted from DigiStar physics engine handling 10M+ particles)

## Build System Commands

### Building Tests
```bash
# From project root
mkdir -p build && cd build
cmake .. -DBUILD_TESTS=ON -DCMAKE_BUILD_TYPE=Release
make -j
ctest --output-on-failure
```

**Testing Framework**: Uses Catch2 v3 (automatically downloaded via CMake FetchContent)
- Modern C++20-friendly BDD-style tests
- Automatic test discovery via CTest
- Each TEST_CASE becomes a separate CTest test
- Tags for organizing tests: `[basic]`, `[topology]`, `[queries]`

**Running Tests with CTest**:
```bash
# Run all tests
cd build
ctest

# Run with verbose output
ctest --output-on-failure

# Run specific test by name
ctest -R "Grid construction"

# Run tests matching a pattern
ctest -R topology

# List all available tests
ctest -N

# Run tests in parallel
ctest -j4
```

**Running Tests Directly (Catch2 CLI)**:
```bash
# Run all tests
./test/test_sparse_hash

# Run tests matching a pattern
./test/test_sparse_hash "Grid*"

# Run tests with specific tags
./test/test_sparse_hash "[basic]"
./test/test_sparse_hash "[topology][bounded]"

# List all tests
./test/test_sparse_hash --list-tests

# List all tags
./test/test_sparse_hash --list-tags
```

### Building Examples
```bash
# From project root
mkdir -p build && cd build
cmake .. -DBUILD_EXAMPLES=ON
make -j

# Run specific example
./examples/collision_2d
./examples/molecular_dynamics_3d
./examples/toroidal_world
./examples/multi_resolution
```

### Building Benchmarks
```bash
# IMPORTANT: Always use Release build for accurate benchmarks
mkdir -p build && cd build
cmake .. -DBUILD_BENCHMARKS=ON -DCMAKE_BUILD_TYPE=Release
make -j
./benchmark/benchmark_sparse_hash
```

**Benchmarking Framework**: Uses Google Benchmark (automatically downloaded via CMake FetchContent)
- Statistical analysis, warmup, and outlier detection built-in
- Prevents compiler optimizations from invalidating results
- Comparative benchmarks included (naive O(n²), std::unordered_map baseline)

**Running Specific Benchmarks**:
```bash
# Filter benchmarks by name
./benchmark/benchmark_sparse_hash --benchmark_filter=BM_Build

# Run with repetitions and show aggregates only
./benchmark/benchmark_sparse_hash --benchmark_repetitions=3 --benchmark_report_aggregates_only=true

# Export results to JSON for analysis
./benchmark/benchmark_sparse_hash --benchmark_format=json > results.json

# Run with specific time unit
./benchmark/benchmark_sparse_hash --benchmark_time_unit=ms
```

**Key Benchmark Comparisons**:
- `BM_Update_Incremental` vs `BM_Update_FullRebuild`: Shows ~3x speedup for incremental updates
- `BM_ForEachPair_SparseHash` vs `BM_ForEachPair_Naive`: Demonstrates spatial hash advantage
- `BM_Build_*` vs `BM_Build_UnorderedMap_Baseline`: Morton encoding vs simple hashing

### Building Everything
```bash
mkdir -p build && cd build
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON -DBUILD_BENCHMARKS=ON
make -j
ctest --output-on-failure
```

### Quick Compilation Test (Manual)
```bash
# Test that header compiles standalone
g++ -std=c++20 -O2 -I./include -o collision_2d examples/collision_detection_2d.cpp
./collision_2d
```

## Architecture and Design Patterns

### Core Design Philosophy

**Header-Only Template Library**: The entire library is in `include/boost/spatial/sparse_spatial_hash.hpp` (~710 lines). This is intentional:
- Template-heavy code benefits from cross-translation-unit inlining
- Zero-overhead generic programming requires visibility at instantiation
- No linking required for users
- Compiler can optimize for specific instantiations

**Customization Points over Inheritance**: Uses trait-based customization (`position_accessor`) rather than virtual functions:
- Non-intrusive (works with any type, including POD structs)
- Compile-time polymorphism (zero runtime overhead)
- Users specialize template for their types
- Default implementation for array-like types

**Topology as First-Class Concept**: Three topology types (bounded, toroidal, infinite):
- `bounded`: Clamp to grid edges (traditional games)
- `toroidal`: Periodic wraparound (N-body simulations, Pac-Man physics)
- `infinite`: Unbounded growth (procedural worlds)

### Key Algorithmic Innovations

1. **Morton Encoding (Z-Order Curve)**: Cell coordinates use Morton code hashing for spatial locality
   - Nearby cells in space = nearby in hash table
   - Improves cache efficiency during neighborhood queries
   - See `cell_hash<Dims, CoordT>` in header

2. **Incremental Updates**: `update()` method tracks which entities changed cells
   - Only updates moved entities (typical: ~1% per frame)
   - 40x faster than `rebuild()` in typical scenarios
   - Uses `entity_cells_` tracking vector (line 244-245)

3. **Generic N-Dimensional Iteration**: Template recursion for neighborhood queries
   - `iterate_recursive<Dim>()` (line 658-676)
   - Compile-time unrolling for known dimensions
   - Works for 2D, 3D, 4D, or any dimension count

### Memory Layout

```
sparse_spatial_hash<Entity, Dims>
├── config_              : grid_config<Dims> (cell sizes, world size, topology)
├── resolution_          : std::array<int, Dims> (cells per dimension)
├── cells_               : unordered_map<cell_coord, vector<IndexT>>
│                          ↑ Sparse storage: only occupied cells exist
└── entity_cells_        : vector<cell_coord> (tracks current cell per entity)
                           ↑ Enables O(k) incremental updates
```

**Memory Efficiency**: For 10M particles in 10000³ world:
- Dense grid: 6 TB (stores every cell)
- Sparse hash: 100 MB (only occupied cells)
- **60,000x reduction**

## Testing Strategy

### Test Structure
Tests use simple assertions (no Catch2 dependency required):
- `test/test_basic.cpp`: Construction, rebuild, queries, statistics
- `test/test_topology.cpp`: Bounded, toroidal, infinite topologies
- `test/test_queries.cpp`: Empty grid edge cases, radius queries, pair iteration

### Custom Position Accessor Pattern
When writing tests for new entity types, always specialize `position_accessor`:

```cpp
struct MyEntity { glm::vec3 pos; };

template<>
struct boost::spatial::position_accessor<MyEntity, 3> {
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

Expected results for 10,000 particles (2D, 1000×1000 world):
- Build: ~1-2 ms
- Update: ~0.2 ms/frame
- Query radius: <1 ms
- Collision detection: ~5-10 ms

## Code Style and Standards

**Boost/Stdlib Conventions:**
- `snake_case` for all identifiers (not `camelCase`)
- Type aliases use `_type` suffix: `entity_type`, `index_type`
- Concepts use lowercase: `coordinate_type`, `has_position`
- Public interface documented with complexity guarantees and exception safety

**Exception Safety Guarantees** (must be maintained):
- Queries: Nothrow guarantee
- Single-entity operations: Strong guarantee
- Bulk operations: Basic guarantee

**C++20 Features Used:**
- Concepts (`requires` clauses)
- Ranges and views (`std::ranges::range`, `std::views::all`)
- Three-way comparison (`operator<=>`)
- Designated initializers (`.cell_size = {...}`)

## Common Development Tasks

### Adding a New Topology Type
1. Add enum value to `topology` enum (line 72-76)
2. Implement wrapping logic in `wrap_cell()` (line 624-643)
3. Add test in `test/test_topology.cpp`
4. Document behavior in README.md and tutorial

### Adding a New Query Type
1. Add public method in "Queries" section (line 456+)
2. Document complexity and exception safety
3. Add test in `test/test_queries.cpp`
4. Add example usage to `docs/getting-started/tutorial.md`

### Performance Optimization
When optimizing:
1. **Profile first**: Use benchmarks to identify bottlenecks
2. **Maintain genericity**: Don't hard-code dimensions or types
3. **Document trade-offs**: If specialization needed, explain why
4. **Benchmark after**: Verify improvement is measurable

### Adding Examples
Examples should:
- Be complete, runnable programs (with `main()`)
- Demonstrate a specific use case or pattern
- Include performance output
- Be simple enough to understand quickly (< 200 lines)

## Important Constraints

**Do NOT:**
- Add external dependencies (keep header-only, stdlib-only)
- Use virtual functions or RTTI (zero-overhead requirement)
- Hard-code dimension count (must work for N dimensions)
- Break exception safety guarantees
- Change public API without strong justification (Boost-quality stability)

**Do:**
- Use concepts for constraints (better error messages)
- Prefer compile-time computation (`constexpr`, templates)
- Maintain STL compatibility (iterators, ranges)
- Document complexity guarantees
- Add tests for new features

## Boost Submission Considerations

This library is being prepared for potential Boost submission. When making changes:

1. **Maintain Generic Programming**: Must work for arbitrary dimensions, coordinate types, precision
2. **Zero Overhead**: Template instantiations should compile to optimal code
3. **Documentation**: Every public method needs complexity and exception safety docs
4. **Testing**: Comprehensive coverage of edge cases
5. **Portability**: C++20 compliant (GCC 10+, Clang 12+, MSVC 2019+)

## Performance Characteristics Reference

| Operation | Complexity | Typical Time (10K entities) |
|-----------|-----------|---------------------------|
| `rebuild()` | O(n) | ~1-2 ms |
| `update()` | O(k), k ≈ 0.01n | ~0.2 ms (40x faster) |
| `query_radius()` | O(m) cells queried | <1 ms |
| `for_each_pair()` | O(c × k²) | ~5-10 ms |

Where:
- n = total entities
- k = entities that changed cells
- m = entities in queried cells
- c = occupied cells
- k̄ = average entities per cell

## Related Documentation

- `README.md`: User-facing documentation, quick start, examples
- `docs/`: MkDocs documentation site (published to GitHub Pages)
  - `docs/getting-started/tutorial.md`: Step-by-step guide for library users
  - `docs/api-reference/`: Complete API documentation
  - `docs/user-guide/`: In-depth usage guides
- `PROJECT_SUMMARY.md`: Design decisions, extraction rationale, comparisons
- Header comments: API reference with complexity guarantees
