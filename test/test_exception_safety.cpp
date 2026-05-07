/**
 * Exception safety tests for sparse_spatial_hash
 *
 * Verifies documented exception safety guarantees:
 * - Queries: Nothrow guarantee
 * - Single-entity operations: Strong guarantee
 * - Bulk operations: Basic guarantee
 */

#include "test_helpers.hpp"
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <stdexcept>

using namespace spatial;

TEST_CASE("query_radius nothrow guarantee", "[exception_safety][nothrow][query]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);

    std::vector<Particle> particles = {
        {5.0f, 5.0f, 5.0f, 0},
        {15.0f, 15.0f, 15.0f, 1}
    };

    grid.rebuild(particles);

    SECTION("Normal query does not throw") {
        REQUIRE_NOTHROW(grid.query_radius(10.0f, 0.0f, 0.0f, 0.0f));
    }

    SECTION("Query with negative radius does not throw") {
        // Negative radius is invalid input but should not throw (nothrow guarantee)
        REQUIRE_NOTHROW(grid.query_radius(-10.0f, 0.0f, 0.0f, 0.0f));
    }

    SECTION("Query with huge radius does not throw") {
        REQUIRE_NOTHROW(grid.query_radius(1e30f, 0.0f, 0.0f, 0.0f));
    }

    SECTION("Query with infinite radius does not throw") {
        REQUIRE_NOTHROW(grid.query_radius(
            std::numeric_limits<float>::infinity(),
            0.0f, 0.0f, 0.0f
        ));
    }

    SECTION("Query with NaN radius does not throw") {
        // NaN is invalid but should not throw (though results may be empty)
        REQUIRE_NOTHROW(grid.query_radius(
            std::numeric_limits<float>::quiet_NaN(),
            0.0f, 0.0f, 0.0f
        ));
    }

    SECTION("Query with NaN coordinates does not throw") {
        REQUIRE_NOTHROW(grid.query_radius(
            10.0f,
            std::numeric_limits<float>::quiet_NaN(),
            0.0f, 0.0f
        ));
    }

    SECTION("Query on empty grid does not throw") {
        sparse_spatial_hash<Particle, 3> empty_grid(cfg);
        REQUIRE_NOTHROW(empty_grid.query_radius(10.0f, 0.0f, 0.0f, 0.0f));
    }

    SECTION("Query with extreme out-of-bounds coordinates does not throw") {
        REQUIRE_NOTHROW(grid.query_radius(10.0f, 1e30f, 1e30f, 1e30f));
        REQUIRE_NOTHROW(grid.query_radius(10.0f, -1e30f, -1e30f, -1e30f));
    }
}

TEST_CASE("cell_entities nothrow guarantee", "[exception_safety][nothrow][iteration]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);

    std::vector<Particle> particles = {
        {5.0f, 5.0f, 5.0f, 0}
    };
    grid.rebuild(particles);

    SECTION("cell_entities on occupied cell does not throw") {
        cell_coord<3, int> cell{5, 5, 5};
        REQUIRE_NOTHROW(grid.cell_entities(cell));
    }

    SECTION("cell_entities on unoccupied cell does not throw") {
        cell_coord<3, int> empty_cell{99, 99, 99};
        REQUIRE_NOTHROW(grid.cell_entities(empty_cell));
    }

    SECTION("cell_entities on empty grid does not throw") {
        sparse_spatial_hash<Particle, 3> empty_grid(cfg);
        cell_coord<3, int> cell{0, 0, 0};
        REQUIRE_NOTHROW(empty_grid.cell_entities(cell));
    }
}

TEST_CASE("for_each_pair basic exception guarantee", "[exception_safety][basic_guarantee][pairs]") {
    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 2> grid(cfg);

    std::vector<Particle> particles = {
        {5.0f, 5.0f, 0.0f, 0},
        {6.0f, 6.0f, 0.0f, 1},
        {7.0f, 7.0f, 0.0f, 2},
        {8.0f, 8.0f, 0.0f, 3}
    };

    grid.rebuild(particles);

    SECTION("Exception in callback does not corrupt grid") {
        bool threw = false;
        int pairs_processed = 0;

        try {
            grid.for_each_pair(20.0f,
                [&](std::size_t i, std::size_t j) {
                    pairs_processed++;
                    if (pairs_processed == 3) {
                        throw std::runtime_error("Test exception");
                    }
                });
        } catch (const std::runtime_error&) {
            threw = true;
        }

        // THEN: Exception should propagate
        REQUIRE(threw);
        REQUIRE(pairs_processed == 3);

        // AND: Grid should remain in valid state (basic guarantee)
        REQUIRE(grid.cell_count() > 0);
        REQUIRE(grid.entity_count() == 4);

        // Should be able to continue using grid
        auto results = grid.query_radius(10.0f, 5.0f, 5.0f);
        REQUIRE_FALSE(results.empty());
    }

    SECTION("for_each_pair with throwing callback on empty grid") {
        sparse_spatial_hash<Particle, 2> empty_grid(cfg);
        std::vector<Particle> empty;
        empty_grid.rebuild(empty);

        int callback_calls = 0;
        // Should not call callback on empty grid, so no exception
        REQUIRE_NOTHROW(
            empty_grid.for_each_pair(20.0f,
                [&](std::size_t, std::size_t) {
                    callback_calls++;
                    throw std::runtime_error("Should not be called");
                })
        );

        REQUIRE(callback_calls == 0);
    }
}

