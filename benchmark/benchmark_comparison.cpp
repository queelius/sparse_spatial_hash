/**
 * Performance comparison: Original vs Optimized Implementation
 */

#include <spatial/sparse_spatial_hash.hpp>
#include <boost/spatial/sparse_spatial_hash_optimized.hpp>
#include <benchmark/benchmark.h>
#include <vector>
#include <random>

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
    std::mt19937 rng(42);
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
// Build Performance Comparison
// ============================================================================

static void BM_Build_3D_10K_Original(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    for (auto _ : state) {
        spatial::sparse_spatial_hash<Particle, 3> grid(cfg);
        grid.rebuild(particles);
        benchmark::DoNotOptimize(grid);
    }
    state.SetItemsProcessed(state.iterations() * particles.size());
}
BENCHMARK(BM_Build_3D_10K_Original);

static void BM_Build_3D_10K_Optimized(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    for (auto _ : state) {
        spatial::sparse_spatial_hash<Particle, 3> grid(cfg);
        grid.rebuild(particles);
        benchmark::DoNotOptimize(grid);
    }
    state.SetItemsProcessed(state.iterations() * particles.size());
}
BENCHMARK(BM_Build_3D_10K_Optimized);

// ============================================================================
// Update Performance Comparison
// ============================================================================

static void BM_Update_Incremental_Original(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    spatial::sparse_spatial_hash<Particle, 3> grid(cfg);
    grid.rebuild(particles);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> delta(-1.0f, 1.0f);

    for (auto _ : state) {
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
BENCHMARK(BM_Update_Incremental_Original);

static void BM_Update_Incremental_Optimized(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    spatial::sparse_spatial_hash<Particle, 3> grid(cfg);
    grid.rebuild(particles);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> delta(-1.0f, 1.0f);

    for (auto _ : state) {
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
BENCHMARK(BM_Update_Incremental_Optimized);

// ============================================================================
// Query Performance Comparison
// ============================================================================

static void BM_Query_Radius_Original(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {20.0f, 20.0f, 20.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    spatial::sparse_spatial_hash<Particle, 3> grid(cfg);
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
BENCHMARK(BM_Query_Radius_Original);

static void BM_Query_Radius_Optimized(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {20.0f, 20.0f, 20.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    spatial::sparse_spatial_hash<Particle, 3> grid(cfg);
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
BENCHMARK(BM_Query_Radius_Optimized);

// ============================================================================
// Topology Performance Comparison
// ============================================================================

static void BM_Topology_Toroidal_Original(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::toroidal
    };

    for (auto _ : state) {
        spatial::sparse_spatial_hash<Particle, 3> grid(cfg);
        grid.rebuild(particles);
        benchmark::DoNotOptimize(grid);
    }
    state.SetItemsProcessed(state.iterations() * particles.size());
}
BENCHMARK(BM_Topology_Toroidal_Original);

static void BM_Topology_Toroidal_Optimized(benchmark::State& state) {
    auto particles = create_particles_3d(10000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::toroidal
    };

    for (auto _ : state) {
        spatial::sparse_spatial_hash<Particle, 3> grid(cfg);
        grid.rebuild(particles);
        benchmark::DoNotOptimize(grid);
    }
    state.SetItemsProcessed(state.iterations() * particles.size());
}
BENCHMARK(BM_Topology_Toroidal_Optimized);

// ============================================================================
// ForEachPair Performance Comparison
// ============================================================================

static void BM_ForEachPair_Original(benchmark::State& state) {
    auto particles = create_particles_3d(5000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {20.0f, 20.0f, 20.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    spatial::sparse_spatial_hash<Particle, 3> grid(cfg);
    grid.rebuild(particles);

    for (auto _ : state) {
        std::size_t pair_count = 0;
        grid.for_each_pair(particles, 30.0f,
            [&](std::size_t i, std::size_t j) {
                pair_count++;
                benchmark::DoNotOptimize(particles[i].x + particles[j].x);
            });
        benchmark::DoNotOptimize(pair_count);
    }
}
BENCHMARK(BM_ForEachPair_Original);

static void BM_ForEachPair_Optimized(benchmark::State& state) {
    auto particles = create_particles_3d(5000, 1000.0f);
    grid_config<3> cfg{
        .cell_size = {20.0f, 20.0f, 20.0f},
        .world_size = {1000.0f, 1000.0f, 1000.0f},
        .topology_type = topology::bounded
    };

    spatial::sparse_spatial_hash<Particle, 3> grid(cfg);
    grid.rebuild(particles);

    for (auto _ : state) {
        std::size_t pair_count = 0;
        grid.for_each_pair(particles, 30.0f,
            [&](std::size_t i, std::size_t j) {
                pair_count++;
                benchmark::DoNotOptimize(particles[i].x + particles[j].x);
            });
        benchmark::DoNotOptimize(pair_count);
    }
}
BENCHMARK(BM_ForEachPair_Optimized);

// ============================================================================
// Main
// ============================================================================

BENCHMARK_MAIN();
