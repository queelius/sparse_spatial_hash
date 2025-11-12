# Getting Started

Welcome to Boost.Spatial - Sparse Spatial Hash! This section will help you get up and running quickly.

## What You'll Learn

In this getting started guide, you'll learn how to:

1. **[Install](installation.md)** the library in your project
2. **[Quick Start](quick-start.md)** with a working example in 5 minutes
3. **[Tutorial](tutorial.md)** - comprehensive step-by-step guide

## Prerequisites

Before you begin, ensure you have:

- **C++20 Compiler**: GCC 10+, Clang 12+, or MSVC 2019+
- **CMake** (optional): 3.15+ for build system integration
- **Standard Library**: No external dependencies required

## Learning Path

### New to Spatial Indexing?

If you're new to spatial indexing concepts, we recommend:

1. Start with [Core Concepts](../user-guide/core-concepts.md) to understand spatial hashing
2. Try the [Quick Start](quick-start.md) example
3. Follow the complete [Tutorial](tutorial.md)
4. Explore [Common Patterns](../user-guide/common-patterns.md)

### Experienced with Spatial Data Structures?

If you're familiar with R-trees, octrees, or other spatial structures:

1. Check out the [Performance Comparison](../performance/comparison.md)
2. Jump to [Installation](installation.md)
3. Review the [API Reference](../api-reference/index.md)
4. See [Advanced Usage](../user-guide/advanced-usage.md)

## Quick Links

<div class="grid cards" markdown>

-   :material-download:{ .lg .middle } **Installation**

    ---

    Get the library installed in your project

    [Install Now](installation.md){ .md-button }

-   :material-rocket-launch:{ .lg .middle } **Quick Start**

    ---

    Working example in 5 minutes

    [Start Coding](quick-start.md){ .md-button }

-   :material-school:{ .lg .middle } **Tutorial**

    ---

    Comprehensive step-by-step guide

    [Learn More](tutorial.md){ .md-button }

-   :material-code-braces:{ .lg .middle } **Examples**

    ---

    Full working examples on GitHub

    [View Examples](https://github.com/queelius/sparse_spatial_hash/tree/main/examples){ .md-button }

</div>

## What is a Sparse Spatial Hash?

A **sparse spatial hash grid** is a data structure that divides space into a grid of cells and uses a hash map to store only the occupied cells. This makes it:

- **Memory efficient**: Only occupied cells use memory
- **Fast**: O(1) insertion and cell lookup
- **Dynamic**: Handles moving entities efficiently
- **Simple**: Easier to implement than trees

### When to Use It?

Sparse spatial hashing excels when you have:

- **Large, sparse worlds** (entities scattered across space)
- **Dynamic scenes** (entities frequently moving)
- **Uniform distribution** (entities spread relatively evenly)
- **Real-time requirements** (games, simulations needing <16ms frame times)

### When to Use Alternatives?

Consider other data structures if you have:

- **Dense, small worlds** → Use dense grid
- **Static data** → Use R-tree or kd-tree
- **Hierarchical queries** → Use octree or quadtree
- **Non-uniform clustering** → Use R-tree

[Compare Data Structures](../performance/comparison.md){ .md-button }

## Common Use Cases

### Game Development

```cpp
// Collision detection
grid.for_each_pair(entities, collision_radius,
    [](size_t i, size_t j) {
        resolve_collision(entities[i], entities[j]);
    });
```

### Physics Simulation

```cpp
// Force calculations
for (auto idx : grid.query_radius(cutoff, particle.position)) {
    compute_forces(particle, particles[idx]);
}
```

### AI and Pathfinding

```cpp
// Find nearby entities for awareness
auto nearby = grid.query_radius(vision_radius, agent.position);
for (auto idx : nearby) {
    process_awareness(agent, entities[idx]);
}
```

## Help and Support

Need help? Here's where to go:

- **API Questions**: See [API Reference](../api-reference/index.md)
- **Performance Questions**: See [Performance Guide](../performance/index.md)
- **Bug Reports**: [GitHub Issues](https://github.com/queelius/sparse_spatial_hash/issues)
- **Discussions**: [GitHub Discussions](https://github.com/queelius/sparse_spatial_hash/discussions)

## Ready to Begin?

[Install the Library](installation.md){ .md-button .md-button--primary }
[View Quick Start](quick-start.md){ .md-button }
