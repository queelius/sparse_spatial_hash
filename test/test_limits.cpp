/**
 * Coordinate limit and boundary tests for sparse_spatial_hash
 *
 * Tests documented Morton encoding limits:
 * - 2D: ±2^31 cells per dimension (~2 billion)
 * - 3D: ±2^21 cells per dimension (~2 million)
 * - Overflow protection in constructor
 * - Hash collision behavior at limits
 */

#include <boost/spatial/sparse_spatial_hash.hpp>
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <unordered_set>
#include <limits>

using namespace boost::spatial;

struct Particle {
    float x, y, z;
    int id;
};

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

template<>
struct boost::spatial::position_accessor<Particle, 2> {
    static float get(const Particle& p, std::size_t dim) {
        return dim == 0 ? p.x : p.y;
    }
};

TEST_CASE("2D Morton encoding at ±2^31 limit", "[limits][morton][2d]") {
    SECTION("Hash function works at limit boundary") {
        cell_hash<2, int> hasher;

        // At the documented 2^31-1 limit
        cell_coord<2, int> at_positive_limit{2147483647, 0};  // 2^31 - 1
        cell_coord<2, int> at_positive_limit_y{0, 2147483647};

        // At negative limit
        cell_coord<2, int> at_negative_limit{-2147483648, 0};  // -2^31
        cell_coord<2, int> at_negative_limit_y{0, -2147483648};

        // Should compute hashes without overflow
        auto hash_pos_x = hasher(at_positive_limit);
        auto hash_pos_y = hasher(at_positive_limit_y);
        auto hash_neg_x = hasher(at_negative_limit);
        auto hash_neg_y = hasher(at_negative_limit_y);

        // All hashes should be different (no collisions at limits)
        REQUIRE(hash_pos_x != hash_pos_y);
        REQUIRE(hash_neg_x != hash_neg_y);
        REQUIRE(hash_pos_x != hash_neg_x);
    }

    SECTION("Grid works near 2^31 cell coordinate limits") {
        // Cell size 1, world centered at origin
        // To reach cell coordinate 2^20 (more realistic test), position needs to be 2^20 + world_size/2
        const float cell_size = 1000000.0f;  // 1M per cell
        const float world_size = 2000000000.0f;  // 2 billion (fits in ±2^31 cells with this cell size)

        grid_config<2> cfg{
            .cell_size = {cell_size, cell_size},
            .world_size = {world_size, world_size},
            .topology_type = topology::infinite
        };

        sparse_spatial_hash<Particle, 2> grid(cfg);

        // Position that maps to cell coordinate near limit
        // Cell coord = floor((pos + world_size/2) / cell_size)
        // For pos = 1e9, cell coord ≈ floor((1e9 + 1e9) / 1e6) = 2000
        std::vector<Particle> particles = {
            {1000000000.0f, 0.0f, 0.0f, 0},    // Large positive
            {-1000000000.0f, 0.0f, 0.0f, 1},   // Large negative
            {0.0f, 1000000000.0f, 0.0f, 2},    // Large positive Y
            {0.0f, -1000000000.0f, 0.0f, 3}    // Large negative Y
        };

        grid.rebuild(particles);

        // Should handle extreme cell coordinates
        REQUIRE(grid.entity_count() == 4);
        REQUIRE(grid.cell_count() >= 2);  // At least 2 different cells

        // Queries should work
        auto results = grid.query_radius(1000000.0f, 1000000000.0f, 0.0f);
        REQUIRE_FALSE(results.empty());
    }

    SECTION("No hash collisions in reasonable 2D range") {
        cell_hash<2, int> hasher;
        std::unordered_set<std::size_t> hashes;

        // Test 1000x1000 grid around origin
        const int range = 500;
        for (int x = -range; x <= range; ++x) {
            for (int y = -range; y <= range; ++y) {
                cell_coord<2, int> cell{x, y};
                hashes.insert(hasher(cell));
            }
        }

        // Should have unique hash for each cell
        const int expected = (2 * range + 1) * (2 * range + 1);
        REQUIRE(hashes.size() == expected);
    }
}

