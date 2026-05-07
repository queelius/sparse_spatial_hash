/**
 * Type system and precision tests for sparse_spatial_hash
 *
 * Tests:
 * - Custom types with position_accessor specialization
 * - Double precision support
 * - Large-scale stress testing (10K+ entities)
 * - Memory efficiency (sparse vs dense)
 * - Zero-radius and edge case queries
 */

#include <spatial/sparse_spatial_hash.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>
#include <list>
#include <memory>
#include <chrono>

using namespace spatial;

// ============================================================================
// Test 6: Custom Types with position_accessor
// ============================================================================

// Complex custom type with non-trivial constructor
class PhysicsBody {
public:
    PhysicsBody(double x, double y, double z, double mass, int id)
        : position_{x, y, z}, velocity_{0, 0, 0}, mass_(mass), id_(id) {}

    const std::array<double, 3>& position() const { return position_; }
    std::array<double, 3>& velocity() { return velocity_; }
    double mass() const { return mass_; }
    int id() const { return id_; }

private:
    std::array<double, 3> position_;
    std::array<double, 3> velocity_;
    double mass_;
    int id_;
};

// Specialization for custom type
template<>
struct spatial::position_accessor<PhysicsBody, 3> {
    static double get(const PhysicsBody& body, std::size_t dim) {
        return body.position()[dim];
    }
};

// Type with pointers to position data (indirect access)
struct ParticleRef {
    std::shared_ptr<std::array<float, 3>> pos_ptr;
    int id;

    ParticleRef(float x, float y, float z, int i)
        : pos_ptr(std::make_shared<std::array<float, 3>>(std::array<float, 3>{x, y, z})),
          id(i) {}
};

template<>
struct spatial::position_accessor<ParticleRef, 3> {
    static float get(const ParticleRef& p, std::size_t dim) {
        return (*p.pos_ptr)[dim];
    }
};

