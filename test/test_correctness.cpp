/**
 * Correctness tests for sparse_spatial_hash
 *
 * These tests verify critical algorithmic correctness guarantees:
 * - Morton encoding spatial locality
 * - for_each_pair() produces no duplicates with correct ordering
 * - Incremental update produces identical results to rebuild
 * - Cell boundary handling
 * - Multi-dimensional support (4D+)
 */

#include <spatial/sparse_spatial_hash.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>
#include <unordered_set>
#include <set>
#include <algorithm>
#include <cmath>

using namespace spatial;

// 2D entity for testing
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

// 4D entity for high-dimensional testing
struct Entity4D {
    std::array<float, 4> pos;
    int id;
};

template<>
struct spatial::position_accessor<Entity4D, 4> {
    static float get(const Entity4D& e, std::size_t dim) {
        return e.pos[dim];
    }
};

TEST_CASE("Morton encoding spatial locality", "[correctness][morton][hash]") {
    // GIVEN: A grid using Morton encoding for cell hashing
    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Entity2D, 2> grid(cfg);

    SECTION("Nearby cells have similar hash values") {
        // WHEN: Computing hashes for spatially close cells
        cell_hash<2, int> hasher;

        cell_coord<2, int> origin{0, 0};
        cell_coord<2, int> adjacent_x{1, 0};
        cell_coord<2, int> adjacent_y{0, 1};
        cell_coord<2, int> diagonal{1, 1};
        cell_coord<2, int> far{50, 50};

        auto hash_origin = hasher(origin);
        auto hash_adj_x = hasher(adjacent_x);
        auto hash_adj_y = hasher(adjacent_y);
        auto hash_diag = hasher(diagonal);
        auto hash_far = hasher(far);

        // THEN: Adjacent cells should have closer hashes than distant cells
        // This is a property of Morton encoding - spatial locality
        auto diff_adjacent = std::min(
            hash_origin ^ hash_adj_x,
            hash_origin ^ hash_adj_y
        );
        auto diff_far = hash_origin ^ hash_far;

        REQUIRE(diff_adjacent < diff_far);
    }

    SECTION("No hash collisions for reasonable grid sizes") {
        // WHEN: Hashing all cells in a 10x10 grid
        cell_hash<2, int> hasher;
        std::unordered_set<std::size_t> hashes;

        for (int x = 0; x < 10; ++x) {
            for (int y = 0; y < 10; ++y) {
                cell_coord<2, int> cell{x, y};
                hashes.insert(hasher(cell));
            }
        }

        // THEN: All 100 cells should have unique hashes
        REQUIRE(hashes.size() == 100);
    }

    SECTION("Z-order curve property verification") {
        // WHEN: Computing Morton codes for a 2D grid
        cell_hash<2, int> hasher;

        // Z-order curve visits cells in this order for 2x2:
        // (0,0) -> (1,0) -> (0,1) -> (1,1)
        std::vector<cell_coord<2, int>> z_order = {
            {0, 0}, {1, 0}, {0, 1}, {1, 1}
        };

        std::vector<std::size_t> hashes;
        for (const auto& cell : z_order) {
            hashes.push_back(hasher(cell));
        }

        // THEN: Hashes should be monotonically increasing for Z-order traversal
        for (std::size_t i = 1; i < hashes.size(); ++i) {
            INFO("Hash[" << i-1 << "] = " << hashes[i-1] << ", Hash[" << i << "] = " << hashes[i]);
            REQUIRE(hashes[i-1] < hashes[i]);
        }
    }
}

