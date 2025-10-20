/**
 * Basic unit tests for sparse_spatial_hash using Catch2
 *
 * Tests fundamental operations: construction, insertion, queries
 */

#include <boost/spatial/sparse_spatial_hash.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>

using namespace boost::spatial;

// Simple test particle
struct Particle {
    float x, y, z;
    int id;
};

// Position accessor for 3D
template<>
struct boost::spatial::position_accessor<Particle, 3> {
    static float get(const Particle& p, std::size_t dim) {
        switch(dim) {
            case 0: return p.x;
            case 1: return p.y;
            case 2: return p.z;
            default: return 0.0f;
        }
    }
};

// Position accessor for 2D
template<>
struct boost::spatial::position_accessor<Particle, 2> {
    static float get(const Particle& p, std::size_t dim) {
        return dim == 0 ? p.x : p.y;
    }
};

TEST_CASE("Grid construction", "[basic][construction]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);

    REQUIRE(grid.empty());
    REQUIRE(grid.cell_count() == 0);
    REQUIRE(grid.entity_count() == 0);
}

TEST_CASE("Grid rebuild", "[basic][rebuild]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);

    SECTION("Empty rebuild") {
        std::vector<Particle> particles;
        grid.rebuild(particles);

        REQUIRE(grid.empty());
    }

    SECTION("Single particle") {
        std::vector<Particle> particles = {
            {5.0f, 5.0f, 5.0f, 0}
        };

        grid.rebuild(particles);

        REQUIRE_FALSE(grid.empty());
        REQUIRE(grid.cell_count() == 1);
    }

    SECTION("Multiple particles in different cells") {
        std::vector<Particle> particles = {
            {5.0f, 5.0f, 5.0f, 0},
            {15.0f, 15.0f, 15.0f, 1},
            {25.0f, 25.0f, 25.0f, 2}
        };

        grid.rebuild(particles);

        REQUIRE_FALSE(grid.empty());
        REQUIRE(grid.cell_count() == 3);  // Each in different cell
    }

    SECTION("Multiple particles in same cell") {
        std::vector<Particle> particles = {
            {5.0f, 5.0f, 5.0f, 0},
            {6.0f, 6.0f, 6.0f, 1},  // Same cell as first
        };

        grid.rebuild(particles);

        REQUIRE(grid.cell_count() == 1);  // Both in same cell
    }
}

TEST_CASE("Query radius", "[basic][query]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);

    std::vector<Particle> particles = {
        {0.0f, 0.0f, 0.0f, 0},    // At origin
        {5.0f, 0.0f, 0.0f, 1},    // Close
        {50.0f, 0.0f, 0.0f, 2},   // Far
    };

    grid.rebuild(particles);

    SECTION("Small radius query") {
        auto results = grid.query_radius(8.0f, 0.0f, 0.0f, 0.0f);

        // Should find at least particles 0 and 1
        // (might include more due to conservative cell-based query)
        REQUIRE(results.size() >= 2);

        bool found_0 = false, found_1 = false;
        for (auto idx : results) {
            if (idx == 0) found_0 = true;
            if (idx == 1) found_1 = true;
        }

        REQUIRE(found_0);
        REQUIRE(found_1);
    }

    SECTION("Large radius query") {
        auto results = grid.query_radius(60.0f, 0.0f, 0.0f, 0.0f);

        // Should find all particles
        REQUIRE(results.size() >= 3);
    }

    SECTION("Empty grid query") {
        sparse_spatial_hash<Particle, 3> empty_grid(cfg);
        auto results = empty_grid.query_radius(10.0f, 0.0f, 0.0f, 0.0f);

        REQUIRE(results.empty());
    }
}