TEST_CASE("rebuild basic exception guarantee", "[exception_safety][basic_guarantee][rebuild]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);

    SECTION("rebuild with valid data does not throw") {
        std::vector<Particle> particles = {
            {5.0f, 5.0f, 5.0f, 0},
            {15.0f, 15.0f, 15.0f, 1}
        };

        REQUIRE_NOTHROW(grid.rebuild(particles));
        REQUIRE(grid.cell_count() == 2);
    }

    SECTION("rebuild clears previous state") {
        std::vector<Particle> particles1 = {
            {5.0f, 5.0f, 5.0f, 0},
            {15.0f, 15.0f, 15.0f, 1}
        };
        grid.rebuild(particles1);
        REQUIRE(grid.cell_count() == 2);

        std::vector<Particle> particles2 = {
            {25.0f, 25.0f, 25.0f, 0}
        };
        grid.rebuild(particles2);

        // Should have new state, not combined
        REQUIRE(grid.cell_count() == 1);
        REQUIRE(grid.entity_count() == 1);
    }

    SECTION("rebuild with empty range clears grid") {
        std::vector<Particle> particles = {{5.0f, 5.0f, 5.0f, 0}};
        grid.rebuild(particles);
        REQUIRE_FALSE(grid.empty());

        std::vector<Particle> empty;
        grid.rebuild(empty);

        REQUIRE(grid.empty());
        REQUIRE(grid.cell_count() == 0);
        REQUIRE(grid.entity_count() == 0);
    }
}

TEST_CASE("update basic exception guarantee", "[exception_safety][basic_guarantee][update]") {
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

    SECTION("update with no changes succeeds") {
        REQUIRE_NOTHROW(grid.update(particles));
        REQUIRE(grid.cell_count() == 2);
    }

    SECTION("update with entity count change triggers rebuild") {
        particles.push_back({25.0f, 25.0f, 0.0f, 2});

        REQUIRE_NOTHROW(grid.update(particles));
        REQUIRE(grid.entity_count() == 3);
        REQUIRE(grid.cell_count() == 3);
    }

    SECTION("update maintains valid state after modifications") {
        particles[0].x = 16.0f;
        particles[0].y = 16.0f;

        grid.update(particles);

        // Both particles now in same cell region
        REQUIRE(grid.entity_count() == 2);

        // Grid should be queryable
        auto results = grid.query_radius(20.0f, 15.0f, 15.0f);
        REQUIRE_FALSE(results.empty());
    }
}

TEST_CASE("clear nothrow guarantee", "[exception_safety][nothrow][clear]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);

    std::vector<Particle> particles = {
        {5.0f, 5.0f, 5.0f, 0},
        {15.0f, 15.0f, 15.0f, 1}
    };
    grid.rebuild(particles);

    SECTION("clear does not throw") {
        REQUIRE_NOTHROW(grid.clear());
        REQUIRE(grid.empty());
    }

    SECTION("clear on empty grid does not throw") {
        grid.clear();
        REQUIRE_NOTHROW(grid.clear());
    }

    SECTION("Operations work after clear") {
        grid.clear();

        REQUIRE_NOTHROW(grid.rebuild(particles));
        REQUIRE(grid.cell_count() == 2);
    }
}

TEST_CASE("Statistics methods nothrow guarantee", "[exception_safety][nothrow][stats]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);

    SECTION("Stats on empty grid does not throw") {
        REQUIRE_NOTHROW(grid.stats());
        auto s = grid.stats();
        REQUIRE(s.occupied_cells == 0);
    }

    SECTION("Stats on populated grid does not throw") {
        std::vector<Particle> particles = {
            {5.0f, 5.0f, 5.0f, 0},
            {15.0f, 15.0f, 15.0f, 1}
        };
        grid.rebuild(particles);

        REQUIRE_NOTHROW(grid.stats());
    }

    SECTION("Capacity methods do not throw") {
        REQUIRE_NOTHROW(grid.empty());
        REQUIRE_NOTHROW(grid.cell_count());
        REQUIRE_NOTHROW(grid.entity_count());
        REQUIRE_NOTHROW(grid.occupancy());
    }
}

