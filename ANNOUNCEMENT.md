# Announcing sparse_spatial_hash v2.0.0

A **generic N-dimensional sparse spatial hash grid** for high-performance spatial indexing and neighbor queries.

## 🚀 What is it?

`sparse_spatial_hash` is a production-ready, header-only C++20 library providing efficient spatial data structures for:
- Game development (collision detection, AI pathfinding, rendering culling)
- Scientific computing (N-body simulations, molecular dynamics, SPH fluids)
- Robotics (SLAM, obstacle detection, swarm coordination)
- GIS and spatial databases (proximity queries, spatial indexing)

## ✨ Key Features

- **N-Dimensional**: Works with 2D, 3D, 4D, or any dimension count
- **Sparse Storage**: 60,000x memory reduction vs dense grids (hash-based, only occupied cells)
- **Multiple Topologies**: Bounded, toroidal (periodic), and infinite spaces
- **Incremental Updates**: O(k) where k ≈ 1% of entities (40x faster than rebuild)
- **STL-Compatible**: Works with ranges, concepts, and standard algorithms
- **Header-Only**: Single include, no linking required
- **Zero-Overhead**: Generic programming with compile-time polymorphism
- **Small Vector Optimization**: 5-40% performance improvement

## 📦 Installation

### CMake FetchContent (Recommended)
```cmake
include(FetchContent)
FetchContent_Declare(
  sparse_spatial_hash
  GIT_REPOSITORY https://github.com/queelius/sparse_spatial_hash.git
  GIT_TAG v2.0.0
)
FetchContent_MakeAvailable(sparse_spatial_hash)
target_link_libraries(your_target PRIVATE sparse_spatial_hash::sparse_spatial_hash)
```

### vcpkg (Coming Soon)
```bash
vcpkg install sparse-spatial-hash
```

## 🎯 Quick Example

```cpp
#include <spatial/sparse_spatial_hash.hpp>

struct Particle {
    float x, y, z;
    float vx, vy, vz;
};

// Position accessor (one-time setup)
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

    std::vector<Particle> particles(10000);
    // ... initialize particles ...

    // Build spatial index
    grid.rebuild(particles);

    // Query neighbors within 50 units
    auto neighbors = grid.query_radius(50.0f, 100.0f, 200.0f, 300.0f);

    // Process all pairs within 20 units
    grid.for_each_pair(particles, 20.0f,
        [&](std::size_t i, std::size_t j) {
            // Compute interaction between particles[i] and particles[j]
        });

    // Efficient incremental update (only moved entities)
    grid.update(particles);  // Much faster than rebuild()
}
```

## 🎬 Real-World Performance

From DigiStar physics engine (10M particles, 10000³ world):

| Operation | Time | Notes |
|-----------|------|-------|
| **Rebuild** | ~80ms | Full grid reconstruction |
| **Update** | ~2ms | When 1% of particles move cells |
| **Query** | <1ms | Radius query with typical cell occupancy |
| **Collision** | ~150ms | All-pairs with 20-unit radius |

**Memory**: 100MB grid + 80MB tracking (vs 6TB for dense grid)

## 🆚 Why Sparse Hash over Alternatives?

### vs. R-tree (Boost.Geometry)
- ✅ Faster insertions: O(1) vs O(log n)
- ✅ Simpler incremental updates: No tree rebalancing
- ✅ Native toroidal topology support
- 📊 Choose R-tree when: Static data, hierarchical queries needed

### vs. Octree
- ✅ More memory efficient with sparse data
- ✅ Faster queries: Direct hash lookup vs tree traversal
- 📊 Choose Octree when: Hierarchical LOD required

### vs. Dense Grid
- ✅ 60,000x memory reduction for large sparse worlds
- ✅ Scales to huge worlds without memory explosion
- 📊 Choose Dense when: Small, densely populated world

### vs. kd-tree
- ✅ Dynamic updates without rebuild
- ✅ Consistent O(1) insertion performance
- 📊 Choose kd-tree when: Static dataset, kNN > radius queries

## 🔬 Technical Highlights

- **Morton Encoding (Z-order curve)**: Cache-friendly spatial locality
- **Small Vector Optimization**: Inline storage for typical cells (≤16 entities)
- **Trait-based Customization**: Non-intrusive `position_accessor`
- **Exception Safety**: Nothrow queries, strong/basic guarantees documented
- **Comprehensive Testing**: 54 tests, 326 assertions, 100% pass rate

## 📊 Memory Efficiency

For **10 million particles** in **10,000³ world**:
- Dense Grid: **6 TB** (stores every cell)
- Octree: 400 MB (tree structure overhead)
- R-tree: 400 MB (balancing overhead)
- **Sparse Hash: 100 MB** ✨ (only occupied cells)

## 🛠️ Requirements

- **C++20** compiler (GCC 10+, Clang 12+, MSVC 2019+)
- **Standard Library** (no external dependencies)
- Header-only, no linking required

## 📚 Links

- **GitHub**: https://github.com/queelius/sparse_spatial_hash
- **Documentation**: https://github.com/queelius/sparse_spatial_hash/blob/main/README.md
- **Examples**: https://github.com/queelius/sparse_spatial_hash/tree/main/examples
- **License**: Boost Software License 1.0

## 🙏 Feedback Welcome!

This library was extracted from the [DigiStar](https://github.com/spinoza/digistar) physics engine after handling 10M+ particle simulations in production. I'd love to hear your feedback, use cases, and contributions!

**What's your spatial data structure of choice? When would you use a sparse hash grid?**

---

*Released under the Boost Software License 1.0*
