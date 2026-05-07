/**
 * Edge case tests for sparse_spatial_hash
 * Tests corner cases identified during pre-submission review
 */

#include "test_helpers.hpp"
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <stdexcept>

using namespace spatial;

TEST_CASE("cell_entities() with unoccupied cells", "[edge][cell_entities]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);

    SECTION("Empty cell returns empty range") {
        cell_coord<3, int> empty_cell{5, 5, 5};
        auto entities = grid.cell_entities(empty_cell);

        // Should be able to iterate (but will be empty)
        int count = 0;
        for (auto idx : entities) {
            (void)idx;
            count++;
        }
        REQUIRE(count == 0);
    }

    SECTION("Occupied cell returns entities") {
        std::vector<Particle> particles = {
            {5.0f, 5.0f, 5.0f, 0},    // At position (5,5,5)
            {6.0f, 6.0f, 6.0f, 1}     // Same cell
        };
        grid.rebuild(particles);

        // These positions map to cell (5,5,5): floor((5+50)/10) = floor(5.5) = 5
        cell_coord<3, int> occupied_cell{5, 5, 5};
        auto entities = grid.cell_entities(occupied_cell);

        int count = 0;
        for (auto idx : entities) {
            REQUIRE((idx == 0 || idx == 1));
            count++;
        }
        REQUIRE(count == 2);
    }
}

TEST_CASE("Large coordinates in infinite topology", "[edge][infinite][morton]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100000.0f, 100000.0f, 100000.0f},
        .topology_type = topology::infinite
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);

    SECTION("Very large positive coordinates") {
        std::vector<Particle> particles = {
            {10000.0f, 10000.0f, 10000.0f, 0},
            {10005.0f, 10005.0f, 10005.0f, 1}
        };

        grid.rebuild(particles);

        // Should still work, though may have hash collisions
        REQUIRE(grid.entity_count() == 2);
        REQUIRE(grid.cell_count() > 0);
    }

    SECTION("Very large negative coordinates") {
        std::vector<Particle> particles = {
            {-10000.0f, -10000.0f, -10000.0f, 0},
            {-10005.0f, -10005.0f, -10005.0f, 1}
        };

        grid.rebuild(particles);

        REQUIRE(grid.entity_count() == 2);
        REQUIRE(grid.cell_count() > 0);
    }
}

TEST_CASE("Bounded topology near world edges", "[edge][bounded]") {
    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 2> grid(cfg);

    SECTION("Particles at exact boundary") {
        std::vector<Particle> particles = {
            {0.0f, 0.0f, 0.0f, 0},      // Min corner
            {99.9f, 99.9f, 0.0f, 1},    // Max corner
            {50.0f, 0.0f, 0.0f, 2},     // Edge
            {0.0f, 50.0f, 0.0f, 3}      // Edge
        };

        grid.rebuild(particles);

        REQUIRE(grid.entity_count() == 4);
        REQUIRE(grid.cell_count() > 0);
    }

    SECTION("Particles outside bounds are clamped") {
        std::vector<Particle> particles = {
            {-10.0f, -10.0f, 0.0f, 0},   // Should clamp to (0,0)
            {110.0f, 110.0f, 0.0f, 1}    // Should clamp to (99,99)
        };

        grid.rebuild(particles);

        // Both should be placed in valid cells
        REQUIRE(grid.entity_count() == 2);
        REQUIRE(grid.cell_count() > 0);
    }

    SECTION("for_each_pair near boundaries") {
        std::vector<Particle> particles = {
            {5.0f, 5.0f, 0.0f, 0},
            {6.0f, 6.0f, 0.0f, 1}
        };

        grid.rebuild(particles);

        int pair_count = 0;
        grid.for_each_pair(20.0f,
            [&](std::size_t i, std::size_t j) {
                REQUIRE(i < j);  // No duplicates
                pair_count++;
            });

        REQUIRE(pair_count >= 1);
    }
}