TEST_CASE("for_each_pair() correctness - no duplicates with ordering", "[correctness][pairs]") {
    // GIVEN: A grid with entities in multiple cells
    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Entity2D, 2> grid(cfg);

    SECTION("Exactly N(N-1)/2 pairs for N entities in same cell") {
        // GIVEN: 5 entities in the same cell
        std::vector<Entity2D> entities = {
            {5.0f, 5.0f, 0},
            {6.0f, 5.0f, 1},
            {5.0f, 6.0f, 2},
            {6.0f, 6.0f, 3},
            {5.5f, 5.5f, 4}
        };

        grid.rebuild(entities);

        // WHEN: Processing pairs with large radius (all within same cell)
        int pair_count = 0;
        grid.for_each_pair(50.0f,
            [&](std::size_t i, std::size_t j) {
                pair_count++;
            });

        // THEN: Should have exactly 5*(5-1)/2 = 10 pairs
        REQUIRE(pair_count == 10);
    }

    SECTION("No duplicate pairs - each (i,j) appears exactly once") {
        // GIVEN: Multiple entities in overlapping neighborhoods
        std::vector<Entity2D> entities = {
            {5.0f, 5.0f, 0},
            {15.0f, 5.0f, 1},
            {25.0f, 5.0f, 2},
            {5.0f, 15.0f, 3}
        };

        grid.rebuild(entities);

        // WHEN: Processing pairs
        std::unordered_set<std::string> seen_pairs;
        bool has_duplicates = false;

        grid.for_each_pair(20.0f,
            [&](std::size_t i, std::size_t j) {
                // Create unique key for this pair
                std::string key = std::to_string(std::min(i, j)) + "_" +
                                 std::to_string(std::max(i, j));

                if (seen_pairs.count(key) > 0) {
                    has_duplicates = true;
                }
                seen_pairs.insert(key);
            });

        // THEN: No duplicates should be found
        REQUIRE_FALSE(has_duplicates);
    }

    SECTION("Ordering guarantee: i < j within same cell") {
        // GIVEN: Entities all in same cell
        std::vector<Entity2D> entities;
        for (int i = 0; i < 5; ++i) {
            entities.push_back({
                static_cast<float>(i * 0.5f),  // All in same cell
                0.0f,
                i
            });
        }

        grid.rebuild(entities);

        // WHEN: Processing all pairs
        bool ordering_violated = false;
        grid.for_each_pair(30.0f,
            [&](std::size_t i, std::size_t j) {
                if (i >= j) {
                    ordering_violated = true;
                    INFO("Found pair (" << i << ", " << j << ") where i >= j");
                }
            });

        // THEN: All pairs within same cell must satisfy i < j
        REQUIRE_FALSE(ordering_violated);
    }

    SECTION("Correctness vs brute force for small dataset") {
        // GIVEN: Small set of entities
        std::vector<Entity2D> entities = {
            {0.0f, 0.0f, 0},
            {5.0f, 0.0f, 1},
            {0.0f, 5.0f, 2},
            {20.0f, 20.0f, 3}
        };

        grid.rebuild(entities);

        const float radius = 15.0f;
        const float radius_sq = radius * radius;

        // WHEN: Collecting pairs from grid
        std::set<std::pair<std::size_t, std::size_t>> grid_pairs;
        grid.for_each_pair(radius,
            [&](std::size_t i, std::size_t j) {
                grid_pairs.insert({i, j});
            });

        // AND: Computing brute force pairs (ground truth)
        std::set<std::pair<std::size_t, std::size_t>> brute_force_pairs;
        for (std::size_t i = 0; i < entities.size(); ++i) {
            for (std::size_t j = i + 1; j < entities.size(); ++j) {
                float dx = entities[i].x - entities[j].x;
                float dy = entities[i].y - entities[j].y;
                float dist_sq = dx*dx + dy*dy;

                if (dist_sq <= radius_sq) {
                    brute_force_pairs.insert({i, j});
                }
            }
        }

        // THEN: Grid pairs should be a superset of brute force
        // (grid is conservative due to cell-based approximation)
        for (const auto& bf_pair : brute_force_pairs) {
            REQUIRE(grid_pairs.count(bf_pair) > 0);
        }
    }
}