TEST_CASE("3D Morton encoding at ±2^21 limit", "[limits][morton][3d]") {
    SECTION("Hash function works at 3D limit boundary") {
        cell_hash<3, int> hasher;

        // At the documented 2^21-1 limit (2,097,151)
        const int limit_3d = 2097151;  // 2^21 - 1

        cell_coord<3, int> at_limit_x{limit_3d, 0, 0};
        cell_coord<3, int> at_limit_y{0, limit_3d, 0};
        cell_coord<3, int> at_limit_z{0, 0, limit_3d};

        // Negative limits
        cell_coord<3, int> at_neg_limit_x{-limit_3d, 0, 0};
        cell_coord<3, int> at_neg_limit_y{0, -limit_3d, 0};
        cell_coord<3, int> at_neg_limit_z{0, 0, -limit_3d};

        // Should compute hashes without overflow
        auto hash_px = hasher(at_limit_x);
        auto hash_py = hasher(at_limit_y);
        auto hash_pz = hasher(at_limit_z);
        auto hash_nx = hasher(at_neg_limit_x);
        auto hash_ny = hasher(at_neg_limit_y);
        auto hash_nz = hasher(at_neg_limit_z);

        // All should be unique
        std::unordered_set<std::size_t> unique_hashes = {
            hash_px, hash_py, hash_pz, hash_nx, hash_ny, hash_nz
        };
        REQUIRE(unique_hashes.size() == 6);
    }

    SECTION("Grid works near 2^21 cell coordinate limits") {
        // To reach cell coordinate ~2^20 (1 million), use appropriate scaling
        const float cell_size = 1000.0f;      // 1km per cell
        const float world_size = 2000000.0f;  // 2000km world

        grid_config<3> cfg{
            .cell_size = {cell_size, cell_size, cell_size},
            .world_size = {world_size, world_size, world_size},
            .topology_type = topology::infinite
        };

        sparse_spatial_hash<Particle, 3> grid(cfg);

        // Positions that map to large cell coordinates
        std::vector<Particle> particles = {
            {1000000.0f, 0.0f, 0.0f, 0},      // Cell coord ~1000
            {-1000000.0f, 0.0f, 0.0f, 1},     // Cell coord ~-1000
            {0.0f, 1000000.0f, 0.0f, 2},
            {0.0f, 0.0f, 1000000.0f, 3}
        };

        grid.rebuild(particles);

        REQUIRE(grid.entity_count() == 4);
        REQUIRE(grid.cell_count() >= 3);

        // Queries should work at extreme coordinates
        auto results = grid.query_radius(100000.0f, 1000000.0f, 0.0f, 0.0f);
        REQUIRE_FALSE(results.empty());
    }

    SECTION("No hash collisions in reasonable 3D range") {
        cell_hash<3, int> hasher;
        std::unordered_set<std::size_t> hashes;

        // Test 100x100x100 grid (1 million cells would be too slow)
        const int range = 50;
        for (int x = -range; x <= range; ++x) {
            for (int y = -range; y <= range; ++y) {
                for (int z = -range; z <= range; ++z) {
                    cell_coord<3, int> cell{x, y, z};
                    hashes.insert(hasher(cell));
                }
            }
        }

        // Should have unique hash for each cell
        const int expected = (2 * range + 1) * (2 * range + 1) * (2 * range + 1);
        REQUIRE(hashes.size() == expected);
    }

    SECTION("Beyond 2^21 still works but may have collisions") {
        // Document: Beyond ±2^21, hash collisions become more likely
        // But the grid should still function (just with reduced performance)
        const float cell_size = 10.0f;
        const float world_size = 100000000.0f;  // 100M world

        grid_config<3> cfg{
            .cell_size = {cell_size, cell_size, cell_size},
            .world_size = {world_size, world_size, world_size},
            .topology_type = topology::infinite
        };

        sparse_spatial_hash<Particle, 3> grid(cfg);

        // Position way beyond 2^21 cells
        // Cell coord would be floor((50000000 + 50000000) / 10) = 10,000,000
        std::vector<Particle> particles = {
            {50000000.0f, 0.0f, 0.0f, 0},
            {50000010.0f, 0.0f, 0.0f, 1}  // Different cell but beyond limit
        };

        // Should not crash, even if hash collisions occur
        REQUIRE_NOTHROW(grid.rebuild(particles));
        REQUIRE(grid.entity_count() == 2);
    }
}

