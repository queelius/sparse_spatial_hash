/**
 * Advanced tests for sparse_spatial_hash
 *
 * Tests:
 * - Negative coordinates in infinite topology
 * - Very small/large cell sizes
 * - STL compatibility with various containers
 * - Statistics accuracy verification
 * - Move semantics and copy correctness
 */

#include <spatial/sparse_spatial_hash.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>
#include <list>
#include <deque>
#include <ranges>
#include <algorithm>

using namespace spatial;

struct TestEntity {
    float x, y, z;
    int id;
};

template<>
struct spatial::position_accessor<TestEntity, 3> {
    static float get(const TestEntity& e, std::size_t dim) {
        switch(dim) {
            case 0: return e.x;
            case 1: return e.y;
            case 2: return e.z;
            default: return 0.0f;
        }
    }
};

struct Entity2D {
    float x, y;
    int id;
};

template<>
struct spatial::position_accessor<Entity2D, 2> {
    static float get(const Entity2D& e, std::size_t dim) {
        return dim == 0 ? e.x : e.y;
    }
};

// ============================================================================
// Test 11: Negative Coordinates in Infinite Topology
// ============================================================================

TEST_CASE("Negative coordinates in infinite topology", "[topology][infinite][negative]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::infinite
    };

    sparse_spatial_hash<TestEntity, 3> grid(cfg);

    SECTION("Large negative coordinates work correctly") {
        // GIVEN: Entities with large negative coordinates
        std::vector<TestEntity> entities = {
            {-1000.0f, -1000.0f, -1000.0f, 0},
            {-500.0f, -500.0f, -500.0f, 1},
            {0.0f, 0.0f, 0.0f, 2},
            {500.0f, 500.0f, 500.0f, 3},
            {1000.0f, 1000.0f, 1000.0f, 4}
        };

        // WHEN: Building grid
        grid.rebuild(entities);

        // THEN: All entities should be placed correctly
        REQUIRE(grid.cell_count() == 5);

        auto stats = grid.stats();
        REQUIRE(stats.total_entities == 5);
    }

    SECTION("Queries with negative coordinates") {
        // GIVEN: Entities in negative space
        std::vector<TestEntity> entities = {
            {-10.0f, -10.0f, -10.0f, 0},
            {-5.0f, -10.0f, -10.0f, 1},
            {100.0f, 100.0f, 100.0f, 2}
        };

        grid.rebuild(entities);

        // WHEN: Querying in negative space
        auto results = grid.query_radius(20.0f, -10.0f, -10.0f, -10.0f);

        // THEN: Should find nearby entities in negative space
        REQUIRE_FALSE(results.empty());

        bool found_0 = false, found_1 = false;
        for (auto idx : results) {
            if (idx == 0) found_0 = true;
            if (idx == 1) found_1 = true;
        }
        REQUIRE(found_0);
        REQUIRE(found_1);
    }

    SECTION("Cell coordinate calculation handles sign correctly") {
        // GIVEN: Entities on both sides of origin
        std::vector<TestEntity> entities = {
            {-15.0f, 0.0f, 0.0f, 0},  // Negative cell
            {-5.0f, 0.0f, 0.0f, 1},   // Different negative cell
            {5.0f, 0.0f, 0.0f, 2},    // Positive cell
            {15.0f, 0.0f, 0.0f, 3}    // Different positive cell
        };

        grid.rebuild(entities);

        // THEN: Should create separate cells for different coordinates
        REQUIRE(grid.cell_count() == 4);
    }

    SECTION("Incremental update with negative movements") {
        // GIVEN: Entities in positive space
        std::vector<TestEntity> entities = {
            {10.0f, 10.0f, 10.0f, 0},
            {20.0f, 20.0f, 20.0f, 1}
        };

        grid.rebuild(entities);
        REQUIRE(grid.cell_count() == 2);

        // WHEN: Moving to negative space
        entities[0].x = -100.0f;
        entities[0].y = -100.0f;
        entities[0].z = -100.0f;

        grid.update(entities);

        // THEN: Should track correctly across origin
        REQUIRE(grid.cell_count() == 2);

        auto results = grid.query_radius(20.0f, -100.0f, -100.0f, -100.0f);
        bool found_0 = std::find(results.begin(), results.end(), 0) != results.end();
        REQUIRE(found_0);
    }

    SECTION("for_each_pair works with negative coordinates") {
        // GIVEN: Entities in negative space
        std::vector<TestEntity> entities = {
            {-10.0f, -10.0f, -10.0f, 0},
            {-8.0f, -10.0f, -10.0f, 1},
            {-100.0f, -100.0f, -100.0f, 2}
        };

        grid.rebuild(entities);

        // WHEN: Processing pairs
        int pair_count = 0;
        grid.for_each_pair(entities, 30.0f,
            [&](std::size_t i, std::size_t j) {
                pair_count++;
                REQUIRE(i < j);
            });

        // THEN: Should find pairs in negative space
        REQUIRE(pair_count >= 1);
    }

    SECTION("Very large negative coordinates don't overflow") {
        // GIVEN: Extreme negative coordinates
        std::vector<TestEntity> entities = {
            {-100000.0f, -100000.0f, -100000.0f, 0},
            {-99990.0f, -100000.0f, -100000.0f, 1}
        };

        // WHEN: Building grid
        grid.rebuild(entities);

        // THEN: Should handle without overflow
        REQUIRE(grid.cell_count() > 0);

        auto results = grid.query_radius(50.0f, -100000.0f, -100000.0f, -100000.0f);
        REQUIRE_FALSE(results.empty());
    }
}