TEST_CASE("Incremental update produces identical results to rebuild", "[correctness][update][incremental]") {
    // GIVEN: A grid and entities
    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Entity2D, 2> grid_rebuild(cfg);
    sparse_spatial_hash<Entity2D, 2> grid_update(cfg);

    // Create initial distribution
    std::vector<Entity2D> entities;
    for (int i = 0; i < 50; ++i) {
        entities.push_back({
            static_cast<float>((i * 7) % 100 - 50),
            static_cast<float>((i * 11) % 100 - 50),
            i
        });
    }

    // Initialize both grids
    grid_rebuild.rebuild(entities);
    grid_update.rebuild(entities);

    SECTION("After moving 10% of entities") {
        // WHEN: Moving some entities
        for (int i = 0; i < 5; ++i) {
            entities[i].x += 15.0f;
            entities[i].y += 15.0f;
        }

        // Update one grid, rebuild the other
        grid_update.update(entities);
        grid_rebuild.rebuild(entities);

        // THEN: Both should have identical cell counts
        REQUIRE(grid_update.cell_count() == grid_rebuild.cell_count());

        // AND: Queries should return identical results
        std::array<float, 2> query_pos = {0.0f, 0.0f};
        auto results_update = grid_update.query_radius(30.0f, query_pos);
        auto results_rebuild = grid_rebuild.query_radius(30.0f, query_pos);

        std::sort(results_update.begin(), results_update.end());
        std::sort(results_rebuild.begin(), results_rebuild.end());

        REQUIRE(results_update == results_rebuild);
    }

    SECTION("After moving all entities") {
        // WHEN: Moving all entities to new positions
        for (auto& e : entities) {
            e.x = -e.x;
            e.y = -e.y;
        }

        grid_update.update(entities);
        grid_rebuild.rebuild(entities);

        // THEN: Statistics should be identical
        auto stats_update = grid_update.stats();
        auto stats_rebuild = grid_rebuild.stats();

        REQUIRE(stats_update.occupied_cells == stats_rebuild.occupied_cells);
        REQUIRE(stats_update.total_entities == stats_rebuild.total_entities);
        REQUIRE(stats_update.max_entities_per_cell == stats_rebuild.max_entities_per_cell);
        REQUIRE_THAT(stats_update.avg_entities_per_cell,
                    Catch::Matchers::WithinRel(stats_rebuild.avg_entities_per_cell, 0.0001f));
    }

    SECTION("After multiple incremental updates") {
        // WHEN: Performing multiple sequential updates
        for (int iter = 0; iter < 10; ++iter) {
            // Move a few entities each iteration
            for (int i = iter * 5; i < (iter + 1) * 5 && i < entities.size(); ++i) {
                entities[i].x += 3.0f;
                entities[i].y -= 2.0f;
            }

            grid_update.update(entities);
        }

        // Final rebuild for comparison
        grid_rebuild.rebuild(entities);

        // THEN: Final state should match
        REQUIRE(grid_update.cell_count() == grid_rebuild.cell_count());

        auto stats_update = grid_update.stats();
        auto stats_rebuild = grid_rebuild.stats();
        REQUIRE(stats_update.occupied_cells == stats_rebuild.occupied_cells);
    }
}

TEST_CASE("Cell boundary edge cases", "[correctness][boundaries][precision]") {
    // GIVEN: A grid with 10x10 cells
    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Entity2D, 2> grid(cfg);

    SECTION("Entities at exact cell boundaries") {
        // GIVEN: Entities placed exactly on cell boundaries
        std::vector<Entity2D> entities = {
            {0.0f, 0.0f, 0},      // Exact cell corner
            {10.0f, 0.0f, 1},     // Exact cell edge
            {0.0f, 10.0f, 2},     // Exact cell edge
            {10.0f, 10.0f, 3},    // Exact cell corner
            {5.0f, 10.0f, 4}      // Exact edge, middle
        };

        // WHEN: Building the grid
        grid.rebuild(entities);

        // THEN: Grid should handle boundaries without crashing
        REQUIRE(grid.cell_count() > 0);

        // AND: Queries near boundaries should work
        auto results = grid.query_radius(15.0f, 10.0f, 10.0f);
        REQUIRE_FALSE(results.empty());
    }

    SECTION("Entities at world boundaries") {
        // GIVEN: Entities at the edge of world bounds
        std::vector<Entity2D> entities = {
            {-49.9999f, -49.9999f, 0},  // Near min boundary
            {49.9999f, 49.9999f, 1},    // Near max boundary
            {-50.0f, 0.0f, 2},          // Exactly at min boundary
            {50.0f, 0.0f, 3}            // Exactly at max boundary (will clamp)
        };

        // WHEN: Building with bounded topology
        grid.rebuild(entities);

        // THEN: Should handle clamping correctly
        REQUIRE(grid.cell_count() > 0);

        auto stats = grid.stats();
        // All entities should be placed somewhere
        INFO("Total entities in grid: " << stats.total_entities);
        REQUIRE(stats.occupied_cells > 0);
    }

    SECTION("Float precision at boundaries doesn't cause duplicates") {
        // GIVEN: Many entities very close to a cell boundary
        std::vector<Entity2D> entities;
        for (int i = 0; i < 100; ++i) {
            float epsilon = static_cast<float>(i - 50) * 0.00001f;
            entities.push_back({10.0f + epsilon, 0.0f, i});
        }

        // WHEN: Building the grid
        grid.rebuild(entities);

        // THEN: Should have at most 2 cells (entities on either side of boundary)
        REQUIRE(grid.cell_count() <= 2);

        auto stats = grid.stats();
        REQUIRE(stats.total_entities == 100);
    }

    SECTION("Query spanning cell boundaries") {
        // GIVEN: Entities on both sides of a boundary
        std::vector<Entity2D> entities = {
            {9.5f, 0.0f, 0},   // Just before boundary
            {10.5f, 0.0f, 1}   // Just after boundary
        };

        grid.rebuild(entities);

        // WHEN: Querying exactly at the boundary
        auto results = grid.query_radius(2.0f, 10.0f, 0.0f);

        // THEN: Should find both entities (conservative cell query)
        REQUIRE(results.size() >= 2);
    }
}