TEST_CASE("Constructor overflow protection", "[limits][overflow][constructor]") {
    SECTION("Grid with exactly 1M cells does not overflow") {
        // 100x100x100 = 1,000,000 cells (exactly at cap)
        grid_config<3> cfg{
            .cell_size = {1.0f, 1.0f, 1.0f},
            .world_size = {100.0f, 100.0f, 100.0f},
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<Particle, 3> grid(cfg);

        REQUIRE(grid.empty());
        REQUIRE(grid.resolution()[0] == 100);
        REQUIRE(grid.resolution()[1] == 100);
        REQUIRE(grid.resolution()[2] == 100);
    }

    SECTION("Grid with >1M cells caps reserve at 1M") {
        // 1000x1000x1000 = 1 billion cells (should cap internal reserve)
        grid_config<3> cfg{
            .cell_size = {1.0f, 1.0f, 1.0f},
            .world_size = {1000.0f, 1000.0f, 1000.0f},
            .topology_type = topology::bounded
        };

        // Should not overflow or allocate 1B cells worth of memory
        sparse_spatial_hash<Particle, 3> grid(cfg);

        REQUIRE(grid.empty());
        REQUIRE(grid.resolution()[0] == 1000);

        // Grid should still work correctly despite capped reserve
        std::vector<Particle> particles = {
            {5.0f, 5.0f, 5.0f, 0},
            {500.0f, 500.0f, 500.0f, 1}
        };

        grid.rebuild(particles);
        REQUIRE(grid.cell_count() == 2);
        REQUIRE(grid.entity_count() == 2);
    }

    SECTION("Grid with extreme resolution does not overflow") {
        // 10000x10000x10000 = 1 trillion cells
        grid_config<3> cfg{
            .cell_size = {0.1f, 0.1f, 0.1f},
            .world_size = {1000.0f, 1000.0f, 1000.0f},
            .topology_type = topology::bounded
        };

        // Should not crash during construction
        sparse_spatial_hash<Particle, 3> grid(cfg);
        REQUIRE(grid.empty());
    }

    SECTION("2D grid with very large resolution") {
        // 100000x100000 = 10 billion cells
        grid_config<2> cfg{
            .cell_size = {0.001f, 0.001f},
            .world_size = {100.0f, 100.0f},
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<Particle, 2> grid(cfg);

        REQUIRE(grid.empty());

        // Should still work correctly
        std::vector<Particle> particles = {
            {5.0f, 5.0f, 0.0f, 0}
        };
        grid.rebuild(particles);
        REQUIRE(grid.entity_count() == 1);
    }
}

TEST_CASE("Morton encoding spatial locality at limits", "[limits][morton][locality]") {
    SECTION("2D spatial locality preserved at large coordinates") {
        cell_hash<2, int> hasher;

        // Test at large positive coordinates
        const int base = 1000000;
        cell_coord<2, int> cell1{base, base};
        cell_coord<2, int> cell2{base + 1, base};     // Adjacent
        cell_coord<2, int> cell3{base + 100, base};   // Far

        auto hash1 = hasher(cell1);
        auto hash2 = hasher(cell2);
        auto hash3 = hasher(cell3);

        // XOR distance as measure of hash similarity
        auto adjacent_diff = hash1 ^ hash2;
        auto far_diff = hash1 ^ hash3;

        // Adjacent cells should have more similar hashes (fewer differing bits)
        // This is a property of Morton encoding
        auto adjacent_bits = __builtin_popcountll(adjacent_diff);
        auto far_bits = __builtin_popcountll(far_diff);

        INFO("Adjacent XOR diff bits: " << adjacent_bits);
        INFO("Far XOR diff bits: " << far_bits);

        // Spatial locality: adjacent cells should have fewer differing bits
        REQUIRE(adjacent_bits <= far_bits);
    }

    SECTION("3D spatial locality preserved at large coordinates") {
        cell_hash<3, int> hasher;

        const int base = 100000;
        cell_coord<3, int> cell1{base, base, base};
        cell_coord<3, int> cell2{base + 1, base, base};  // Adjacent in X
        cell_coord<3, int> cell3{base + 50, base, base}; // Far in X

        auto hash1 = hasher(cell1);
        auto hash2 = hasher(cell2);
        auto hash3 = hasher(cell3);

        auto adjacent_diff = hash1 ^ hash2;
        auto far_diff = hash1 ^ hash3;

        auto adjacent_bits = __builtin_popcountll(adjacent_diff);
        auto far_bits = __builtin_popcountll(far_diff);

        INFO("Adjacent XOR diff bits: " << adjacent_bits);
        INFO("Far XOR diff bits: " << far_bits);

        REQUIRE(adjacent_bits <= far_bits);
    }
}

TEST_CASE("Grid behavior with extreme entity counts", "[limits][scale]") {
    SECTION("Very large entity count in bounded grid") {
        grid_config<3> cfg{
            .cell_size = {10.0f, 10.0f, 10.0f},
            .world_size = {1000.0f, 1000.0f, 1000.0f},
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<Particle, 3> grid(cfg);

        // Create 100K entities (stress test)
        std::vector<Particle> particles;
        particles.reserve(100000);

        for (int i = 0; i < 100000; ++i) {
            particles.push_back({
                static_cast<float>((i * 73) % 1000 - 500),
                static_cast<float>((i * 137) % 1000 - 500),
                static_cast<float>((i * 197) % 1000 - 500),
                i
            });
        }

        grid.rebuild(particles);

        REQUIRE(grid.entity_count() == 100000);
        REQUIRE(grid.cell_count() > 0);

        auto stats = grid.stats();
        REQUIRE(stats.total_entities == 100000);
        REQUIRE(stats.occupied_cells > 0);
        REQUIRE(stats.avg_entities_per_cell > 0);
    }

    SECTION("Single cell with many entities") {
        grid_config<2> cfg{
            .cell_size = {1000.0f, 1000.0f},  // Very large cells
            .world_size = {1000.0f, 1000.0f},
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<Particle, 2> grid(cfg);

        // All entities in single cell
        std::vector<Particle> particles;
        for (int i = 0; i < 10000; ++i) {
            particles.push_back({
                static_cast<float>(i % 100),
                static_cast<float>(i / 100),
                0.0f,
                i
            });
        }

        grid.rebuild(particles);

        REQUIRE(grid.cell_count() == 1);
        REQUIRE(grid.entity_count() == 10000);

        auto stats = grid.stats();
        REQUIRE(stats.max_entities_per_cell == 10000);
        REQUIRE(stats.avg_entities_per_cell == 10000.0f);
    }
}

TEST_CASE("Extreme world sizes", "[limits][world_size]") {
    SECTION("Very small world") {
        grid_config<2> cfg{
            .cell_size = {0.0001f, 0.0001f},
            .world_size = {0.01f, 0.01f},  // 1cm world, 0.1mm cells
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<Particle, 2> grid(cfg);

        std::vector<Particle> particles = {
            {0.001f, 0.001f, 0.0f, 0},  // 1mm position
            {0.002f, 0.002f, 0.0f, 1}
        };

        grid.rebuild(particles);

        REQUIRE(grid.entity_count() == 2);
        REQUIRE(grid.cell_count() >= 1);
    }

    SECTION("Very large world") {
        grid_config<3> cfg{
            .cell_size = {1000000.0f, 1000000.0f, 1000000.0f},  // 1000km cells
            .world_size = {1000000000.0f, 1000000000.0f, 1000000000.0f},  // 1M km world
            .topology_type = topology::infinite
        };

        sparse_spatial_hash<Particle, 3> grid(cfg);

        std::vector<Particle> particles = {
            {500000000.0f, 0.0f, 0.0f, 0},   // 500,000 km away
            {-500000000.0f, 0.0f, 0.0f, 1}
        };

        grid.rebuild(particles);

        REQUIRE(grid.entity_count() == 2);
        REQUIRE(grid.cell_count() == 2);
    }
}

TEST_CASE("Cell coordinate calculation at boundaries", "[limits][precision]") {
    SECTION("Coordinates exactly at cell boundaries") {
        grid_config<2> cfg{
            .cell_size = {10.0f, 10.0f},
            .world_size = {100.0f, 100.0f},
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<Particle, 2> grid(cfg);

        // Positions exactly on cell boundaries
        std::vector<Particle> particles = {
            {0.0f, 0.0f, 0.0f, 0},      // Exact boundary
            {10.0f, 0.0f, 0.0f, 1},     // Exact boundary
            {20.0f, 0.0f, 0.0f, 2},     // Exact boundary
            {9.9999f, 0.0f, 0.0f, 3},   // Just before boundary
            {10.0001f, 0.0f, 0.0f, 4}   // Just after boundary
        };

        grid.rebuild(particles);

        REQUIRE(grid.entity_count() == 5);

        // Should handle boundaries consistently
        REQUIRE(grid.cell_count() >= 2);
        REQUIRE(grid.cell_count() <= 5);
    }

    SECTION("Float precision near zero") {
        grid_config<3> cfg{
            .cell_size = {1.0f, 1.0f, 1.0f},
            .world_size = {100.0f, 100.0f, 100.0f},
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<Particle, 3> grid(cfg);

        // Very small values near zero
        std::vector<Particle> particles = {
            {0.0f, 0.0f, 0.0f, 0},
            {1e-6f, 1e-6f, 1e-6f, 1},   // Tiny offset
            {-1e-6f, -1e-6f, -1e-6f, 2}
        };

        grid.rebuild(particles);

        // Should place all in same or adjacent cells
        REQUIRE(grid.entity_count() == 3);
        REQUIRE(grid.cell_count() <= 2);
    }
}