// ============================================================================
// Test 12: Very Small/Large Cell Sizes
// ============================================================================

TEST_CASE("Very small and large cell sizes", "[config][cell_size][precision]") {
    SECTION("Very small cell size (high resolution)") {
        // GIVEN: Tiny cells for high precision
        grid_config<2> cfg{
            .cell_size = {0.001f, 0.001f},
            .world_size = {10.0f, 10.0f},
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<Entity2D, 2> grid(cfg);

        // WHEN: Placing closely spaced entities
        std::vector<Entity2D> entities = {
            {0.0f, 0.0f, 0},
            {0.002f, 0.0f, 1},   // 2mm away
            {0.004f, 0.0f, 2}    // 4mm away
        };

        grid.rebuild(entities);

        // THEN: Should create separate cells for each
        REQUIRE(grid.cell_count() >= 2);

        // AND: Queries should work at small scale
        auto results = grid.query_radius(0.003f, 0.0f, 0.0f);
        REQUIRE_FALSE(results.empty());
    }

    SECTION("Very large cell size (low resolution)") {
        // GIVEN: Huge cells
        grid_config<2> cfg{
            .cell_size = {10000.0f, 10000.0f},
            .world_size = {100000.0f, 100000.0f},
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<Entity2D, 2> grid(cfg);

        // WHEN: Placing widely spaced entities
        std::vector<Entity2D> entities = {
            {0.0f, 0.0f, 0},
            {5000.0f, 0.0f, 1},      // 5km away, same cell
            {15000.0f, 0.0f, 2}      // 15km away, different cell
        };

        grid.rebuild(entities);

        // THEN: Should group entities into large cells
        REQUIRE(grid.cell_count() <= 2);

        auto stats = grid.stats();
        REQUIRE(stats.total_entities == 3);
    }

    SECTION("Cell size affects query results") {
        // Compare two grids with different cell sizes
        std::vector<Entity2D> entities = {
            {5.0f, 5.0f, 0},
            {15.0f, 5.0f, 1},
            {25.0f, 5.0f, 2}
        };

        // Fine-grained grid
        grid_config<2> fine_cfg{
            .cell_size = {5.0f, 5.0f},
            .world_size = {100.0f, 100.0f},
            .topology_type = topology::bounded
        };
        sparse_spatial_hash<Entity2D, 2> fine_grid(fine_cfg);
        fine_grid.rebuild(entities);

        // Coarse-grained grid
        grid_config<2> coarse_cfg{
            .cell_size = {20.0f, 20.0f},
            .world_size = {100.0f, 100.0f},
            .topology_type = topology::bounded
        };
        sparse_spatial_hash<Entity2D, 2> coarse_grid(coarse_cfg);
        coarse_grid.rebuild(entities);

        // WHEN: Same query on both
        auto fine_results = fine_grid.query_radius(8.0f, 5.0f, 5.0f);
        auto coarse_results = coarse_grid.query_radius(8.0f, 5.0f, 5.0f);

        // THEN: Coarse grid may return more (conservative)
        REQUIRE(coarse_results.size() >= fine_results.size());
    }

    SECTION("Non-uniform cell sizes") {
        // GIVEN: Different cell sizes per dimension
        grid_config<3> cfg{
            .cell_size = {1.0f, 10.0f, 100.0f},  // Different scales
            .world_size = {100.0f, 100.0f, 100.0f},
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<TestEntity, 3> grid(cfg);

        // WHEN: Placing entities
        std::vector<TestEntity> entities = {
            {0.5f, 5.0f, 50.0f, 0},
            {1.5f, 5.0f, 50.0f, 1},   // Different cell in X
            {0.5f, 15.0f, 50.0f, 2},  // Different cell in Y
            {0.5f, 5.0f, 150.0f, 3}   // Different cell in Z (clamped)
        };

        grid.rebuild(entities);

        // THEN: Should handle non-uniform cells
        REQUIRE(grid.cell_count() > 0);

        auto stats = grid.stats();
        REQUIRE(stats.total_entities == 4);
    }

    SECTION("Resolution calculation correctness") {
        // GIVEN: Various cell configurations
        grid_config<2> cfg1{
            .cell_size = {10.0f, 10.0f},
            .world_size = {100.0f, 100.0f},
            .topology_type = topology::bounded
        };

        grid_config<2> cfg2{
            .cell_size = {5.0f, 5.0f},
            .world_size = {100.0f, 100.0f},
            .topology_type = topology::bounded
        };

        // WHEN: Computing resolution
        auto res1 = cfg1.resolution();
        auto res2 = cfg2.resolution();

        // THEN: Resolution should scale inversely with cell size
        REQUIRE(res1[0] == 10);  // 100/10 = 10
        REQUIRE(res1[1] == 10);
        REQUIRE(res2[0] == 20);  // 100/5 = 20
        REQUIRE(res2[1] == 20);
    }
}

// ============================================================================
// Test 13: STL Compatibility
// ============================================================================

TEST_CASE("STL compatibility with various containers", "[stl][containers][ranges]") {
    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    SECTION("Works with std::vector") {
        sparse_spatial_hash<Entity2D, 2> grid(cfg);

        std::vector<Entity2D> entities = {
            {5.0f, 5.0f, 0},
            {15.0f, 15.0f, 1}
        };

        grid.rebuild(entities);
        REQUIRE(grid.cell_count() == 2);
    }

    SECTION("Works with std::list") {
        sparse_spatial_hash<Entity2D, 2> grid(cfg);

        std::list<Entity2D> entities = {
            {5.0f, 5.0f, 0},
            {15.0f, 15.0f, 1},
            {25.0f, 25.0f, 2}
        };

        grid.rebuild(entities);
        REQUIRE(grid.cell_count() == 3);

        // Update should also work
        entities.front().x = 16.0f;
        entities.front().y = 16.0f;
        grid.update(entities);
        REQUIRE(grid.cell_count() == 2);
    }

    SECTION("Works with std::deque") {
        sparse_spatial_hash<Entity2D, 2> grid(cfg);

        std::deque<Entity2D> entities;
        entities.push_back({5.0f, 5.0f, 0});
        entities.push_back({15.0f, 15.0f, 1});

        grid.rebuild(entities);
        REQUIRE(grid.cell_count() == 2);
    }

    SECTION("Works with transformed containers") {
        sparse_spatial_hash<Entity2D, 2> grid(cfg);

        std::vector<Entity2D> all_entities = {
            {5.0f, 5.0f, 0},
            {15.0f, 15.0f, 1},
            {25.0f, 25.0f, 2},
            {35.0f, 35.0f, 3}
        };

        // Filter to only even IDs and collect
        std::vector<Entity2D> even_entities;
        for (const auto& e : all_entities) {
            if (e.id % 2 == 0) {
                even_entities.push_back(e);
            }
        }

        grid.rebuild(even_entities);

        // Should only have entities 0 and 2
        REQUIRE(grid.cell_count() == 2);
    }

    SECTION("for_each_pair works with different container types") {
        sparse_spatial_hash<Entity2D, 2> grid(cfg);

        std::list<Entity2D> entities = {
            {5.0f, 5.0f, 0},
            {6.0f, 5.0f, 1},
            {50.0f, 50.0f, 2}
        };

        grid.rebuild(entities);

        int pair_count = 0;
        grid.for_each_pair(entities, 20.0f,
            [&](std::size_t, std::size_t) { pair_count++; });

        REQUIRE(pair_count >= 1);
    }

    SECTION("Query results are STL-compatible") {
        sparse_spatial_hash<Entity2D, 2> grid(cfg);

        std::vector<Entity2D> entities = {
            {5.0f, 5.0f, 0},
            {15.0f, 5.0f, 1},
            {25.0f, 5.0f, 2}
        };

        grid.rebuild(entities);

        auto results = grid.query_radius(30.0f, 10.0f, 5.0f);

        // Should work with STL algorithms
        std::sort(results.begin(), results.end());
        auto unique_end = std::unique(results.begin(), results.end());
        results.erase(unique_end, results.end());

        REQUIRE_FALSE(results.empty());
    }

    SECTION("Cell iteration is ranges-compatible") {
        sparse_spatial_hash<Entity2D, 2> grid(cfg);

        std::vector<Entity2D> entities = {
            {5.0f, 5.0f, 0},
            {15.0f, 15.0f, 1},
            {25.0f, 25.0f, 2}
        };

        grid.rebuild(entities);

        // Iterate over cells
        int cell_count = 0;
        for (const auto& cell : grid.cells()) {
            cell_count++;
        }
        REQUIRE(cell_count == 3);

        // Iterate over cell contents
        int entity_total = 0;
        for (const auto& [cell, cell_entities] : grid.cell_contents()) {
            entity_total += cell_entities.size();
        }
        REQUIRE(entity_total == 3);
    }
}

// ============================================================================
// Test 14: Statistics Accuracy
// ============================================================================

TEST_CASE("Statistics accuracy verification", "[stats][accuracy][verification]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<TestEntity, 3> grid(cfg);

    SECTION("Occupied cells count is accurate") {
        // GIVEN: Known distribution
        std::vector<TestEntity> entities = {
            {5.0f, 5.0f, 5.0f, 0},
            {6.0f, 5.0f, 5.0f, 1},   // Same cell
            {15.0f, 15.0f, 15.0f, 2},
            {16.0f, 15.0f, 15.0f, 3} // Same cell
        };

        grid.rebuild(entities);

        auto stats = grid.stats();

        // THEN: Should have exactly 2 occupied cells
        REQUIRE(stats.occupied_cells == 2);
        REQUIRE(grid.cell_count() == 2);
    }

    SECTION("Total entities is accurate") {
        std::vector<TestEntity> entities;
        for (int i = 0; i < 100; ++i) {
            entities.push_back({
                static_cast<float>((i * 7) % 100 - 50),
                static_cast<float>((i * 11) % 100 - 50),
                static_cast<float>((i * 13) % 100 - 50),
                i
            });
        }

        grid.rebuild(entities);

        auto stats = grid.stats();

        // Manual count
        int manual_count = 0;
        for (const auto& [cell, cell_entities] : grid.cell_contents()) {
            manual_count += cell_entities.size();
        }

        REQUIRE(stats.total_entities == 100);
        REQUIRE(stats.total_entities == manual_count);
    }

    SECTION("Max entities per cell is correct") {
        // GIVEN: Controlled distribution
        std::vector<TestEntity> entities = {
            {5.0f, 5.0f, 5.0f, 0},
            {6.0f, 5.0f, 5.0f, 1},
            {7.0f, 5.0f, 5.0f, 2},  // 3 in first cell
            {50.0f, 50.0f, 50.0f, 3},
            {51.0f, 50.0f, 50.0f, 4} // 2 in second cell
        };

        grid.rebuild(entities);

        auto stats = grid.stats();

        // THEN: Max should be 3
        REQUIRE(stats.max_entities_per_cell == 3);
    }

    SECTION("Average entities per cell is accurate") {
        // GIVEN: 10 entities in multiple cells
        std::vector<TestEntity> entities;
        for (int i = 0; i < 5; ++i) {
            // Place two entities far apart to ensure different cells
            entities.push_back({
                static_cast<float>(i * 20 - 40),
                0.0f,
                0.0f,
                i * 2
            });
            entities.push_back({
                static_cast<float>(i * 20 - 40 + 1),  // Just offset, likely same cell
                0.0f,
                0.0f,
                i * 2 + 1
            });
        }

        grid.rebuild(entities);

        auto stats = grid.stats();

        REQUIRE(stats.total_entities == 10);
        REQUIRE(stats.occupied_cells > 0);
        // Average should be total / occupied
        float expected_avg = static_cast<float>(stats.total_entities) / stats.occupied_cells;
        REQUIRE_THAT(stats.avg_entities_per_cell,
                    Catch::Matchers::WithinRel(expected_avg, 0.0001f));
    }

    SECTION("Occupancy ratio calculation") {
        // GIVEN: Known world size and cell configuration
        // Resolution is 10x10x10 = 1000 cells
        std::vector<TestEntity> entities;
        for (int i = 0; i < 100; ++i) {
            entities.push_back({
                static_cast<float>(i * 5),
                0.0f,
                0.0f,
                i
            });
        }

        grid.rebuild(entities);

        auto stats = grid.stats();

        // THEN: Occupancy should be occupied_cells / total_cells
        float expected_occupancy = static_cast<float>(stats.occupied_cells) / 1000.0f;
        REQUIRE_THAT(stats.occupancy_ratio,
                    Catch::Matchers::WithinRel(expected_occupancy, 0.0001f));
        REQUIRE(stats.occupancy_ratio <= 1.0f);
    }

    SECTION("Statistics remain consistent after updates") {
        std::vector<TestEntity> entities = {
            {5.0f, 5.0f, 5.0f, 0},
            {15.0f, 15.0f, 15.0f, 1}
        };

        grid.rebuild(entities);
        auto stats1 = grid.stats();

        // Move entity to same cell as other
        entities[0].x = 16.0f;
        entities[0].y = 16.0f;
        entities[0].z = 16.0f;

        grid.update(entities);
        auto stats2 = grid.stats();

        // Total entities should remain same
        REQUIRE(stats1.total_entities == stats2.total_entities);

        // Occupied cells should decrease (both in roughly same area)
        REQUIRE(stats2.occupied_cells <= stats1.occupied_cells);
    }
}

// ============================================================================
// Test 15: Move Semantics and Copy Correctness
// ============================================================================

TEST_CASE("Move semantics and copy correctness", "[copy][move][semantics]") {
    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    SECTION("Copy constructor creates independent copy") {
        // GIVEN: Original grid
        sparse_spatial_hash<Entity2D, 2> grid1(cfg);

        std::vector<Entity2D> entities = {
            {5.0f, 5.0f, 0},
            {15.0f, 15.0f, 1}
        };
        grid1.rebuild(entities);

        // WHEN: Copying grid
        sparse_spatial_hash<Entity2D, 2> grid2 = grid1;

        // THEN: Both should have same state
        REQUIRE(grid1.cell_count() == grid2.cell_count());
        REQUIRE(grid1.stats().total_entities == grid2.stats().total_entities);

        // AND: Modifying copy doesn't affect original
        std::vector<Entity2D> more_entities = {
            {25.0f, 25.0f, 2}
        };
        grid2.rebuild(more_entities);

        REQUIRE(grid1.cell_count() == 2);  // Original unchanged
        REQUIRE(grid2.cell_count() == 1);  // Copy changed
    }

    SECTION("Copy assignment creates independent copy") {
        sparse_spatial_hash<Entity2D, 2> grid1(cfg);
        sparse_spatial_hash<Entity2D, 2> grid2(cfg);

        std::vector<Entity2D> entities1 = {{5.0f, 5.0f, 0}};
        std::vector<Entity2D> entities2 = {{15.0f, 15.0f, 1}, {25.0f, 25.0f, 2}};

        grid1.rebuild(entities1);
        grid2.rebuild(entities2);

        // WHEN: Copy assignment
        grid2 = grid1;

        // THEN: grid2 should match grid1
        REQUIRE(grid2.cell_count() == grid1.cell_count());

        // AND: They are independent
        grid2.clear();
        REQUIRE(grid1.cell_count() == 1);  // Original unchanged
        REQUIRE(grid2.cell_count() == 0);  // Copy cleared
    }

    SECTION("Move constructor transfers ownership") {
        sparse_spatial_hash<Entity2D, 2> grid1(cfg);

        std::vector<Entity2D> entities = {
            {5.0f, 5.0f, 0},
            {15.0f, 15.0f, 1},
            {25.0f, 25.0f, 2}
        };
        grid1.rebuild(entities);

        auto original_cell_count = grid1.cell_count();

        // WHEN: Moving grid
        sparse_spatial_hash<Entity2D, 2> grid2 = std::move(grid1);

        // THEN: grid2 should have the data
        REQUIRE(grid2.cell_count() == original_cell_count);

        // grid1 is in valid but unspecified state (likely empty after move)
        // We don't test grid1's state as it's implementation-defined
    }

    SECTION("Move assignment transfers ownership") {
        sparse_spatial_hash<Entity2D, 2> grid1(cfg);
        sparse_spatial_hash<Entity2D, 2> grid2(cfg);

        std::vector<Entity2D> entities1 = {{5.0f, 5.0f, 0}, {15.0f, 15.0f, 1}};
        std::vector<Entity2D> entities2 = {{25.0f, 25.0f, 2}};

        grid1.rebuild(entities1);
        grid2.rebuild(entities2);

        // WHEN: Move assignment
        grid2 = std::move(grid1);

        // THEN: grid2 should have grid1's data
        REQUIRE(grid2.cell_count() == 2);
    }

    SECTION("Copied grid queries return same results") {
        sparse_spatial_hash<Entity2D, 2> grid1(cfg);

        std::vector<Entity2D> entities = {
            {5.0f, 5.0f, 0},
            {15.0f, 5.0f, 1},
            {25.0f, 5.0f, 2}
        };
        grid1.rebuild(entities);

        // Copy
        sparse_spatial_hash<Entity2D, 2> grid2 = grid1;

        // WHEN: Same query on both
        auto results1 = grid1.query_radius(20.0f, 10.0f, 5.0f);
        auto results2 = grid2.query_radius(20.0f, 10.0f, 5.0f);

        // THEN: Results should be identical
        std::sort(results1.begin(), results1.end());
        std::sort(results2.begin(), results2.end());
        REQUIRE(results1 == results2);
    }

    SECTION("Self-assignment is safe") {
        sparse_spatial_hash<Entity2D, 2> grid(cfg);

        std::vector<Entity2D> entities = {{5.0f, 5.0f, 0}};
        grid.rebuild(entities);

        // WHEN: Self-assignment
        grid = grid;

        // THEN: Grid should still work
        REQUIRE(grid.cell_count() == 1);

        auto results = grid.query_radius(10.0f, 5.0f, 5.0f);
        REQUIRE_FALSE(results.empty());
    }
}
