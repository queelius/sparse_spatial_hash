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
grid.update_one(index, entity);

// Querying
auto neighbors = grid.query_radius(r, x, y, z);
grid.for_each_in_radius(radius, position, callback);
grid.for_each_pair(radius, callback);

// Statistics
auto stats = grid.stats();
size_t count = grid.cell_count();
bool consistent = grid.validate();
```

`update_one()` preserves entity membership if allocation fails. Bulk `update()`
may retain earlier successful moves when a later move fails, while keeping the
grid consistent.

Callback radius queries and pair iteration allocate no memory themselves and
visit each candidate entity or unordered pair at most once, including at bounded
edges and across toroidal wraps. Callbacks may allocate or throw; callback
exceptions propagate to the caller.

`validate()` checks for missing, duplicate, out-of-range, and incorrectly bucketed
indices in O(entity count + occupied cells) time. It uses one temporary byte per
entity through the configured allocator and may throw on allocation failure;
the grid remains unchanged. It is not declared `noexcept`.

[View Full API](sparse-spatial-hash.md){ .md-button .md-button--primary }
