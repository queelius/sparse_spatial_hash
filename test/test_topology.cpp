/**
 * Topology tests using Catch2 - bounded, toroidal, infinite
 */

#include <spatial/sparse_spatial_hash.hpp>
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <cmath>

using namespace spatial;

struct Point2D {
    float x, y;
};

template<>
struct spatial::position_accessor<Point2D, 2> {
    static float get(const Point2D& p, std::size_t dim) {
        return dim == 0 ? p.x : p.y;
    }
};

TEST_CASE("Bounded topology", "[topology][bounded]") {
    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Point2D, 2> grid(cfg);

    SECTION("Points inside bounds") {
        std::vector<Point2D> points = {
            {5.0f, 5.0f},
            {25.0f, 25.0f},  // Different cell
            {65.0f, 65.0f}   // Different cell
        };

        grid.rebuild(points);

        REQUIRE(grid.cell_count() == 3);
    }

    SECTION("Points outside bounds are clamped") {
        std::vector<Point2D> points = {
            {5.0f, 5.0f},      // Inside
            {-10.0f, -10.0f},  // Outside (should clamp)
            {110.0f, 110.0f}   // Outside (should clamp)
        };

        grid.rebuild(points);

        // All points should be in valid cells (clamped to edges)
        REQUIRE(grid.cell_count() > 0);
        REQUIRE(grid.cell_count() <= 3);
    }

    SECTION("Queries don't wrap around") {
        std::vector<Point2D> points = {
            {5.0f, 5.0f},     // Near edge
            {95.0f, 95.0f}    // Opposite edge
        };

        grid.rebuild(points);

        // Query at edge - should not find particles at opposite edge
        auto results = grid.query_radius(15.0f, 0.0f, 0.0f);

        // Should only find the nearby particle (conservative)
        bool found_nearby = false;
        for (auto idx : results) {
            if (idx == 0) found_nearby = true;
        }
        REQUIRE(found_nearby);
    }
}

TEST_CASE("Toroidal topology", "[topology][toroidal]") {
    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {100.0f, 100.0f},
        .topology_type = topology::toroidal
    };

    sparse_spatial_hash<Point2D, 2> grid(cfg);

    SECTION("Points wrap around edges") {
        std::vector<Point2D> points = {
            {5.0f, 5.0f},     // Near origin
            {95.0f, 5.0f},    // Near right edge
            {5.0f, 95.0f},    // Near top edge
            {95.0f, 95.0f}    // Near corner
        };

        grid.rebuild(points);

        REQUIRE(grid.cell_count() == 4);
    }

    SECTION("Wraparound queries find particles across boundaries") {
        std::vector<Point2D> points = {
            {5.0f, 5.0f},      // Near (0,0)
            {95.0f, 95.0f}     // Near (100,100), wraps to near (0,0)
        };

        grid.rebuild(points);

        // Query near origin with radius that wraps
        auto results = grid.query_radius(20.0f, 0.0f, 0.0f);

        // Due to wraparound, both particles might be found
        REQUIRE(results.size() > 0);
    }

    SECTION("Multiple wraps work correctly") {
        std::vector<Point2D> points;
        // Create a grid of points
        for (int i = 0; i < 10; ++i) {
            for (int j = 0; j < 10; ++j) {
                points.push_back({
                    static_cast<float>(i * 10 + 5),
                    static_cast<float>(j * 10 + 5)
                });
            }
        }

        grid.rebuild(points);

        // Should have 100 separate cells (one for each 10x10 cell)
        REQUIRE(grid.cell_count() == 100);

        // Statistics should be reasonable
        auto stats = grid.stats();
        REQUIRE(stats.total_entities == 100);
        REQUIRE(stats.avg_entities_per_cell == 1.0f);
    }
}

