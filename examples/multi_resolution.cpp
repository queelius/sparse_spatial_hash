/**
 * Example: Multi-Resolution Grid Hierarchy
 *
 * Shows how to use multiple grids with different cell sizes
 * for different types of queries (like game physics).
 */

#include <boost/spatial/sparse_spatial_hash.hpp>
#include <iostream>
#include <vector>

struct GameObject {
    float x, y, z;
    int type;  // 0=small, 1=medium, 2=large
};

template<>
struct boost::spatial::position_accessor<GameObject, 3> {
    static float get(const GameObject& obj, std::size_t dim) {
        switch(dim) {
            case 0: return obj.x;
            case 1: return obj.y;
            case 2: return obj.z;
            default: return 0.0f;
        }
    }
};

int main() {
    using namespace boost::spatial;

    std::cout << "=== Multi-Resolution Grid Hierarchy ===\n\n";

    constexpr float world_size = 1000.0f;

    // Fine grid for precise collision detection (bullets, small objects)
    grid_config<3> fine_cfg{
        .cell_size = {5.0f, 5.0f, 5.0f},
        .world_size = {world_size, world_size, world_size},
        .topology_type = topology::bounded
    };
    sparse_spatial_hash<GameObject, 3> fine_grid(fine_cfg);

    // Medium grid for character interactions
    grid_config<3> medium_cfg{
        .cell_size = {20.0f, 20.0f, 20.0f},
        .world_size = {world_size, world_size, world_size},
        .topology_type = topology::bounded
    };
    sparse_spatial_hash<GameObject, 3> medium_grid(medium_cfg);

    // Coarse grid for long-range queries (AI awareness, LOD)
    grid_config<3> coarse_cfg{
        .cell_size = {100.0f, 100.0f, 100.0f},
        .world_size = {world_size, world_size, world_size},
        .topology_type = topology::bounded
    };
    sparse_spatial_hash<GameObject, 3> coarse_grid(coarse_cfg);

    // Create mixed objects
    std::vector<GameObject> objects = {
        {100.0f, 100.0f, 100.0f, 0},   // Small
        {105.0f, 105.0f, 105.0f, 0},   // Small nearby
        {200.0f, 200.0f, 200.0f, 1},   // Medium
        {500.0f, 500.0f, 500.0f, 2},   // Large
        {700.0f, 700.0f, 700.0f, 2},   // Large distant
    };

    // Build all grids
    fine_grid.rebuild(objects);
    medium_grid.rebuild(objects);
    coarse_grid.rebuild(objects);

    std::cout << "World: " << world_size << "³\n\n";

    std::cout << "Fine grid (5-unit cells):\n";
    auto stats = fine_grid.stats();
    std::cout << "  Occupied cells: " << stats.occupied_cells << "\n";
    std::cout << "  Occupancy: " << (stats.occupancy_ratio * 100) << "%\n\n";

    std::cout << "Medium grid (20-unit cells):\n";
    stats = medium_grid.stats();
    std::cout << "  Occupied cells: " << stats.occupied_cells << "\n";
    std::cout << "  Occupancy: " << (stats.occupancy_ratio * 100) << "%\n\n";

    std::cout << "Coarse grid (100-unit cells):\n";
    stats = coarse_grid.stats();
    std::cout << "  Occupied cells: " << stats.occupied_cells << "\n";
    std::cout << "  Occupancy: " << (stats.occupancy_ratio * 100) << "%\n\n";

    // Example queries
    std::cout << "Queries from position (100, 100, 100):\n\n";

    std::cout << "  Fine grid (radius 10):\n";
    auto results = fine_grid.query_radius(10.0f, 100.0f, 100.0f, 100.0f);
    std::cout << "    Found " << results.size() << " objects (precise collisions)\n";

    std::cout << "  Medium grid (radius 50):\n";
    results = medium_grid.query_radius(50.0f, 100.0f, 100.0f, 100.0f);
    std::cout << "    Found " << results.size() << " objects (character interactions)\n";

    std::cout << "  Coarse grid (radius 300):\n";
    results = coarse_grid.query_radius(300.0f, 100.0f, 100.0f, 100.0f);
    std::cout << "    Found " << results.size() << " objects (AI awareness)\n";

    std::cout << "\nUse case:\n";
    std::cout << "  - Fine grid: Bullet collisions, precise interactions\n";
    std::cout << "  - Medium grid: Character proximity, melee combat\n";
    std::cout << "  - Coarse grid: AI awareness, rendering culling, LOD selection\n";

    return 0;
}