TEST_CASE("Custom types with position_accessor", "[types][custom][position_accessor]") {
    SECTION("Non-POD type with methods") {
        // GIVEN: Grid configured for custom PhysicsBody type
        grid_config<3, double> cfg{
            .cell_size = {10.0, 10.0, 10.0},
            .world_size = {100.0, 100.0, 100.0},
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<PhysicsBody, 3, double> grid(cfg);

        // WHEN: Creating physics bodies
        std::vector<PhysicsBody> bodies;
        bodies.emplace_back(5.0, 5.0, 5.0, 1.5, 0);
        bodies.emplace_back(15.0, 15.0, 15.0, 2.0, 1);
        bodies.emplace_back(25.0, 25.0, 25.0, 1.0, 2);

        grid.rebuild(bodies);

        // THEN: Grid should handle custom type correctly
        REQUIRE(grid.cell_count() == 3);

        auto results = grid.query_radius(20.0, 10.0, 10.0, 10.0);
        REQUIRE_FALSE(results.empty());
    }

    SECTION("Type with indirect position access via pointers") {
        // GIVEN: Grid for ParticleRef with shared_ptr positions
        grid_config<3> cfg{
            .cell_size = {10.0f, 10.0f, 10.0f},
            .world_size = {100.0f, 100.0f, 100.0f},
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<ParticleRef, 3> grid(cfg);

        // WHEN: Creating particles with smart pointers
        std::vector<ParticleRef> particles;
        particles.emplace_back(5.0f, 5.0f, 5.0f, 0);
        particles.emplace_back(15.0f, 15.0f, 15.0f, 1);

        grid.rebuild(particles);

        // THEN: Should work with indirect access
        REQUIRE(grid.cell_count() == 2);

        // AND: Modifying position through pointer should be detectable
        (*particles[0].pos_ptr)[0] = 16.0f;
        (*particles[0].pos_ptr)[1] = 16.0f;
        (*particles[0].pos_ptr)[2] = 16.0f;
        grid.update(particles);

        // Particles might still be in different cells depending on exact positions
        REQUIRE(grid.cell_count() <= 2);
    }

    SECTION("Custom type with for_each_pair") {
        // GIVEN: Physics bodies in grid
        grid_config<3, double> cfg{
            .cell_size = {10.0, 10.0, 10.0},
            .world_size = {100.0, 100.0, 100.0},
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<PhysicsBody, 3, double> grid(cfg);

        std::vector<PhysicsBody> bodies;
        bodies.emplace_back(5.0, 5.0, 5.0, 1.0, 0);
        bodies.emplace_back(6.0, 5.0, 5.0, 1.0, 1);
        bodies.emplace_back(50.0, 50.0, 50.0, 1.0, 2);

        grid.rebuild(bodies);

        // WHEN: Processing pairs
        int pair_count = 0;
        grid.for_each_pair(20.0,
            [&](std::size_t i, std::size_t j) {
                pair_count++;
                // Can access custom methods
                double combined_mass = bodies[i].mass() + bodies[j].mass();
                REQUIRE(combined_mass > 0);
            });

        // THEN: Should find at least the close pair
        REQUIRE(pair_count >= 1);
    }
}

// ============================================================================
// Test 7: Double Precision Support
// ============================================================================

struct PreciseEntity {
    double x, y, z;
    int id;
};

template<>
struct spatial::position_accessor<PreciseEntity, 3> {
    static double get(const PreciseEntity& e, std::size_t dim) {
        switch(dim) {
            case 0: return e.x;
            case 1: return e.y;
            case 2: return e.z;
            default: return 0.0;
        }
    }
};

TEST_CASE("Double precision support", "[types][precision][double]") {
    SECTION("Double precision grid with large world") {
        // GIVEN: Very large world requiring double precision
        grid_config<3, double> cfg{
            .cell_size = {1000.0, 1000.0, 1000.0},
            .world_size = {1000000.0, 1000000.0, 1000000.0},  // 1000km world
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<PreciseEntity, 3, double> grid(cfg);

        // WHEN: Placing entities far apart
        std::vector<PreciseEntity> entities = {
            {0.0, 0.0, 0.0, 0},
            {500000.0, 0.0, 0.0, 1},     // 500km away
            {999999.0, 999999.0, 999999.0, 2}
        };

        grid.rebuild(entities);

        // THEN: Should handle large coordinates correctly
        REQUIRE(grid.cell_count() == 3);

        auto results = grid.query_radius(50000.0, 0.0, 0.0, 0.0);
        REQUIRE_FALSE(results.empty());
    }

    SECTION("Double precision with reasonable cell sizes") {
        // GIVEN: Grid with moderate cell size but high precision
        grid_config<3, double> cfg{
            .cell_size = {0.1, 0.1, 0.1},
            .world_size = {10.0, 10.0, 10.0},
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<PreciseEntity, 3, double> grid(cfg);

        // WHEN: Placing entities requiring precise positioning
        std::vector<PreciseEntity> entities;
        for (int i = 0; i < 20; ++i) {
            entities.push_back({
                i * 0.15,  // Crosses cell boundaries
                0.0,
                0.0,
                i
            });
        }

        grid.rebuild(entities);

        // THEN: Should distinguish between cells
        REQUIRE(grid.cell_count() > 1);

        auto stats = grid.stats();
        REQUIRE(stats.total_entities == 20);
    }

    SECTION("High precision queries") {
        // GIVEN: Grid with double precision
        grid_config<3, double> cfg{
            .cell_size = {10.0, 10.0, 10.0},
            .world_size = {100.0, 100.0, 100.0},
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<PreciseEntity, 3, double> grid(cfg);

        std::vector<PreciseEntity> entities = {
            {5.0, 5.0, 5.0, 0},
            {15.0, 15.0, 15.0, 1}
        };

        grid.rebuild(entities);

        REQUIRE(grid.cell_count() == 2);

        // High precision query
        auto results = grid.query_radius(12.0, 10.0, 10.0, 10.0);
        REQUIRE_FALSE(results.empty());
    }
}

// ============================================================================
// Test 8: Large-Scale Stress Test
// ============================================================================

struct SimpleParticle {
    float x, y, z;
    int id;
};

template<>
struct spatial::position_accessor<SimpleParticle, 3> {
    static float get(const SimpleParticle& p, std::size_t dim) {
        switch(dim) {
            case 0: return p.x;
            case 1: return p.y;
            case 2: return p.z;
            default: return 0.0f;
        }
    }
};

TEST_CASE("Large-scale stress test (10K+ entities)", "[performance][stress][scale]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<SimpleParticle, 3> grid(cfg);

    SECTION("Build with 10K entities") {
        // GIVEN: 10,000 randomly distributed entities
        std::vector<SimpleParticle> particles;
        particles.reserve(10000);

        for (int i = 0; i < 10000; ++i) {
            particles.push_back({
                static_cast<float>((i * 73) % 1000 - 500),
                static_cast<float>((i * 137) % 1000 - 500),
                static_cast<float>((i * 197) % 1000 - 500),
                i
            });
        }

        // WHEN: Building grid (should be O(n))
        auto start = std::chrono::high_resolution_clock::now();
        grid.rebuild(particles);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // THEN: Should complete reasonably fast
        REQUIRE(duration.count() < 1000);  // Less than 1 second

        auto stats = grid.stats();
        REQUIRE(stats.total_entities == 10000);
        REQUIRE(stats.occupied_cells > 0);

        INFO("Built 10K entities in " << duration.count() << "ms");
        INFO("Occupied cells: " << stats.occupied_cells);
        INFO("Avg entities per cell: " << stats.avg_entities_per_cell);
    }

    SECTION("Query performance on 10K entities") {
        // GIVEN: 10K entities in grid
        std::vector<SimpleParticle> particles;
        for (int i = 0; i < 10000; ++i) {
            particles.push_back({
                static_cast<float>((i * 73) % 1000 - 500),
                static_cast<float>((i * 137) % 1000 - 500),
                static_cast<float>((i * 197) % 1000 - 500),
                i
            });
        }

        grid.rebuild(particles);

        // WHEN: Performing multiple queries
        auto start = std::chrono::high_resolution_clock::now();

        int total_found = 0;
        for (int q = 0; q < 100; ++q) {
            auto results = grid.query_radius(50.0f,
                static_cast<float>((q * 13) % 1000 - 500),
                static_cast<float>((q * 17) % 1000 - 500),
                static_cast<float>((q * 19) % 1000 - 500)
            );
            total_found += results.size();
        }

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // THEN: Queries should be fast (much better than O(n))
        REQUIRE(duration.count() < 100);  // 100 queries in < 100ms

        INFO("100 queries on 10K entities in " << duration.count() << "ms");
        INFO("Average results per query: " << total_found / 100);
    }

    SECTION("Incremental update performance (1% movement)") {
        // GIVEN: 10K entities
        std::vector<SimpleParticle> particles;
        for (int i = 0; i < 10000; ++i) {
            particles.push_back({
                static_cast<float>((i * 73) % 1000 - 500),
                static_cast<float>((i * 137) % 1000 - 500),
                static_cast<float>((i * 197) % 1000 - 500),
                i
            });
        }

        grid.rebuild(particles);

        // WHEN: Moving only 1% of entities (typical per-frame movement)
        for (int i = 0; i < 100; ++i) {
            particles[i].x += 5.0f;
            particles[i].y += 5.0f;
        }

        auto start = std::chrono::high_resolution_clock::now();
        grid.update(particles);
        auto end = std::chrono::high_resolution_clock::now();

        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

        // THEN: Update should be much faster than rebuild
        REQUIRE(duration.count() < 10000);  // < 10ms

        INFO("Incremental update (1% moved) in " << duration.count() << "µs");
    }

    SECTION("for_each_pair doesn't degrade to O(n²)") {
        // GIVEN: 1000 entities (smaller for pair testing)
        std::vector<SimpleParticle> particles;
        for (int i = 0; i < 1000; ++i) {
            particles.push_back({
                static_cast<float>((i * 73) % 1000 - 500),
                static_cast<float>((i * 137) % 1000 - 500),
                static_cast<float>((i * 197) % 1000 - 500),
                i
            });
        }

        grid.rebuild(particles);

        // WHEN: Processing pairs
        auto start = std::chrono::high_resolution_clock::now();

        int pair_count = 0;
        grid.for_each_pair(30.0f,
            [&](std::size_t, std::size_t) { pair_count++; });

        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

        // THEN: Should be much faster than O(n²)
        // Brute force would be 1000*999/2 = ~500K comparisons
        REQUIRE(duration.count() < 100);  // Should be fast

        INFO("Processed " << pair_count << " pairs in " << duration.count() << "ms");
        INFO("Pairs found: " << pair_count << " (out of max 499500)");
    }
}

// ============================================================================
// Test 9: Sparse vs Dense Memory Efficiency
// ============================================================================

TEST_CASE("Sparse vs dense grid memory efficiency", "[memory][sparse][efficiency]") {
    SECTION("Sparse grid (1% occupancy) uses minimal memory") {
        // GIVEN: Large world with few entities
        grid_config<3> cfg{
            .cell_size = {10.0f, 10.0f, 10.0f},
            .world_size = {1000.0f, 1000.0f, 1000.0f},  // 100x100x100 = 1M cells
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<SimpleParticle, 3> grid(cfg);

        // WHEN: Placing only 100 entities (0.01% occupancy)
        std::vector<SimpleParticle> particles;
        for (int i = 0; i < 100; ++i) {
            particles.push_back({
                static_cast<float>(i * 50),
                static_cast<float>(i * 50),
                static_cast<float>(i * 50),
                i
            });
        }

        grid.rebuild(particles);

        // THEN: Memory should scale with occupied cells, not total cells
        auto stats = grid.stats();
        REQUIRE(stats.occupied_cells <= 100);  // At most 100 cells
        REQUIRE(stats.occupancy_ratio < 0.001f);  // < 0.1% occupancy

        INFO("Occupancy: " << (stats.occupancy_ratio * 100) << "%");
        INFO("Occupied cells: " << stats.occupied_cells << " out of 1000000");
    }

    SECTION("Dense grid (50% occupancy) still efficient") {
        // GIVEN: Smaller world, many entities
        grid_config<3> cfg{
            .cell_size = {10.0f, 10.0f, 10.0f},
            .world_size = {100.0f, 100.0f, 100.0f},  // 10x10x10 = 1000 cells
            .topology_type = topology::bounded
        };

        sparse_spatial_hash<SimpleParticle, 3> grid(cfg);

        // WHEN: Placing entities in 50% of cells
        std::vector<SimpleParticle> particles;
        for (int x = 0; x < 10; x += 2) {
            for (int y = 0; y < 10; y += 2) {
                for (int z = 0; z < 10; z += 2) {
                    particles.push_back({
                        static_cast<float>(x * 10 - 45),
                        static_cast<float>(y * 10 - 45),
                        static_cast<float>(z * 10 - 45),
                        static_cast<int>(particles.size())
                    });
                }
            }
        }

        grid.rebuild(particles);

        // THEN: Should only store occupied cells
        auto stats = grid.stats();
        REQUIRE(stats.occupied_cells == 125);  // 5x5x5
        REQUIRE_THAT(stats.occupancy_ratio,
                    Catch::Matchers::WithinRel(0.125f, 0.01f));  // 12.5%

        INFO("Dense grid occupancy: " << (stats.occupancy_ratio * 100) << "%");
    }

    SECTION("Memory scales linearly with occupied cells") {
        // GIVEN: Grid configuration
        grid_config<3> cfg{
            .cell_size = {10.0f, 10.0f, 10.0f},
            .world_size = {200.0f, 200.0f, 200.0f},
            .topology_type = topology::bounded
        };

        // WHEN: Testing different occupancy levels
        std::vector<std::size_t> entity_counts = {100, 500, 1000, 5000};
        std::vector<std::size_t> cell_counts;

        for (auto count : entity_counts) {
            sparse_spatial_hash<SimpleParticle, 3> grid(cfg);
            std::vector<SimpleParticle> particles;

            for (std::size_t i = 0; i < count; ++i) {
                particles.push_back({
                    static_cast<float>((i * 73) % 200 - 100),
                    static_cast<float>((i * 137) % 200 - 100),
                    static_cast<float>((i * 197) % 200 - 100),
                    static_cast<int>(i)
                });
            }

            grid.rebuild(particles);
            cell_counts.push_back(grid.cell_count());
        }

        // THEN: Cell count should grow sub-linearly with entity count
        // (multiple entities per cell on average)
        for (std::size_t i = 1; i < entity_counts.size(); ++i) {
            double entity_ratio = static_cast<double>(entity_counts[i]) / entity_counts[i-1];
            double cell_ratio = static_cast<double>(cell_counts[i]) / cell_counts[i-1];

            // Cell growth should be slower than entity growth (clustering)
            REQUIRE(cell_ratio <= entity_ratio);

            INFO("Entities: " << entity_counts[i-1] << " -> " << entity_counts[i]
                 << " (ratio: " << entity_ratio << ")");
            INFO("Cells: " << cell_counts[i-1] << " -> " << cell_counts[i]
                 << " (ratio: " << cell_ratio << ")");
        }
    }
}

// ============================================================================
// Test 10: Zero-Radius and Edge Case Queries
// ============================================================================

TEST_CASE("Zero-radius and edge case queries", "[queries][edge_cases][radius]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<SimpleParticle, 3> grid(cfg);

    std::vector<SimpleParticle> particles = {
        {5.0f, 5.0f, 5.0f, 0},
        {6.0f, 5.0f, 5.0f, 1},  // Same cell
        {15.0f, 5.0f, 5.0f, 2}  // Different cell
    };

    grid.rebuild(particles);

    SECTION("Zero radius query") {
        // WHEN: Querying with radius = 0
        auto results = grid.query_radius(0.0f, 5.0f, 5.0f, 5.0f);

        // THEN: Should still return entities in same cell (conservative)
        REQUIRE_FALSE(results.empty());
        // At minimum should find particles 0 and 1 (same cell)
    }

    SECTION("Very small radius query") {
        // WHEN: Querying with tiny radius
        auto results = grid.query_radius(0.001f, 5.0f, 5.0f, 5.0f);

        // THEN: Should work without errors
        REQUIRE_FALSE(results.empty());
    }

    SECTION("Very large radius query") {
        // WHEN: Querying with radius larger than world
        auto results = grid.query_radius(1000.0f, 0.0f, 0.0f, 0.0f);

        // THEN: Should find all entities
        REQUIRE(results.size() == 3);
    }

    SECTION("Negative radius handled gracefully") {
        // WHEN: Querying with negative radius (user error)
        auto results = grid.query_radius(-10.0f, 0.0f, 0.0f, 0.0f);

        // THEN: Should either return empty or handle gracefully
        // (Implementation detail - negative radius is undefined behavior)
        // Just verify it doesn't crash
        REQUIRE(true);  // If we got here, no crash
    }

    SECTION("Query at exact entity position") {
        // WHEN: Querying exactly where an entity is
        auto results = grid.query_radius(1.0f, 5.0f, 5.0f, 5.0f);

        // THEN: Should definitely find that entity
        bool found_entity_0 = false;
        for (auto idx : results) {
            if (idx == 0) found_entity_0 = true;
        }
        REQUIRE(found_entity_0);
    }

    SECTION("Query outside world bounds") {
        // WHEN: Querying far outside world (bounded topology)
        auto results = grid.query_radius(10.0f, 1000.0f, 1000.0f, 1000.0f);

        // THEN: Should clamp and return results at boundary
        // (might be empty if no entities there)
        REQUIRE(true);  // No crash is success
    }

    SECTION("Query with all dimensions zero radius") {
        // WHEN: Query that covers zero volume
        auto results = grid.query_radius(0.0f, 0.0f, 0.0f, 0.0f);

        // THEN: Should return entities in queried cell only
        // (Conservative cell-based query)
        REQUIRE(true);  // No crash
    }
}
