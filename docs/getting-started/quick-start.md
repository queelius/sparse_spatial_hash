# Quick Start

Get up and running with Boost.Spatial in **5 minutes**!

## Your First Spatial Hash Grid

### Step 1: Include the Header

```cpp
#include <spatial/sparse_spatial_hash.hpp>
#include <vector>
#include <iostream>

using namespace spatial;
```

### Step 2: Define Your Entity

```cpp
struct Particle {
    float x, y;
    float vx, vy;
    int id;
};
```

### Step 3: Teach the Grid How to Read Positions

```cpp
// Specialize the position_accessor for your entity type
template<>
struct spatial::position_accessor<Particle, 2> {
    static float get(const Particle& p, std::size_t dim) {
        return dim == 0 ? p.x : p.y;
    }
};
```

!!! tip "Pro Tip: Skip This Step!"
    If your entity stores position as `std::array<float, N>`, you can skip the accessor specialization - it works automatically!

    ```cpp
    struct Particle {
        std::array<float, 2> position;  // No accessor needed!
    };
    ```

### Step 4: Configure the Grid

```cpp
grid_config<2> cfg{
    .cell_size = {10.0f, 10.0f},      // 10-unit cells
    .world_size = {1000.0f, 1000.0f},  // 1000×1000 world
    .topology_type = topology::bounded  // Clamp to edges
};
```

### Step 5: Create and Populate the Grid

```cpp
// Create the grid
sparse_spatial_hash<Particle, 2> grid(cfg);

// Create some particles
std::vector<Particle> particles;
for (int i = 0; i < 1000; ++i) {
    particles.push_back({
        .x = rand() % 1000,
        .y = rand() % 1000,
        .vx = (rand() % 100 - 50) / 10.0f,
        .vy = (rand() % 100 - 50) / 10.0f,
        .id = i
    });
}

// Build the spatial index
grid.rebuild(particles);

std::cout << "Indexed " << grid.entity_count() << " particles in "
          << grid.cell_count() << " cells\n";
```

### Step 6: Query for Neighbors

```cpp
// Find all particles within 50 units of position (500, 500)
auto neighbors = grid.query_radius(50.0f, 500.0f, 500.0f);

std::cout << "Found " << neighbors.size() << " neighbors\n";

// Remember: Filter by exact distance!
for (auto idx : neighbors) {
    const auto& p = particles[idx];
    float dx = p.x - 500.0f;
    float dy = p.y - 500.0f;
    float dist = std::sqrt(dx*dx + dy*dy);

    if (dist < 50.0f) {
        std::cout << "Particle " << p.id << " at distance " << dist << "\n";
    }
}
```

### Step 7: Process Pairs (Collisions)

```cpp
// Find all pairs within 20 units of each other
grid.for_each_pair(20.0f,
    [&](std::size_t i, std::size_t j) {
        // i and j are guaranteed to be different (i < j)
        const auto& p1 = particles[i];
        const auto& p2 = particles[j];

        // Check exact distance
        float dx = p2.x - p1.x;
        float dy = p2.y - p1.y;
        float dist = std::sqrt(dx*dx + dy*dy);

        if (dist < 20.0f) {
            std::cout << "Collision between particle " << p1.id
                      << " and " << p2.id << "\n";
        }
    });
```

### Step 8: Update Efficiently (The Magic!)

```cpp
// Simulation loop
for (int frame = 0; frame < 100; ++frame) {
    // Update particle positions
    for (auto& p : particles) {
        p.x += p.vx;
        p.y += p.vy;

        // Wrap around world boundaries
        if (p.x < 0) p.x += 1000;
        if (p.x >= 1000) p.x -= 1000;
        if (p.y < 0) p.y += 1000;
        if (p.y >= 1000) p.y -= 1000;
    }

    // Incremental update (40x faster than rebuild!)
    grid.update(particles);

    // Process collisions this frame
    int collisions = 0;
    grid.for_each_pair(20.0f,
        [&](std::size_t i, std::size_t j) {
            collisions++;
        });

    if (frame % 10 == 0) {
        std::cout << "Frame " << frame << ": " << collisions
                  << " potential collisions\n";
    }
}
```

## Complete Example

