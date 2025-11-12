# API Reference

Complete API documentation for Boost.Spatial - Sparse Spatial Hash.

## Core Classes

- **[grid_config](grid-config.md)** - Configuration for spatial grid
- **[sparse_spatial_hash](sparse-spatial-hash.md)** - Main grid class
- **[position_accessor](position-accessor.md)** - Position extraction trait

## Operations

- **[Queries & Iteration](queries.md)** - Finding and iterating entities
- **[Statistics](statistics.md)** - Grid metrics and debugging
- **[Type Aliases](type-aliases.md)** - Convenience typedefs

## Quick Reference

```cpp
#include <spatial/sparse_spatial_hash.hpp>
using namespace spatial;

// Configuration
grid_config<3> cfg{...};

// Grid creation
sparse_spatial_hash<Entity, 3> grid(cfg);

// Building
grid.rebuild(entities);
grid.update(entities);

// Querying
auto neighbors = grid.query_radius(r, x, y, z);
grid.for_each_pair(entities, radius, callback);

// Statistics
auto stats = grid.stats();
size_t count = grid.cell_count();
```

[View Full API](sparse-spatial-hash.md){ .md-button .md-button--primary }
