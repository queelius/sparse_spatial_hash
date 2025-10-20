/**
 * Example: 2D Particle Collision Detection
 *
 * Demonstrates using sparse_spatial_hash for efficient broad-phase
 * collision detection in a 2D particle system.
 *
 * Compile:
 *   g++ -std=c++20 -O2 -I../include collision_detection_2d.cpp -o collision_2d
 *
 * Run:
 *   ./collision_2d
 */

#include <boost/spatial/sparse_spatial_hash.hpp>
#include <vector>
#include <iostream>
#include <random>
#include <cmath>
#include <chrono>

// Simple 2D particle
struct Particle {
    float x, y;           // Position
    float vx, vy;         // Velocity
    float radius;         // Collision radius
    int id;               // Unique identifier

    void update(float dt, float world_size) {
        // Update position
        x += vx * dt;
        y += vy * dt;

        // Wrap around (toroidal world)
        if (x < 0) x += world_size;
        if (x >= world_size) x -= world_size;
        if (y < 0) y += world_size;
        if (y >= world_size) y -= world_size;
    }
};

// Specialize position accessor for Particle
template<>
struct boost::spatial::position_accessor<Particle, 2> {
    static float get(const Particle& p, std::size_t dim) {
        return dim == 0 ? p.x : p.y;
    }
};

// Calculate distance between two particles (with toroidal wrap)
float distance(const Particle& a, const Particle& b, float world_size) {
    float dx = std::abs(a.x - b.x);
    float dy = std::abs(a.y - b.y);

    // Handle wraparound
    if (dx > world_size / 2) dx = world_size - dx;
    if (dy > world_size / 2) dy = world_size - dy;

    return std::sqrt(dx * dx + dy * dy);
}

// Handle collision between two particles
void handle_collision(Particle& a, Particle& b, float world_size) {
    float dist = distance(a, b, world_size);
    float min_dist = a.radius + b.radius;

    if (dist < min_dist && dist > 0) {
        // Simple elastic collision response
        float dx = a.x - b.x;
        float dy = a.y - b.y;

        // Handle wraparound
        if (std::abs(dx) > world_size / 2) {
            dx = dx > 0 ? dx - world_size : dx + world_size;
        }
        if (std::abs(dy) > world_size / 2) {
            dy = dy > 0 ? dy - world_size : dy + world_size;
        }

        // Normalize
        float nx = dx / dist;
        float ny = dy / dist;

        // Relative velocity
        float dvx = a.vx - b.vx;
        float dvy = a.vy - b.vy;

        // Relative velocity in collision normal direction
        float dvn = dvx * nx + dvy * ny;

        // Don't resolve if velocities are separating
        if (dvn < 0) {
            // Apply impulse (assuming equal mass)
            a.vx -= dvn * nx;
            a.vy -= dvn * ny;
            b.vx += dvn * nx;
            b.vy += dvn * ny;
        }
    }
}