TEST_CASE("Iteration methods nothrow guarantee", "[exception_safety][nothrow][iteration]") {
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

    SECTION("cells() does not throw") {
        REQUIRE_NOTHROW(grid.cells());

        // Iteration should not throw
        bool threw = false;
        try {
            for (const auto& cell : grid.cells()) {
                (void)cell;  // Use cell to avoid warning
            }
        } catch (...) {
            threw = true;
        }
        REQUIRE_FALSE(threw);
    }

    SECTION("cell_contents() does not throw") {
        REQUIRE_NOTHROW(grid.cell_contents());

        // Iteration should not throw
        bool threw = false;
        try {
            for (const auto& [cell, entities] : grid.cell_contents()) {
                (void)cell;
                (void)entities;
            }
        } catch (...) {
            threw = true;
        }
        REQUIRE_FALSE(threw);
    }
}

TEST_CASE("Configuration access nothrow guarantee", "[exception_safety][nothrow][config]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);

    SECTION("config() does not throw") {
        REQUIRE_NOTHROW(grid.config());
    }

    SECTION("resolution() does not throw") {
        REQUIRE_NOTHROW(grid.resolution());
    }
}

TEST_CASE("Copy and move operations exception safety", "[exception_safety][copy][move]") {
    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 2> grid1(cfg);

    std::vector<Particle> particles = {
        {5.0f, 5.0f, 0.0f, 0},
        {15.0f, 15.0f, 0.0f, 1}
    };
    grid1.rebuild(particles);

    SECTION("Copy constructor basic guarantee") {
        sparse_spatial_hash<Particle, 2> grid2(grid1);

        REQUIRE(grid2.cell_count() == grid1.cell_count());
        REQUIRE(grid2.entity_count() == grid1.entity_count());
    }

    SECTION("Copy assignment basic guarantee") {
        sparse_spatial_hash<Particle, 2> grid2(cfg);

        grid2 = grid1;

        REQUIRE(grid2.cell_count() == grid1.cell_count());
        REQUIRE(grid2.entity_count() == grid1.entity_count());
    }

    SECTION("Move constructor nothrow") {
        // Move constructor should be noexcept
        sparse_spatial_hash<Particle, 2> grid2(std::move(grid1));

        REQUIRE(grid2.cell_count() == 2);
        REQUIRE(grid2.entity_count() == 2);
    }

    SECTION("Move assignment nothrow") {
        // Move assignment should be noexcept
        sparse_spatial_hash<Particle, 2> grid2(cfg);

        grid2 = std::move(grid1);

        REQUIRE(grid2.cell_count() == 2);
        REQUIRE(grid2.entity_count() == 2);
    }
}

TEST_CASE("Extreme inputs do not cause undefined behavior", "[exception_safety][robustness]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);

    SECTION("Particles with extreme coordinates") {
        std::vector<Particle> particles = {
            {1e30f, 1e30f, 1e30f, 0},
            {-1e30f, -1e30f, -1e30f, 1},
            {std::numeric_limits<float>::max(), 0.0f, 0.0f, 2},
            {std::numeric_limits<float>::lowest(), 0.0f, 0.0f, 3}
        };

        // Should not crash with extreme coordinates
        REQUIRE_NOTHROW(grid.rebuild(particles));

        // Grid should be in valid state
        REQUIRE(grid.entity_count() == 4);
    }

    SECTION("Particles with NaN coordinates") {
        std::vector<Particle> particles = {
            {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0},
            {0.0f, std::numeric_limits<float>::quiet_NaN(), 0.0f, 1}
        };

        // Behavior with NaN is undefined but should not crash
        // Grid will place them somewhere (likely cell 0,0,0 after conversion)
        REQUIRE_NOTHROW(grid.rebuild(particles));
    }

    SECTION("Particles with infinity coordinates") {
        std::vector<Particle> particles = {
            {std::numeric_limits<float>::infinity(), 0.0f, 0.0f, 0},
            {-std::numeric_limits<float>::infinity(), 0.0f, 0.0f, 1}
        };

        // Should handle infinity gracefully (clamp or wrap)
        REQUIRE_NOTHROW(grid.rebuild(particles));
    }
}
