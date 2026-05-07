# Sparse Spatial Hash Grid

[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.wikipedia.org/wiki/C%2B%2B20)
[![License](https://img.shields.io/badge/license-Boost-blue.svg)](http://www.boost.org/LICENSE_1_0.txt)
[![Header-Only](https://img.shields.io/badge/header--only-yes-green.svg)]()

A **generic N-dimensional sparse spatial hash grid** for high-performance spatial indexing and neighbor queries. Designed to stdlib quality standards with zero-overhead abstractions.

## Features

- **N-Dimensional Support**: Works with 2D, 3D, 4D, or any dimension count
- **Sparse Storage**: Memory-efficient hash-based storage (only occupied cells)
- **Multiple Topologies**: Bounded, toroidal (periodic), and infinite spaces
- **Incremental Updates**: O(k) updates where k ≈ 1% of entities (typical)
- **STL-Compatible**: Works with ranges, concepts, and standard algorithms
- **Header-Only**: Single include, no linking required
- **Zero-Overhead**: Generic programming with compile-time polymorphism
- **Morton Hashing**: Cache-friendly Z-order curve for spatial locality
- **Small Vector Optimization**: Inline storage for small cells (5-40% faster)

## Quick Start

```cpp
#include <spatial/sparse_spatial_hash.hpp>
#include <vector>

struct Particle {
    float x, y, z;
    float vx, vy, vz;
};

// Customize position extraction (one-time setup)
template<>
struct spatial::position_accessor<Particle, 3> {
    static float get(const Particle& p, std::size_t dim) {
        switch(dim) {
            case 0: return p.x;
            case 1: return p.y;
            case 2: return p.z;
            default: return 0.0f;
        }
    }
};

int main() {
    using namespace spatial;

    // Create 3D toroidal grid (1000³ world, 10-unit cells)
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::toroidal
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);

    // Create particles
    std::vector<Particle> particles(10000);
    // ... initialize particles ...

    // Build spatial index
    grid.rebuild(particles);

    // Query neighbors within 50 units of position (100, 200, 300)
    auto neighbors = grid.query_radius(50.0f, 100.0f, 200.0f, 300.0f);

    // Process all pairs within 20 units
    grid.for_each_pair(20.0f,
        [&](std::size_t i, std::size_t j) {
            // Compute interaction between particles[i] and particles[j]
        });

    // Efficient incremental update (only entities that changed cells)
    // Typical: ~1% of particles move cells per frame
    grid.update(particles);  // Much faster than rebuild()

    // Get statistics
    auto stats = grid.stats();
    std::cout << "Occupancy: " << stats.occupancy_ratio << "\n";
    std::cout << "Avg entities/cell: " << stats.avg_entities_per_cell << "\n";
}
```

## Performance Comparison

### Memory Efficiency

For a **10 million particle** simulation in a **10,000³ world**:

| Approach | Memory Usage | Notes |
|----------|--------------|-------|
| **Dense Grid** | 6 TB | Stores every cell (1000³ cells) |
| **Octree** | 400 MB | Tree structure overhead |
| **R-tree** | 400 MB | Balancing overhead |
| **Sparse Hash** | **100 MB** | Only occupied cells (1M/1B ratio) |

**Memory reduction: 60,000x** vs dense grid!

### Time Complexity

| Operation | Dense Grid | R-tree | **Sparse Hash** |
|-----------|-----------|---------|-----------------|
| Insert | O(1) | O(log n) | **O(1)** |
| Query radius | O(k) | O(log n + k) | **O(k)** |
| Incremental update | O(n) | O(m log n) | **O(m), m ≈ 0.01n** |
| Build | O(n) | O(n log n) | **O(n)** |

### Real-World Performance

From DigiStar physics engine (10M particles, 10000³ world):

- **Rebuild**: ~80ms (full grid reconstruction)
- **Incremental Update**: ~2ms (when 1% of particles change cells)
- **Collision Detection**: ~150ms (20-unit interaction radius)
- **Memory**: 100MB grid + 80MB particle tracking

**40x faster** than rebuild when using incremental updates!

**Latest Optimizations** (v1.1.0+):
- Small vector optimization provides **5-40% speedup** across all operations
- **40% faster rebuild** through inline storage for small cells (≤16 entities)
- **5-11% faster updates and queries** via improved cache locality
- Memory tradeoff: +256 bytes per occupied cell (acceptable for most workloads)

## Use Cases

### Game Development
- **Collision Detection**: Broad-phase spatial partitioning
- **AI Pathfinding**: Efficient neighbor queries
- **Rendering Culling**: Frustum-based entity filtering
- **Particle Systems**: Fast neighbor searches

### Scientific Computing
- **N-body Simulations**: Gravity/electrostatics neighbor lists
- **Molecular Dynamics**: Bonded/non-bonded interactions
- **SPH Fluids**: Smoothed particle hydrodynamics
- **Atmospheric Modeling**: Grid-based weather simulations

### Robotics
- **SLAM**: Spatial mapping and localization
- **Obstacle Detection**: Real-time collision avoidance
- **Multi-Agent Systems**: Swarm coordination

### GIS and Spatial Databases
- **Proximity Queries**: Find nearby points of interest
- **Spatial Indexing**: Large-scale geographic data
- **Clustering**: Identify spatial groups

## Why Sparse Hash over Alternatives?

### vs. R-tree (Boost.Geometry)
- **Faster insertions**: O(1) vs O(log n)
- **Simpler incremental updates**: No tree rebalancing
- **Better for dynamic scenes**: Most entities don't move far each frame
- **Toroidal topology**: First-class periodic boundary support

**Choose R-tree when**: Static data, hierarchical bounding volumes needed

### vs. Octree
- **Memory efficient**: Sparse storage vs tree nodes
- **Faster queries**: Direct hash lookup vs tree traversal
- **Easier to implement**: Simpler data structure

**Choose Octree when**: Hierarchical LOD required, data is spatially clustered

### vs. Dense Grid
- **Memory efficient**: 60,000x reduction for large sparse worlds
- **Scalable**: Handles huge worlds without memory explosion

**Choose Dense Grid when**: World is small and densely populated

### vs. kd-tree
- **Dynamic updates**: kd-tree requires rebuild on changes
- **Consistent performance**: No worst-case tree imbalance

**Choose kd-tree when**: Static dataset, kNN queries more important than radius

## Design Highlights

### Generic Programming
```cpp
// Works with ANY dimension
sparse_spatial_hash<Entity, 2> grid_2d;  // 2D
sparse_spatial_hash<Entity, 3> grid_3d;  // 3D
sparse_spatial_hash<Entity, 4> grid_4d;  // Space-time?

// Custom precision
sparse_spatial_hash<Entity, 3, double> precise_grid;

// Custom index type (for >4B entities)
sparse_spatial_hash<Entity, 3, float, uint64_t> large_grid;

// Tune small vector size (default=16, set to 0 to disable)
sparse_spatial_hash<Entity, 3, float, std::size_t, 32> tuned_grid;  // Larger inline storage
```

### Topology Support
```cpp
// Bounded: Clamp to edges (traditional)
grid_config<2> bounded{
    .topology_type = topology::bounded
};

// Toroidal: Periodic wraparound (pac-man physics)
grid_config<2> toroidal{
    .topology_type = topology::toroidal
};

// Infinite: Unbounded growth
grid_config<2> infinite{
    .topology_type = topology::infinite
};
```

### Range Support
```cpp
// Works with any range
std::vector<Particle> vec_particles;
grid.rebuild(vec_particles);

std::list<Particle> list_particles;
grid.rebuild(list_particles);

// Works with sized views (subrange of a contiguous range)
auto first_half = std::ranges::subrange(
    particles.begin(), particles.begin() + particles.size() / 2);
grid.rebuild(first_half);

// For non-sized views (e.g., std::views::filter), materialize first:
std::vector<Particle> moving;
std::ranges::copy_if(particles, std::back_inserter(moving),
    [](const auto& p) { return p.speed > 10.0f; });
grid.rebuild(moving);
```

### Customization Points
```cpp
// Default: Works with array-like types
struct Point { float coords[3]; };  // Automatically supported

// Custom: Specialize for your types
struct MyEntity {
    glm::vec3 position;
    glm::vec3 velocity;
};

template<>
struct spatial::position_accessor<MyEntity, 3> {
    static float get(const MyEntity& e, std::size_t dim) {
        return e.position[dim];
    }
};
```

## Advanced Examples

### Multi-Resolution Grids
```cpp
// Fine grid for collision detection (2-unit cells)
grid_config<3> collision_cfg{
    .cell_size = {2.0f, 2.0f, 2.0f},
    .world_size = {1000.0f, 1000.0f, 1000.0f}
};
sparse_spatial_hash<Particle, 3> collision_grid(collision_cfg);

// Coarse grid for long-range forces (50-unit cells)
grid_config<3> force_cfg{
    .cell_size = {50.0f, 50.0f, 50.0f},
    .world_size = {1000.0f, 1000.0f, 1000.0f}
};
sparse_spatial_hash<Particle, 3> force_grid(force_cfg);

// Different grids for different physics!
```

### Parallel Processing
```cpp
// Process cells in parallel
#pragma omp parallel for
for (const auto& [cell, entities] : grid.cell_contents()) {
    // Thread-safe: Each cell processed independently
    for (std::size_t i = 0; i < entities.size(); ++i) {
        for (std::size_t j = i + 1; j < entities.size(); ++j) {
            handle_collision(entities[i], entities[j]);
        }
    }
}

// Or use standard parallel algorithms
std::for_each(std::execution::par_unseq,
    grid.cells().begin(), grid.cells().end(),
    [&](const auto& cell) { /* process */ });
```

### Non-Uniform Cell Sizes
```cpp
// Rectangular world with different resolutions per axis
grid_config<3> cfg{
    .cell_size = {10.0f, 10.0f, 100.0f},  // Z has coarser resolution
    .world_size = {1000.0f, 1000.0f, 10000.0f}  // Z is 10x larger
};
```

## Building and Installation

### Method 1: CMake FetchContent (Recommended)
Add to your `CMakeLists.txt`:
```cmake
include(FetchContent)

FetchContent_Declare(
  sparse_spatial_hash
  GIT_REPOSITORY https://github.com/queelius/sparse_spatial_hash.git
  GIT_TAG        v2.0.0  # Use the latest release tag
)
FetchContent_MakeAvailable(sparse_spatial_hash)

target_link_libraries(your_target
  PRIVATE sparse_spatial_hash::sparse_spatial_hash)
```

Then just include in your code:
```cpp
#include <spatial/sparse_spatial_hash.hpp>
```

### Method 2: Manual Installation
```bash
# Copy the header to your include path
cp -r include/spatial /usr/local/include/

# Or add to CMake:
target_include_directories(your_target PUBLIC
    /path/to/sparse_spatial_hash/include
)
```

### Method 3: vcpkg (Coming Soon)
```bash
vcpkg install sparse-spatial-hash
```

### Method 4: System Install
```bash
mkdir build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local
sudo cmake --install .
```

Then in your CMakeLists.txt:
```cmake
find_package(sparse_spatial_hash REQUIRED)
target_link_libraries(your_target
  PRIVATE sparse_spatial_hash::sparse_spatial_hash)
```

### Requirements
- **C++20** compiler (GCC 10+, Clang 12+, MSVC 2019+)
- **Standard Library** (no external dependencies)

## Testing

```bash
cd test
mkdir build && cd build
cmake ..
make -j
ctest --output-on-failure
```

Test coverage includes:
- 2D, 3D, and 4D grids
- All topology types
- Incremental updates
- Edge cases (empty grid, single entity, boundary conditions)
- Performance benchmarks

## Benchmarks

```bash
cd benchmark
mkdir build && cd build
cmake ..
make -j
./benchmark_sparse_hash
```

Compares against:
- `std::unordered_map` (naive approach)
- Boost.Geometry R-tree
- Dense grid baseline

## Documentation

Full documentation is available at: **https://queelius.github.io/sparse_spatial_hash/**

- **Tutorial**: Getting started guide with examples
- **API Reference**: Complete API documentation
- **User Guide**: In-depth coverage of features and patterns
- **Performance**: Benchmarks, optimizations, and best practices

## Examples

See `examples/` directory:
- `collision_detection_2d.cpp` - Simple 2D game physics
- `molecular_dynamics_3d.cpp` - N-body particle simulation
- `toroidal_world.cpp` - Periodic boundary demonstration
- `multi_resolution.cpp` - Hierarchical grid setup

## Contributing

Contributions welcome! This library aims for Boost-quality standards:

1. **Code Quality**: Follow Boost coding guidelines
2. **Testing**: Add tests for new features
3. **Documentation**: Update docs and examples
4. **Performance**: Provide benchmarks for changes

## License

Distributed under the [Boost Software License, Version 1.0](http://www.boost.org/LICENSE_1_0.txt).

```
Copyright (C) 2025 Alex Towell

Distributed under the Boost Software License, Version 1.0.
(See accompanying file LICENSE_1_0.txt or copy at
http://www.boost.org/LICENSE_1_0.txt)
```

## Acknowledgments

Extracted from the [DigiStar](https://github.com/spinoza/digistar) physics engine, where it enables efficient collision detection and force calculations for 10M+ particle simulations.

Inspired by:
- Teschner et al. "Optimized Spatial Hashing for Collision Detection" (2003)
- Boost.Geometry spatial indexing
- Morton encoding for cache-friendly iteration

## Future Work

- **GPU Support**: CUDA/OpenCL backend for massive parallelism
- **Lazy Evaluation**: Generator-based query results
- **Distance Metrics**: Support for custom distance functions
- **Adaptive Grids**: Dynamic cell size adjustment
- **Serialization**: Save/load grid state

## See Also

- **Boost.Geometry**: R-tree implementation (tree-based alternative)
- **Boost.MultiIndex**: Multi-dimensional indexing (not spatial)
- **DigiStar**: Original physics engine using this library

---

**Status**: Production-ready, extracted from battle-tested physics engine
**Version**: 2.0.0
**Author**: Alex Towell (PhD Student, CS, SIU)
**Contact**: lex@metafunctor.com | github.com/queelius/sparse_spatial_hash
