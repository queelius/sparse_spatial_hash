/**
 * Example: Toroidal World Demonstration
 *
 * Shows how periodic boundaries work with sparse_spatial_hash.
 * Particles wrap around edges like Pac-Man or Asteroids.
 */

#include <boost/spatial/sparse_spatial_hash.hpp>
#include <iostream>
#include <vector>

struct Entity {
    float x, y;
    int id;
};

template<>
struct boost::spatial::position_accessor<Entity, 2> {
    static float get(const Entity& e, std::size_t dim) {
        return dim == 0 ? e.x : e.y;
    }
};

int main() {
    using namespace boost::spatial;

    std::cout << "=== Toroidal Topology Demonstration ===\n\n";

    constexpr float world_size = 100.0f;
    constexpr float cell_size = 10.0f;

    grid_config<2> cfg{
        .cell_size = {cell_size, cell_size},
        .world_size = {world_size, world_size},
        .topology_type = topology::toroidal
    };

    sparse_spatial_hash<Entity, 2> grid(cfg);

    // Place entities near the edges
    std::vector<Entity> entities = {
        {5.0f, 5.0f, 0},      // Near bottom-left
        {95.0f, 5.0f, 1},     // Near bottom-right
        {5.0f, 95.0f, 2},     // Near top-left
        {95.0f, 95.0f, 3},    // Near top-right
        {50.0f, 50.0f, 4}     // Center
    };

    grid.rebuild(entities);

    std::cout << "World: " << world_size << "x" << world_size << " (toroidal)\n";
    std::cout << "Cell size: " << cell_size << "\n\n";

    std::cout << "Entities:\n";
    for (const auto& e : entities) {
        std::cout << "  ID " << e.id << ": (" << e.x << ", " << e.y << ")\n";
    }

    std::cout << "\n";

    // Query near bottom-left (should find entities across wrap)
    std::cout << "Query radius 20 from (0, 0):\n";
    auto neighbors = grid.query_radius(20.0f, 0.0f, 0.0f);
    std::cout << "  Found entities: ";
    for (auto idx : neighbors) {
        std::cout << entities[idx].id << " ";
    }
    std::cout << "\n  (Should include: 0 near bottom-left, 1/2 wrapped from edges)\n\n";

    // Query center
    std::cout << "Query radius 20 from (50, 50):\n";
    neighbors = grid.query_radius(20.0f, 50.0f, 50.0f);
    std::cout << "  Found entities: ";
    for (auto idx : neighbors) {
        std::cout << entities[idx].id << " ";
    }
    std::cout << "\n  (Should include: 4 at center)\n\n";

    std::cout << "Toroidal topology allows seamless wraparound!\n";

    return 0;
}
