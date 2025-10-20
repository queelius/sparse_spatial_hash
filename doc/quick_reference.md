# Sparse Spatial Hash - Quick Reference

## Include
```cpp
#include <boost/spatial/sparse_spatial_hash.hpp>
using namespace boost::spatial;
```

## Basic Setup

### 1. Define Position Accessor
```cpp
template<>
struct position_accessor<MyEntity, 3> {
    static float get(const MyEntity& e, std::size_t dim) {
        return e.position[dim];  // or e.x, e.y, e.z
    }
};
```

### 2. Configure Grid
```cpp
grid_config<3> cfg{
    .cell_size = {10.0f, 10.0f, 10.0f},
    .world_size = {1000.0f, 1000.0f, 1000.0f},
    .topology_type = topology::toroidal  // or bounded, infinite
};
```

### 3. Create Grid
```cpp
sparse_spatial_hash<MyEntity, 3> grid(cfg);
```

## Operations

### Build/Update
```cpp
grid.rebuild(entities);        // Initial O(n)
grid.update(entities);         // Incremental O(k), k ≈ 1% typical
grid.clear();                  // Empty grid
grid.reserve_entities(count);  // Pre-allocate tracking
```

### Queries
```cpp
// Radius query
auto neighbors = grid.query_radius(50.0f, x, y, z);

// Cell entities
auto cell_ents = grid.cell_entities(cell_coord);

// Iterate cells
for (auto cell : grid.cells()) { /* ... */ }

// Iterate cell contents
for (auto& [cell, entities] : grid.cell_contents()) { /* ... */ }
```

### Pair Processing
```cpp
grid.for_each_pair(entities, radius,
    [&](std::size_t i, std::size_t j) {
        // Process entities[i] and entities[j]
        // Guaranteed: i < j (no duplicates)
    });
```

### Statistics
```cpp
auto stats = grid.stats();
// stats.occupied_cells
// stats.total_entities
// stats.max_entities_per_cell
// stats.avg_entities_per_cell
// stats.occupancy_ratio

grid.cell_count();   // Number of occupied cells
grid.entity_count(); // Number of entities tracked
grid.empty();        // True if no entities
```

## Type Aliases

```cpp
// 2D grid
sparse_spatial_hash_2d<Entity> grid_2d(cfg);

// 3D grid
sparse_spatial_hash_3d<Entity> grid_3d(cfg);

// Custom precision
sparse_spatial_hash<Entity, 3, double> precise_grid(cfg);
```

## Topology Types

```cpp
topology::bounded   // Clamp to [0, world_size]
topology::toroidal  // Wrap around (periodic)
topology::infinite  // Unbounded growth
```

## Configuration

```cpp
grid_config<3> cfg{
    .cell_size = {10.0f, 10.0f, 10.0f},     // Per-dimension
    .world_size = {1000.0f, 1000.0f, 1000.0f},
    .topology_type = topology::bounded
};

// Or uniform cell size constructor
sparse_spatial_hash<Entity, 3> grid(
    10.0f,                          // cell_size
    {1000.0f, 1000.0f, 1000.0f},   // world_size
    topology::toroidal
);
```

## Complexity Guarantees

| Operation | Time | Space |
|-----------|------|-------|
| rebuild() | O(n) | O(cells + n) |
| update() | O(k), k ≈ 0.01n | O(cells + n) |
| query_radius() | O(m) | O(m) |
| for_each_pair() | O(c × k²) | O(1) |

## Performance Tips

### ✅ Do
- Use incremental `update()` instead of `rebuild()`
- Set cell size ≈ query radius
- Call `reserve_entities(count)` if count known
- Use toroidal for periodic boundaries

### ❌ Don't
- Make cell size too small (overhead)
- Make cell size too large (false positives)
- Rebuild every frame (use update!)
- Assume query results are exact (filter by distance)

## Common Patterns

### Collision Detection
```cpp
grid.for_each_pair(particles, collision_radius,
    [&](std::size_t i, std::size_t j) {
        if (exact_distance(particles[i], particles[j]) < collision_radius) {
            handle_collision(particles[i], particles[j]);
        }
    });
```

### Neighbor List
```cpp
auto candidates = grid.query_radius(cutoff, position);
std::vector<std::size_t> neighbors;

for (auto idx : candidates) {
    if (exact_distance(entities[idx], position) < cutoff) {
        neighbors.push_back(idx);
    }
}
```

### Multi-Resolution
```cpp
sparse_spatial_hash<Entity, 3> fine_grid(fine_cfg);   // 2-unit cells
sparse_spatial_hash<Entity, 3> coarse_grid(coarse_cfg); // 50-unit cells

fine_grid.rebuild(entities);    // Precise collisions
coarse_grid.rebuild(entities);  // Broad awareness

fine_grid.for_each_pair(entities, 2.0f, handle_collision);
auto nearby = coarse_grid.query_radius(100.0f, player_pos);
```

### Simulation Loop
```cpp
grid.rebuild(entities);  // Once at startup

for (int frame = 0; frame < num_frames; ++frame) {
    // Update positions
    for (auto& e : entities) {
        e.position += e.velocity * dt;
    }

    // Fast incremental update
    grid.update(entities);

    // Process interactions
    grid.for_each_pair(entities, radius, handle_interaction);
}
```

## Customization

### Custom Hash Function
```cpp
template<std::size_t Dims>
struct my_hash {
    std::size_t operator()(const cell_coord<Dims>& cell) const {
        // Custom hash implementation
    }
};

sparse_spatial_hash<Entity, 3, float, std::size_t, my_hash<3>> grid(cfg);
```

### Custom Allocator
```cpp
using Alloc = boost::pool_allocator<std::size_t>;
sparse_spatial_hash<Entity, 3, float, std::size_t, cell_hash<3>, Alloc>
    grid(cfg, Alloc());
```

## Debugging

### Print Statistics
```cpp
auto stats = grid.stats();
std::cout << "Cells: " << stats.occupied_cells << "\n"
          << "Entities: " << stats.total_entities << "\n"
          << "Avg/cell: " << stats.avg_entities_per_cell << "\n"
          << "Max/cell: " << stats.max_entities_per_cell << "\n"
          << "Occupancy: " << (stats.occupancy_ratio * 100) << "%\n";
```

### Visualize Cell Distribution
```cpp
for (const auto& [cell, entities] : grid.cell_contents()) {
    std::cout << "Cell (" << cell[0] << "," << cell[1] << "," << cell[2]
              << "): " << entities.size() << " entities\n";
}
```

## Error Messages

### Common Issues

**"has_position constraint not satisfied"**
→ Did you specialize `position_accessor` for your type?

**"no matching function for call to rebuild"**
→ Is your container a valid range?

**Large memory usage**
→ Cell size too small? Check `stats.occupancy_ratio`

**Slow queries**
→ Cell size too large? Many false positives?

## See Also

- **Tutorial**: `doc/tutorial.md` - Full step-by-step guide
- **README**: `README.md` - Overview and performance comparison
- **Examples**: `examples/` - Complete working programs
- **Tests**: `test/` - Unit test examples