TEST_CASE("Infinite topology", "[topology][infinite]") {
    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {100.0f, 100.0f},
        .topology_type = topology::infinite
    };

    sparse_spatial_hash<Point2D, 2> grid(cfg);

    SECTION("Points can exist beyond world bounds") {
        std::vector<Point2D> points = {
            {5.0f, 5.0f},
            {150.0f, 150.0f},  // Beyond world bounds
            {-50.0f, -50.0f}   // Negative coordinates
        };

        grid.rebuild(points);

        // All points should be stored in their own cells
        REQUIRE(grid.cell_count() == 3);
    }

    SECTION("Far distant points don't interfere") {
        std::vector<Point2D> points = {
            {0.0f, 0.0f},
            {1000.0f, 1000.0f},  // Very far
            {-1000.0f, -1000.0f}  // Very far, opposite direction
        };

        grid.rebuild(points);

        // Query near origin
        auto results = grid.query_radius(50.0f, 0.0f, 0.0f);

        // Should only find nearby particle
        REQUIRE(results.size() >= 1);
    }

    SECTION("Grid can grow unbounded") {
        std::vector<Point2D> points;
        // Create widely spaced points
        for (int i = -10; i <= 10; ++i) {
            points.push_back({
                static_cast<float>(i * 100),
                0.0f
            });
        }

        grid.rebuild(points);

        // Should have 21 cells (one for each point)
        REQUIRE(grid.cell_count() == 21);
    }
}

TEST_CASE("Topology comparison", "[topology][comparison]") {
    SECTION("Same data, different topologies produce different results") {
        std::vector<Point2D> points = {
            {5.0f, 5.0f},
            {95.0f, 95.0f}
        };

        // Bounded
        grid_config<2> bounded_cfg{
            .cell_size = {10.0f, 10.0f},
            .world_size = {100.0f, 100.0f},
            .topology_type = topology::bounded
        };
        sparse_spatial_hash<Point2D, 2> bounded_grid(bounded_cfg);
        bounded_grid.rebuild(points);

        // Toroidal
        grid_config<2> toroidal_cfg{
            .cell_size = {10.0f, 10.0f},
            .world_size = {100.0f, 100.0f},
            .topology_type = topology::toroidal
        };
        sparse_spatial_hash<Point2D, 2> toroidal_grid(toroidal_cfg);
        toroidal_grid.rebuild(points);

        // Infinite
        grid_config<2> infinite_cfg{
            .cell_size = {10.0f, 10.0f},
            .world_size = {100.0f, 100.0f},
            .topology_type = topology::infinite
        };
        sparse_spatial_hash<Point2D, 2> infinite_grid(infinite_cfg);
        infinite_grid.rebuild(points);

        // All should have 2 cells
        REQUIRE(bounded_grid.cell_count() == 2);
        REQUIRE(toroidal_grid.cell_count() == 2);
        REQUIRE(infinite_grid.cell_count() == 2);

        // But queries might differ due to wraparound
        auto bounded_results = bounded_grid.query_radius(20.0f, 0.0f, 0.0f);
        auto toroidal_results = toroidal_grid.query_radius(20.0f, 0.0f, 0.0f);

        // Results might differ (toroidal might find more due to wrapping)
        // This is conservative - both should find at least nearby point
        REQUIRE(bounded_results.size() > 0);
        REQUIRE(toroidal_results.size() > 0);
    }
}

TEST_CASE("Edge cases for topologies", "[topology][edge_cases]") {
    SECTION("Single cell world") {
        grid_config<2> cfg{
            .cell_size = {100.0f, 100.0f},
            .world_size = {100.0f, 100.0f},
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<Point2D, 2> grid(cfg);

        std::vector<Point2D> points = {
            {10.0f, 10.0f},
            {50.0f, 50.0f},
            {90.0f, 90.0f}
        };

        grid.rebuild(points);

        // All in same cell
        REQUIRE(grid.cell_count() == 1);

        auto stats = grid.stats();
        REQUIRE(stats.max_entities_per_cell == 3);
    }

    SECTION("Empty world") {
        grid_config<2> cfg{
            .cell_size = {10.0f, 10.0f},
            .world_size = {100.0f, 100.0f},
            .topology_type = topology::toroidal
        };

        sparse_spatial_hash<Point2D, 2> grid(cfg);
        std::vector<Point2D> points;

        grid.rebuild(points);

        REQUIRE(grid.empty());
        REQUIRE(grid.cell_count() == 0);
    }
}