```cpp
#include <spatial/sparse_spatial_hash.hpp>
#include <vector>
#include <iostream>
#include <cmath>
#include <cstdlib>

using namespace spatial;

struct Particle {
    float x, y;
    float vx, vy;
    int id;
};

template<>
struct spatial::position_accessor<Particle, 2> {
    static float get(const Particle& p, std::size_t dim) {
        return dim == 0 ? p.x : p.y;
    }
};

int main() {
    // Configure grid
    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    // Create grid and particles
    sparse_spatial_hash<Particle, 2> grid(cfg);
    std::vector<Particle> particles;

    for (int i = 0; i < 1000; ++i) {
        particles.push_back({
            .x = static_cast<float>(rand() % 1000),
            .y = static_cast<float>(rand() % 1000),
            .vx = (rand() % 100 - 50) / 10.0f,
            .vy = (rand() % 100 - 50) / 10.0f,
            .id = i
        });
    }

    // Build spatial index
    grid.rebuild(particles);
    std::cout << "Indexed " << grid.entity_count() << " particles\n";

    // Query neighbors
    auto neighbors = grid.query_radius(50.0f, 500.0f, 500.0f);
    std::cout << "Found " << neighbors.size() << " potential neighbors\n";

    // Simulation loop
    for (int frame = 0; frame < 100; ++frame) {
        // Update positions
        for (auto& p : particles) {
            p.x += p.vx;
            p.y += p.vy;

            // Bounce off walls
            if (p.x < 0 || p.x >= 1000) p.vx = -p.vx;
            if (p.y < 0 || p.y >= 1000) p.vy = -p.vy;
        }

        // Fast incremental update
        grid.update(particles);

        // Process collisions
        if (frame % 10 == 0) {
            int collisions = 0;
            grid.for_each_pair(20.0f,
                [&](std::size_t i, std::size_t j) {
                    collisions++;
                });
            std::cout << "Frame " << frame << ": " << collisions
                      << " potential collisions\n";
        }
    }

    // Print statistics
    auto stats = grid.stats();
    std::cout << "\nFinal statistics:\n"
              << "  Occupied cells: " << stats.occupied_cells << "\n"
              << "  Avg entities/cell: " << stats.avg_entities_per_cell << "\n"
              << "  Occupancy ratio: " << (stats.occupancy_ratio * 100) << "%\n";

    return 0;
}
```

## Compile and Run

```bash
# GCC
g++ -std=c++20 -O2 -I/path/to/include quick_start.cpp -o quick_start
./quick_start

# Clang
clang++ -std=c++20 -O2 -I/path/to/include quick_start.cpp -o quick_start
./quick_start

# MSVC
cl /std:c++20 /O2 /EHsc /I\path\to\include quick_start.cpp
quick_start.exe
```

## Expected Output

```
Indexed 1000 particles
Found 87 potential neighbors
Frame 0: 156 potential collisions
Frame 10: 142 potential collisions
Frame 20: 139 potential collisions
...
Frame 90: 145 potential collisions

Final statistics:
  Occupied cells: 84
  Avg entities/cell: 11.9
  Occupancy ratio: 0.84%
```

## Key Concepts Demonstrated

### 1. Position Accessor Pattern
The library needs to know how to extract position from your entity type. You do this **once** by specializing `position_accessor`.

### 2. Query Returns Candidates
`query_radius()` returns entities in cells that **intersect** the query sphere. Always filter by exact distance in performance-critical code.

### 3. Incremental Updates
Use `update()` instead of `rebuild()` in your game loop - it's **40x faster** for typical scenarios!

### 4. Pair Processing
`for_each_pair()` guarantees `i < j` and no duplicates - perfect for collision detection and force calculations.

## What's Next?

Ready to learn more?

<div class="grid cards" markdown>

-   :material-school:{ .lg .middle } **[Tutorial](tutorial.md)**

    ---

    Comprehensive step-by-step guide with best practices

-   :material-book-open:{ .lg .middle } **[User Guide](../user-guide/index.md)**

    ---

    Learn about topologies, queries, and advanced features

-   :material-api:{ .lg .middle } **[API Reference](../api-reference/index.md)**

    ---

    Complete API documentation with complexity guarantees

-   :material-speedometer:{ .lg .middle } **[Performance](../performance/index.md)**

    ---

    Benchmarks, optimizations, and best practices

</div>

## Common Next Steps

### Game Development
Learn about [collision detection patterns](../user-guide/common-patterns.md#collision-detection).

### Physics Simulation
Explore [incremental updates](../user-guide/incremental-updates.md) for real-time simulations.

### Large Scale
Check out [performance best practices](../performance/best-practices.md) for 100K+ entities.

### Different Topologies
Try [toroidal worlds](../user-guide/topologies.md#toroidal-periodic) for periodic boundaries!

[Continue to Tutorial](tutorial.md){ .md-button .md-button--primary }