TEST_CASE("Incremental update", "[basic][update]") {
    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 2> grid(cfg);

    std::vector<Particle> particles = {
        {5.0f, 5.0f, 0.0f, 0},
        {15.0f, 15.0f, 0.0f, 1}
    };

    grid.rebuild(particles);
    REQUIRE(grid.cell_count() == 2);

    SECTION("Move particle to different cell") {
        particles[0].x = 16.0f;
        particles[0].y = 16.0f;

        grid.update(particles);

        REQUIRE(grid.cell_count() == 1);  // Both in same cell now
    }

    SECTION("No movement") {
        grid.update(particles);

        REQUIRE(grid.cell_count() == 2);  // Unchanged
    }

    SECTION("Move both particles") {
        particles[0].x = 25.0f;
        particles[0].y = 25.0f;
        particles[1].x = 35.0f;
        particles[1].y = 35.0f;

        grid.update(particles);

        REQUIRE(grid.cell_count() == 2);  // Still in different cells
    }
}

TEST_CASE("For each pair", "[basic][pairs]") {
    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 2> grid(cfg);

    std::vector<Particle> particles = {
        {5.0f, 5.0f, 0.0f, 0},
        {6.0f, 6.0f, 0.0f, 1},
        {50.0f, 50.0f, 0.0f, 2}
    };

    grid.rebuild(particles);

    SECTION("Count pairs within radius") {
        int pair_count = 0;
        grid.for_each_pair(particles, 20.0f,
            [&](std::size_t i, std::size_t j) {
                pair_count++;
                REQUIRE(i < j);  // No duplicates, ordered
            });

        // Should find at least the (0,1) pair
        REQUIRE(pair_count >= 1);
    }

    SECTION("Empty grid produces no pairs") {
        sparse_spatial_hash<Particle, 2> empty_grid(cfg);
        std::vector<Particle> no_particles;
        empty_grid.rebuild(no_particles);

        int pair_count = 0;
        empty_grid.for_each_pair(no_particles, 20.0f,
            [&](std::size_t, std::size_t) { pair_count++; });

        REQUIRE(pair_count == 0);
    }
}

TEST_CASE("Grid statistics", "[basic][stats]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);

    SECTION("Empty grid statistics") {
        auto stats = grid.stats();

        REQUIRE(stats.occupied_cells == 0);
        REQUIRE(stats.total_entities == 0);
        REQUIRE(stats.max_entities_per_cell == 0);
        REQUIRE(stats.avg_entities_per_cell == 0.0f);
    }

    SECTION("Populated grid statistics") {
        std::vector<Particle> particles;
        for (int i = 0; i < 100; ++i) {
            particles.push_back({
                static_cast<float>(i % 10) * 5.0f,
                static_cast<float>((i / 10) % 10) * 5.0f,
                static_cast<float>(i / 100) * 5.0f,
                i
            });
        }

        grid.rebuild(particles);

        auto stats = grid.stats();

        REQUIRE(stats.total_entities == 100);
        REQUIRE(stats.occupied_cells > 0);
        REQUIRE(stats.avg_entities_per_cell > 0);
        REQUIRE(stats.occupancy_ratio > 0);
        REQUIRE(stats.occupancy_ratio <= 1.0f);
    }
}

TEST_CASE("Grid capacity methods", "[basic][capacity]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);

    SECTION("Empty grid capacity") {
        REQUIRE(grid.empty());
        REQUIRE(grid.cell_count() == 0);
        REQUIRE(grid.entity_count() == 0);
    }

    SECTION("Non-empty grid capacity") {
        std::vector<Particle> particles = {
            {5.0f, 5.0f, 5.0f, 0},
            {15.0f, 15.0f, 15.0f, 1}
        };

        grid.rebuild(particles);

        REQUIRE_FALSE(grid.empty());
        REQUIRE(grid.cell_count() > 0);
        REQUIRE(grid.entity_count() == 0);  // Not tracked until reserve_entities
    }

    SECTION("Clear grid") {
        std::vector<Particle> particles = {{5.0f, 5.0f, 5.0f, 0}};
        grid.rebuild(particles);

        grid.clear();

        REQUIRE(grid.empty());
        REQUIRE(grid.cell_count() == 0);
    }
}