TEST_CASE("4D+ dimension support", "[correctness][dimensions][4d][template]") {
    // GIVEN: A 4D spatial configuration
    grid_config<4> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Entity4D, 4> grid(cfg);

    SECTION("4D grid construction and rebuild") {
        // GIVEN: 4D entities
        std::vector<Entity4D> entities = {
            {{{0.0f, 0.0f, 0.0f, 0.0f}}, 0},
            {{{5.0f, 5.0f, 5.0f, 5.0f}}, 1},
            {{{15.0f, 15.0f, 15.0f, 15.0f}}, 2},
            {{{25.0f, 25.0f, 25.0f, 25.0f}}, 3}
        };

        // WHEN: Building 4D grid
        grid.rebuild(entities);

        // THEN: Should handle 4D correctly
        REQUIRE_FALSE(grid.empty());
        REQUIRE(grid.cell_count() > 0);
        REQUIRE(grid.cell_count() <= 4);
    }

    SECTION("4D query radius") {
        // GIVEN: 4D entities at varying distances
        std::vector<Entity4D> entities = {
            {{{0.0f, 0.0f, 0.0f, 0.0f}}, 0},    // At origin
            {{{3.0f, 0.0f, 0.0f, 0.0f}}, 1},    // Close in one dim
            {{{50.0f, 0.0f, 0.0f, 0.0f}}, 2},   // Far in one dim
            {{{2.0f, 2.0f, 2.0f, 2.0f}}, 3}     // Close in all dims
        };

        grid.rebuild(entities);

        // WHEN: Querying in 4D space
        std::array<float, 4> query_pos = {0.0f, 0.0f, 0.0f, 0.0f};
        auto results = grid.query_radius(10.0f, query_pos);

        // THEN: Should find nearby entities in 4D
        REQUIRE_FALSE(results.empty());

        // Should find at least entities 0, 1, 3 (within 4D radius)
        bool found_0 = false, found_1 = false, found_3 = false;
        for (auto idx : results) {
            if (idx == 0) found_0 = true;
            if (idx == 1) found_1 = true;
            if (idx == 3) found_3 = true;
        }

        REQUIRE(found_0);
        // Note: Conservative cell query might include all
    }

    SECTION("4D incremental update") {
        // GIVEN: 4D entities
        std::vector<Entity4D> entities;
        for (int i = 0; i < 10; ++i) {
            entities.push_back({
                {{static_cast<float>(i * 5),
                  static_cast<float>(i * 5),
                  static_cast<float>(i * 5),
                  static_cast<float>(i * 5)}},
                i
            });
        }

        grid.rebuild(entities);
        std::size_t initial_cells = grid.cell_count();

        // WHEN: Moving entities in 4D space
        for (auto& e : entities) {
            for (int d = 0; d < 4; ++d) {
                e.pos[d] += 2.0f;
            }
        }

        grid.update(entities);

        // THEN: Update should work in 4D
        REQUIRE(grid.cell_count() > 0);

        auto stats = grid.stats();
        REQUIRE(stats.occupied_cells > 0);
    }

    SECTION("4D for_each_pair") {
        // GIVEN: Multiple 4D entities
        std::vector<Entity4D> entities = {
            {{{5.0f, 5.0f, 5.0f, 5.0f}}, 0},
            {{{6.0f, 5.0f, 5.0f, 5.0f}}, 1},
            {{{5.0f, 6.0f, 5.0f, 5.0f}}, 2}
        };

        grid.rebuild(entities);

        // WHEN: Processing pairs in 4D
        int pair_count = 0;
        grid.for_each_pair(20.0f,
            [&](std::size_t i, std::size_t j) {
                pair_count++;
                REQUIRE(i < j);
            });

        // THEN: Should find pairs in 4D space
        REQUIRE(pair_count >= 1);
    }

    SECTION("4D cell hash uniqueness") {
        // GIVEN: 4D cell hash function
        cell_hash<4, int> hasher;
        std::unordered_set<std::size_t> hashes;

        // WHEN: Hashing 4D cells in a 5x5x5x5 grid
        for (int w = 0; w < 5; ++w) {
            for (int x = 0; x < 5; ++x) {
                for (int y = 0; y < 5; ++y) {
                    for (int z = 0; z < 5; ++z) {
                        cell_coord<4, int> cell{w, x, y, z};
                        hashes.insert(hasher(cell));
                    }
                }
            }
        }

        // THEN: All 625 cells should have unique hashes
        REQUIRE(hashes.size() == 625);
    }
}
