/**
 * Performance benchmarks for sparse_spatial_hash using Google Benchmark
 *
 * Measures key operations and compares against naive approaches.
 */

#include <spatial/sparse_spatial_hash.hpp>
#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include <unordered_map>

using namespace spatial;

// ============================================================================
// Test Particle Type
// ============================================================================

struct Particle {
    float x, y, z;
    float vx, vy, vz;
    int id;
};

template<>
struct spatial::position_accessor<Particle, 3> {
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
struct spatial::position_accessor<Particle, 2> {
    static float get(const Particle& p, std::size_t dim) {
        return dim == 0 ? p.x : p.y;
    }
};

// ============================================================================
// Utility Functions
// ============================================================================

std::vector<Particle> create_particles_3d(std::size_t count, float world_size) {
    std::vector<Particle> particles(count);
    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::uniform_real_distribution<float> pos_dist(0.0f, world_size);
    std::uniform_real_distribution<float> vel_dist(-10.0f, 10.0f);

    for (std::size_t i = 0; i < count; ++i) {
        particles[i] = Particle{
            pos_dist(rng), pos_dist(rng), pos_dist(rng),
            vel_dist(rng), vel_dist(rng), vel_dist(rng),
            static_cast<int>(i)
        };
    }
    return particles;
}

std::vector<Particle> create_particles_2d(std::size_t count, float world_size) {
    std::vector<Particle> particles(count);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> pos_dist(0.0f, world_size);
    std::uniform_real_distribution<float> vel_dist(-10.0f, 10.0f);

    for (std::size_t i = 0; i < count; ++i) {
        particles[i] = Particle{
            pos_dist(rng), pos_dist(rng), 0.0f,
            vel_dist(rng), vel_dist(rng), 0.0f,
            static_cast<int>(i)
        };
    }
    return particles;
}

// ============================================================================
// Benchmark: Grid Build (3D)
// ============================================================================

static void BM_Build_3D_1K(benchmark::State& state) {
    auto particles = create_particles_3d(1000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    for (auto _ : state) {
        sparse_spatial_hash<Particle, 3> grid(cfg);
        grid.rebuild(particles);
        benchmark::DoNotOptimize(grid);
    }
    state.SetItemsProcessed(state.iterations() * particles.size());
}
BENCHMARK(BM_Build_3D_1K);

static void BM_Build_3D_10K(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    for (auto _ : state) {
        sparse_spatial_hash<Particle, 3> grid(cfg);
        grid.rebuild(particles);
        benchmark::DoNotOptimize(grid);
    }
    state.SetItemsProcessed(state.iterations() * particles.size());
}
BENCHMARK(BM_Build_3D_10K);

static void BM_Build_3D_100K(benchmark::State& state) {
    auto particles = create_particles_3d(100000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    for (auto _ : state) {
        sparse_spatial_hash<Particle, 3> grid(cfg);
        grid.rebuild(particles);
        benchmark::DoNotOptimize(grid);
    }
    state.SetItemsProcessed(state.iterations() * particles.size());
}
BENCHMARK(BM_Build_3D_100K);

// ============================================================================
// Benchmark: Grid Build (2D)
// ============================================================================

static void BM_Build_2D_10K(benchmark::State& state) {
    auto particles = create_particles_2d(10000, 1000.0f);
    grid_config<2> cfg{
        .cell_size = {10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    for (auto _ : state) {
        sparse_spatial_hash<Particle, 2> grid(cfg);
        grid.rebuild(particles);
        benchmark::DoNotOptimize(grid);
    }
    state.SetItemsProcessed(state.iterations() * particles.size());
}
BENCHMARK(BM_Build_2D_10K);

// ============================================================================
// Benchmark: Incremental Update vs Full Rebuild
// ============================================================================

static void BM_Update_Incremental(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::toroidal
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);
    grid.rebuild(particles);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> delta(-1.0f, 1.0f);

    for (auto _ : state) {
        // Small movement (typically ~1% change cells)
        for (auto& p : particles) {
            p.x += delta(rng);
            p.y += delta(rng);
            p.z += delta(rng);
        }

        grid.update(particles);
        benchmark::DoNotOptimize(grid);
    }
    state.SetItemsProcessed(state.iterations() * particles.size());
}
BENCHMARK(BM_Update_Incremental);

static void BM_Update_FullRebuild(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::toroidal
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);
    grid.rebuild(particles);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> delta(-1.0f, 1.0f);

    for (auto _ : state) {
        // Small movement
        for (auto& p : particles) {
            p.x += delta(rng);
            p.y += delta(rng);
            p.z += delta(rng);
        }

        grid.rebuild(particles);
        benchmark::DoNotOptimize(grid);
    }
    state.SetItemsProcessed(state.iterations() * particles.size());
}
BENCHMARK(BM_Update_FullRebuild);

// ============================================================================
// Benchmark: Radius Queries
// ============================================================================

static void BM_Query_Radius_Small(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {20.0f, 20.0f, 20.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);
    grid.rebuild(particles);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> pos_dist(0.0f, 1000.0f);

    for (auto _ : state) {
        float x = pos_dist(rng);
        float y = pos_dist(rng);
        float z = pos_dist(rng);
        auto results = grid.query_radius(20.0f, x, y, z);
        benchmark::DoNotOptimize(results);
    }
}
BENCHMARK(BM_Query_Radius_Small);

static void BM_Query_Radius_Medium(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {20.0f, 20.0f, 20.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);
    grid.rebuild(particles);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> pos_dist(0.0f, 1000.0f);

    for (auto _ : state) {
        float x = pos_dist(rng);
        float y = pos_dist(rng);
        float z = pos_dist(rng);
        auto results = grid.query_radius(50.0f, x, y, z);
        benchmark::DoNotOptimize(results);
    }
}
BENCHMARK(BM_Query_Radius_Medium);

static void BM_Query_Radius_Large(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {20.0f, 20.0f, 20.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);
    grid.rebuild(particles);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> pos_dist(0.0f, 1000.0f);

    for (auto _ : state) {
        float x = pos_dist(rng);
        float y = pos_dist(rng);
        float z = pos_dist(rng);
        auto results = grid.query_radius(100.0f, x, y, z);
        benchmark::DoNotOptimize(results);
    }
}
BENCHMARK(BM_Query_Radius_Large);

// ============================================================================
// Benchmark: Pair Processing
// ============================================================================

static void BM_ForEachPair_SparseHash(benchmark::State& state) {
    auto particles = create_particles_3d(5000, 1000.0f);  // Smaller for pair processing
    grid_config<3> cfg{
        .cell_size = {20.0f, 20.0f, 20.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Particle, 3> grid(cfg);
    grid.rebuild(particles);

    for (auto _ : state) {
        std::size_t pair_count = 0;
        grid.for_each_pair(particles, 30.0f,
            [&](std::size_t i, std::size_t j) {
                pair_count++;
                // Simulate light work
                benchmark::DoNotOptimize(particles[i].x + particles[j].x);
            });
        benchmark::DoNotOptimize(pair_count);
    }
}
BENCHMARK(BM_ForEachPair_SparseHash);

// ============================================================================
// Benchmark: Topology Comparison
// ============================================================================

static void BM_Topology_Bounded(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    for (auto _ : state) {
        sparse_spatial_hash<Particle, 3> grid(cfg);
        grid.rebuild(particles);
        benchmark::DoNotOptimize(grid);
    }
    state.SetItemsProcessed(state.iterations() * particles.size());
}
BENCHMARK(BM_Topology_Bounded);

static void BM_Topology_Toroidal(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::toroidal
    };

    for (auto _ : state) {
        sparse_spatial_hash<Particle, 3> grid(cfg);
        grid.rebuild(particles);
        benchmark::DoNotOptimize(grid);
    }
    state.SetItemsProcessed(state.iterations() * particles.size());
}
BENCHMARK(BM_Topology_Toroidal);

static void BM_Topology_Infinite(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::infinite
    };

    for (auto _ : state) {
        sparse_spatial_hash<Particle, 3> grid(cfg);
        grid.rebuild(particles);
        benchmark::DoNotOptimize(grid);
    }
    state.SetItemsProcessed(state.iterations() * particles.size());
}
BENCHMARK(BM_Topology_Infinite);

// ============================================================================
// Baseline Comparison: Naive O(n²) Pair Processing
// ============================================================================

static void BM_ForEachPair_Naive(benchmark::State& state) {
    auto particles = create_particles_3d(1000, 1000.0f);  // Much smaller for O(n²)
    constexpr float radius_sq = 30.0f * 30.0f;

    for (auto _ : state) {
        std::size_t pair_count = 0;
        // Naive all-pairs comparison
        for (std::size_t i = 0; i < particles.size(); ++i) {
            for (std::size_t j = i + 1; j < particles.size(); ++j) {
                float dx = particles[i].x - particles[j].x;
                float dy = particles[i].y - particles[j].y;
                float dz = particles[i].z - particles[j].z;
                float dist_sq = dx*dx + dy*dy + dz*dz;

                if (dist_sq <= radius_sq) {
                    pair_count++;
                    benchmark::DoNotOptimize(particles[i].x + particles[j].x);
                }
            }
        }
        benchmark::DoNotOptimize(pair_count);
    }
}
BENCHMARK(BM_ForEachPair_Naive);

// ============================================================================
// Baseline Comparison: std::unordered_map (simpler hash approach)
// ============================================================================

static void BM_Build_UnorderedMap_Baseline(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    constexpr float cell_size = 10.0f;

    for (auto _ : state) {
        // Simple hash map approach (no spatial optimization)
        std::unordered_map<std::size_t, std::vector<std::size_t>> cells;

        for (std::size_t i = 0; i < particles.size(); ++i) {
            int cx = static_cast<int>(particles[i].x / cell_size);
            int cy = static_cast<int>(particles[i].y / cell_size);
            int cz = static_cast<int>(particles[i].z / cell_size);

            // Simple hash (not Morton code)
            std::size_t hash = cx + cy * 1000 + cz * 1000000;
            cells[hash].push_back(i);
        }

        benchmark::DoNotOptimize(cells);
    }
    state.SetItemsProcessed(state.iterations() * particles.size());
}
BENCHMARK(BM_Build_UnorderedMap_Baseline);

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