TEST_CASE("Overflow protection in constructor", "[edge][overflow]") {
    SECTION("Very large grid doesn't cause overflow") {
        grid_config<3> cfg{
            .cell_size = {1.0f, 1.0f, 1.0f},
            .world_size = {10000.0f, 10000.0f, 10000.0f},
            .topology_type = topology::bounded
        };

        // Should not crash or overflow
        sparse_spatial_hash<Particle, 3> grid(cfg);

        REQUIRE(grid.empty());
    }
}

TEST_CASE("entity_count after various operations", "[edge][entity_count]") {
    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 2> grid(cfg);

    SECTION("entity_count after rebuild") {
        std::vector<Particle> particles = {
            {5.0f, 5.0f, 0.0f, 0},
            {15.0f, 15.0f, 0.0f, 1},
            {25.0f, 25.0f, 0.0f, 2}
        };

        grid.rebuild(particles);
        REQUIRE(grid.entity_count() == 3);
    }

    SECTION("entity_count after update") {
        std::vector<Particle> particles = {
            {5.0f, 5.0f, 0.0f, 0},
            {15.0f, 15.0f, 0.0f, 1}
        };

        grid.rebuild(particles);
        REQUIRE(grid.entity_count() == 2);

        // Add more particles
        particles.push_back({25.0f, 25.0f, 0.0f, 2});
        grid.update(particles);

        REQUIRE(grid.entity_count() == 3);
    }

    SECTION("entity_count after clear") {
        std::vector<Particle> particles = {{5.0f, 5.0f, 0.0f, 0}};
        grid.rebuild(particles);

        grid.clear();
        REQUIRE(grid.entity_count() == 0);
    }
}

TEST_CASE("Constructor rejects non-positive grid_config components", "[edge][validation]") {
    SECTION("Zero cell_size throws invalid_argument") {
        grid_config<3> cfg{
            .cell_size = {10.0f, 0.0f, 10.0f},
            .world_size = {100.0f, 100.0f, 100.0f},
            .topology_type = topology::bounded
        };
        REQUIRE_THROWS_AS((sparse_spatial_hash<Particle, 3>(cfg)), std::invalid_argument);
    }

    SECTION("Negative cell_size throws invalid_argument") {
        grid_config<2> cfg{
            .cell_size = {-1.0f, 10.0f},
            .world_size = {100.0f, 100.0f},
            .topology_type = topology::bounded
        };
        REQUIRE_THROWS_AS((sparse_spatial_hash<Particle, 2>(cfg)), std::invalid_argument);
    }

    SECTION("Zero world_size throws invalid_argument") {
        grid_config<2> cfg{
            .cell_size = {10.0f, 10.0f},
            .world_size = {100.0f, 0.0f},
            .topology_type = topology::bounded
        };
        REQUIRE_THROWS_AS((sparse_spatial_hash<Particle, 2>(cfg)), std::invalid_argument);
    }

    SECTION("Negative world_size throws invalid_argument") {
        grid_config<3> cfg{
            .cell_size = {10.0f, 10.0f, 10.0f},
            .world_size = {-100.0f, 100.0f, 100.0f},
            .topology_type = topology::toroidal
        };
        REQUIRE_THROWS_AS((sparse_spatial_hash<Particle, 3>(cfg)), std::invalid_argument);
    }

    SECTION("All-positive config constructs successfully") {
        grid_config<3> cfg{
            .cell_size = {1.0f, 1.0f, 1.0f},
            .world_size = {10.0f, 10.0f, 10.0f},
            .topology_type = topology::bounded
        };
        REQUIRE_NOTHROW((sparse_spatial_hash<Particle, 3>(cfg)));
    }

    SECTION("World too small to round to a single cell throws") {
        // world_size < cell_size/2 rounds to 0 cells; without validation this
        // tripped a divide-by-zero in the reserve overflow guard.
        grid_config<2> cfg{
            .cell_size = {100.0f, 100.0f},
            .world_size = {1.0f, 1.0f},
            .topology_type = topology::bounded
        };
        REQUIRE_THROWS_AS((sparse_spatial_hash<Particle, 2>(cfg)), std::invalid_argument);
    }
}
