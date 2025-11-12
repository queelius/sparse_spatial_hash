# Sparse Spatial Hash - Tutorial

## Introduction

The sparse spatial hash grid is a data structure for efficient spatial indexing. It's particularly well-suited for:

- **Large, sparse worlds**: Only occupied cells use memory
- **Dynamic scenes**: Incremental updates when entities move
- **Uniform distribution**: Works best when entities are spread out
- **Periodic boundaries**: Native support for toroidal (wraparound) worlds

## Basic Usage

### Step 1: Define Your Entity Type

```cpp
struct Particle {
    float x, y, z;
    float mass;
    int id;
};
```

### Step 2: Specialize Position Accessor

The library needs to know how to extract position from your entity:

```cpp
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
```

**Pro tip**: If your entity stores position as an array or `std::array`, the default implementation works automatically!

```cpp
struct Particle {
    std::array<float, 3> position;  // Works without specialization!
};
```

### Step 3: Configure the Grid

```cpp
using namespace spatial;

grid_config<3> cfg{
    .cell_size = {10.0f, 10.0f, 10.0f},      // 10-unit cells
    .world_size = {1000.0f, 1000.0f, 1000.0f}, // 1000³ world
    .topology_type = topology::bounded          // Clamp to edges
};
```

**Cell size selection**:
- Too small → Many cells, more memory and iteration overhead
- Too large → Many entities per cell, more false positives
- **Sweet spot**: Roughly equal to your query radius

### Step 4: Create the Grid

```cpp
sparse_spatial_hash<Particle, 3> grid(cfg);
```

### Step 5: Add Entities

```cpp
std::vector<Particle> particles = /* ... */;

// Initial build
grid.rebuild(particles);

// Later updates (much faster!)
grid.update(particles);  // Only updates entities that changed cells
```

### Step 6: Query Neighbors

```cpp
// Find all entities within 50 units of position (100, 200, 300)
auto neighbors = grid.query_radius(50.0f, 100.0f, 200.0f, 300.0f);

for (auto idx : neighbors) {
    // Process particles[idx]
}
```

**Important**: `query_radius` returns entities in cells that *intersect* the query sphere. You must filter by exact distance:

```cpp
auto candidates = grid.query_radius(radius, x, y, z);

for (auto idx : candidates) {
    float dx = particles[idx].x - x;
    float dy = particles[idx].y - y;
    float dz = particles[idx].z - z;
    float dist = std::sqrt(dx*dx + dy*dy + dz*dz);

    if (dist < radius) {
        // True neighbor
    }
}
```

### Step 7: Process Pairs

For collision detection or force calculations:

```cpp
grid.for_each_pair(particles, 20.0f,
    [&](std::size_t i, std::size_t j) {
        // i and j are within 20 units (roughly)
        // Guaranteed: i < j (no duplicates)

        handle_collision(particles[i], particles[j]);
    });
```

## Topology Types

### Bounded
```cpp
topology_type = topology::bounded
```
Coordinates are clamped to `[0, world_size]`. Like hitting walls.

**Use for**: Traditional bounded worlds, rooms, arenas

### Toroidal
```cpp
topology_type = topology::toroidal
```
Coordinates wrap around (periodic boundaries). Like Pac-Man or Asteroids.

**Use for**: N-body simulations, infinite-feeling game worlds, molecular dynamics

### Infinite
```cpp
topology_type = topology::infinite
```
No bounds checking. Grid can grow indefinitely.

**Use for**: Procedurally generated worlds, expanding universes

## Incremental Updates

The killer feature for real-time applications:

```cpp
// First frame: full build
grid.rebuild(particles);

// Subsequent frames: incremental update
for (int frame = 0; frame < 1000; ++frame) {
    // Update particle positions
    for (auto& p : particles) {
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        p.z += p.vz * dt;
    }

    // Update grid (only ~1% of particles change cells typically)
    grid.update(particles);  // 40x faster than rebuild!

    // Process collisions
    grid.for_each_pair(particles, collision_radius, handle_collision);
}
```

**Performance**: When only 1% of entities change cells (typical), `update()` is **40x faster** than `rebuild()`.

## Multi-Resolution Grids

Use different grids for different purposes:

```cpp
// Fine grid for precise collisions
grid_config<3> fine_cfg{
    .cell_size = {2.0f, 2.0f, 2.0f},
    .world_size = {1000.0f, 1000.0f, 1000.0f}
};
sparse_spatial_hash<Entity, 3> collision_grid(fine_cfg);

// Coarse grid for AI awareness
grid_config<3> coarse_cfg{
    .cell_size = {50.0f, 50.0f, 50.0f},
    .world_size = {1000.0f, 1000.0f, 1000.0f}
};
sparse_spatial_hash<Entity, 3> awareness_grid(coarse_cfg);

// Rebuild both
collision_grid.rebuild(entities);
awareness_grid.rebuild(entities);

// Fine collision detection
collision_grid.for_each_pair(entities, 2.0f, handle_collision);

// Coarse AI queries
auto nearby = awareness_grid.query_radius(100.0f, player_pos);
```

## Advanced: Custom Allocators

```cpp
#include <boost/pool/pool_alloc.hpp>

// Use pool allocator for better performance
using Alloc = boost::pool_allocator<std::size_t>;

sparse_spatial_hash<
    Particle,
    3,
    float,
    std::size_t,
    cell_hash<3>,
    Alloc
> grid(cfg, Alloc());
```

## Performance Tips

1. **Choose cell size wisely**: Set it to your typical query radius
2. **Use incremental updates**: Don't rebuild every frame
3. **Reserve entities**: Call `grid.reserve_entities(count)` if you know entity count upfront
4. **Toroidal for periodic**: Much faster than manual wraparound checking
5. **Multi-resolution**: Use coarse grids for broad queries, fine for precision

## Common Pitfalls

### ❌ Wrong: Rebuilding every frame
```cpp
for (int frame = 0; frame < 1000; ++frame) {
    update_positions(particles);
    grid.rebuild(particles);  // Slow!
}
```

### ✅ Right: Incremental updates
```cpp
grid.rebuild(particles);  // Once
for (int frame = 0; frame < 1000; ++frame) {
    update_positions(particles);
    grid.update(particles);  // Fast!
}
```

### ❌ Wrong: Cell size too small
```cpp
grid_config<3> cfg{
    .cell_size = {0.1f, 0.1f, 0.1f},  // Too small!
    .world_size = {1000.0f, 1000.0f, 1000.0f}
};
// Result: Millions of cells, high overhead
```

### ✅ Right: Cell size matches query
```cpp
// If typical query radius is 20 units:
grid_config<3> cfg{
    .cell_size = {20.0f, 20.0f, 20.0f},  // Just right
    .world_size = {1000.0f, 1000.0f, 1000.0f}
};
```

### ❌ Wrong: Ignoring cell-based nature
```cpp
auto neighbors = grid.query_radius(5.0f, x, y, z);
// Assume all are within exact distance (WRONG!)
```

### ✅ Right: Filter by exact distance
```cpp
auto candidates = grid.query_radius(5.0f, x, y, z);
for (auto idx : candidates) {
    if (exact_distance(particles[idx], pos) < 5.0f) {
        // True neighbor
    }
}
```

## Next Steps

- **API Reference**: Complete method documentation
- **Examples**: See `examples/` for complete working code
- **Performance Guide**: Tuning for your use case
- **Design Rationale**: Why sparse hash over alternatives

## Questions?

See the [README](../README.md) for comparison with alternatives (R-tree, octree, etc.) and performance benchmarks.