int main() {
    using namespace boost::spatial;

    std::cout << "=== 2D Particle Collision Detection Demo ===\n\n";

    // Configuration
    constexpr std::size_t num_particles = 10000;
    constexpr float world_size = 1000.0f;
    constexpr float cell_size = 10.0f;  // Optimize for ~10-unit interactions
    constexpr float collision_radius = 15.0f;
    constexpr float dt = 0.016f;  // 60 FPS
    constexpr int num_frames = 100;

    // Create spatial grid
    grid_config<2> cfg{
        .cell_size = {cell_size, cell_size},
        .world_size = {world_size, world_size},
        .topology_type = topology::toroidal
    };

    sparse_spatial_hash<Particle, 2> grid(cfg);

    // Initialize particles
    std::vector<Particle> particles(num_particles);
    std::mt19937 rng(42);
    std::uniform_real_distribution<float> pos_dist(0.0f, world_size);
    std::uniform_real_distribution<float> vel_dist(-50.0f, 50.0f);
    std::uniform_real_distribution<float> radius_dist(1.0f, 5.0f);

    for (std::size_t i = 0; i < num_particles; ++i) {
        particles[i] = Particle{
            .x = pos_dist(rng),
            .y = pos_dist(rng),
            .vx = vel_dist(rng),
            .vy = vel_dist(rng),
            .radius = radius_dist(rng),
            .id = static_cast<int>(i)
        };
    }

    std::cout << "Configuration:\n";
    std::cout << "  Particles: " << num_particles << "\n";
    std::cout << "  World size: " << world_size << "x" << world_size << "\n";
    std::cout << "  Cell size: " << cell_size << "\n";
    std::cout << "  Topology: Toroidal (periodic boundaries)\n\n";

    // Initial build
    auto start = std::chrono::high_resolution_clock::now();
    grid.rebuild(particles);
    auto end = std::chrono::high_resolution_clock::now();
    auto build_time = std::chrono::duration<double, std::milli>(end - start).count();

    auto stats = grid.stats();
    std::cout << "Initial grid statistics:\n";
    std::cout << "  Occupied cells: " << stats.occupied_cells << "\n";
    std::cout << "  Avg particles/cell: " << stats.avg_entities_per_cell << "\n";
    std::cout << "  Max particles/cell: " << stats.max_entities_per_cell << "\n";
    std::cout << "  Occupancy ratio: " << (stats.occupancy_ratio * 100) << "%\n";
    std::cout << "  Build time: " << build_time << " ms\n\n";

    // Simulation loop
    std::cout << "Running simulation (" << num_frames << " frames)...\n";

    double total_update_time = 0.0;
    double total_collision_time = 0.0;
    std::size_t total_collisions = 0;
    std::size_t total_checks = 0;

    for (int frame = 0; frame < num_frames; ++frame) {
        // Update particle positions
        for (auto& p : particles) {
            p.update(dt, world_size);
        }

        // Update spatial grid (incremental)
        start = std::chrono::high_resolution_clock::now();
        grid.update(particles);
        end = std::chrono::high_resolution_clock::now();
        total_update_time += std::chrono::duration<double, std::milli>(end - start).count();

        // Collision detection
        start = std::chrono::high_resolution_clock::now();
        std::size_t frame_collisions = 0;
        std::size_t frame_checks = 0;

        grid.for_each_pair(particles, collision_radius,
            [&](std::size_t i, std::size_t j) {
                frame_checks++;
                float dist = distance(particles[i], particles[j], world_size);
                float min_dist = particles[i].radius + particles[j].radius;

                if (dist < min_dist) {
                    handle_collision(particles[i], particles[j], world_size);
                    frame_collisions++;
                }
            });

        end = std::chrono::high_resolution_clock::now();
        total_collision_time += std::chrono::duration<double, std::milli>(end - start).count();
        total_collisions += frame_collisions;
        total_checks += frame_checks;

        // Print progress every 20 frames
        if ((frame + 1) % 20 == 0) {
            std::cout << "  Frame " << (frame + 1) << ": "
                      << frame_checks << " checks, "
                      << frame_collisions << " collisions\n";
        }
    }

    // Results
    std::cout << "\n=== Performance Results ===\n";
    std::cout << "Avg grid update time: " << (total_update_time / num_frames) << " ms/frame\n";
    std::cout << "Avg collision time: " << (total_collision_time / num_frames) << " ms/frame\n";
    std::cout << "Avg total time: " << ((total_update_time + total_collision_time) / num_frames) << " ms/frame\n";
    std::cout << "Target FPS (16.67ms): " << ((total_update_time + total_collision_time) / num_frames < 16.67 ? "ACHIEVED" : "MISSED") << "\n";
    std::cout << "\nTotal collision checks: " << total_collisions << "\n";
    std::cout << "Avg checks/frame: " << (total_checks / num_frames) << "\n";

    // Compare to naive O(n²) approach
    std::size_t naive_checks = num_particles * (num_particles - 1) / 2;
    std::cout << "\nNaive O(n²) checks: " << naive_checks << "\n";
    std::cout << "Sparse grid checks: " << (total_checks / num_frames) << "\n";
    std::cout << "Speedup: " << (static_cast<double>(naive_checks) / (total_checks / num_frames)) << "x\n";

    // Final grid stats
    stats = grid.stats();
    std::cout << "\nFinal grid statistics:\n";
    std::cout << "  Occupied cells: " << stats.occupied_cells << "\n";
    std::cout << "  Occupancy ratio: " << (stats.occupancy_ratio * 100) << "%\n";

    return 0;
}
