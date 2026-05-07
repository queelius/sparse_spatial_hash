/**
 * Example: 3D Molecular Dynamics Simulation
 *
 * Demonstrates sparse_spatial_hash for neighbor list generation
 * in a molecular dynamics simulation (Lennard-Jones particles).
 *
 * Compile:
 *   g++ -std=c++20 -O2 -I../include molecular_dynamics_3d.cpp -o md_3d
 *
 * Run:
 *   ./md_3d
 */

#include <spatial/sparse_spatial_hash.hpp>
#include <vector>
#include <iostream>
#include <random>
#include <cmath>
#include <array>

// 3D Molecule (atom)
struct Molecule {
    std::array<double, 3> position;
    std::array<double, 3> velocity;
    std::array<double, 3> force;
    double mass;
    int type;  // Atom type (for different LJ parameters)

    void integrate(double dt) {
        // Velocity Verlet integration
        for (int d = 0; d < 3; ++d) {
            velocity[d] += 0.5 * force[d] / mass * dt;
            position[d] += velocity[d] * dt;
        }
    }

    void update_velocity(double dt) {
        for (int d = 0; d < 3; ++d) {
            velocity[d] += 0.5 * force[d] / mass * dt;
        }
    }
};

// Specialize position accessor
template<>
struct spatial::position_accessor<Molecule, 3> {
    static double get(const Molecule& m, std::size_t dim) {
        return m.position[dim];
    }
};

// Lennard-Jones potential parameters
struct LJParams {
    double epsilon = 1.0;  // Energy scale
    double sigma = 1.0;    // Length scale
    double cutoff = 2.5;   // Cutoff distance (in sigma units)
};

// Calculate Lennard-Jones force
std::array<double, 3> lj_force(
    const Molecule& a,
    const Molecule& b,
    const LJParams& params,
    double world_size
) {
    std::array<double, 3> r;
    double r2 = 0.0;

    // Calculate distance with periodic boundaries
    for (int d = 0; d < 3; ++d) {
        r[d] = a.position[d] - b.position[d];

        // Minimum image convention
        if (r[d] > world_size / 2) r[d] -= world_size;
        if (r[d] < -world_size / 2) r[d] += world_size;

        r2 += r[d] * r[d];
    }

    // Check cutoff
    if (r2 > params.cutoff * params.cutoff * params.sigma * params.sigma) {
        return {0.0, 0.0, 0.0};
    }

    double dist = std::sqrt(r2);
    double sigma_over_r = params.sigma / dist;
    double s6 = std::pow(sigma_over_r, 6);
    double s12 = s6 * s6;

    // LJ force magnitude: F = 24*epsilon/r * (2*s12 - s6)
    double f_mag = 24.0 * params.epsilon / dist * (2.0 * s12 - s6);

    return {
        f_mag * r[0] / dist,
        f_mag * r[1] / dist,
        f_mag * r[2] / dist
    };
}

int main() {
    using namespace spatial;

    std::cout << "=== 3D Molecular Dynamics Demo ===\n\n";

    // Configuration
    constexpr std::size_t num_molecules = 10000;
    constexpr double world_size = 100.0;
    constexpr double density = num_molecules / (world_size * world_size * world_size);
    constexpr double dt = 0.001;
    constexpr int num_steps = 100;

    LJParams lj_params{
        .epsilon = 1.0,
        .sigma = 1.0,
        .cutoff = 2.5
    };

    // Cell size should be >= cutoff for efficiency
    double cell_size = lj_params.cutoff * lj_params.sigma;

    std::cout << "Configuration:\n";
    std::cout << "  Molecules: " << num_molecules << "\n";
    std::cout << "  World size: " << world_size << "³\n";
    std::cout << "  Density: " << density << "\n";
    std::cout << "  Cell size: " << cell_size << "\n";
    std::cout << "  LJ cutoff: " << lj_params.cutoff << " σ\n\n";

    // Create spatial grid
    grid_config<3, double> cfg{
        .cell_size = {cell_size, cell_size, cell_size},
        .world_size = {world_size, world_size, world_size},
        .topology_type = topology::toroidal
    };

    sparse_spatial_hash<Molecule, 3, double> grid(cfg);

    // Initialize molecules on FCC lattice
    std::vector<Molecule> molecules;
    molecules.reserve(num_molecules);

    std::mt19937 rng(42);
    std::normal_distribution<double> vel_dist(0.0, 1.0);

    // Simple cubic lattice for initialization
    int n_per_side = std::ceil(std::cbrt(num_molecules));
    double spacing = world_size / n_per_side;

    for (int i = 0; i < n_per_side && molecules.size() < num_molecules; ++i) {
        for (int j = 0; j < n_per_side && molecules.size() < num_molecules; ++j) {
            for (int k = 0; k < n_per_side && molecules.size() < num_molecules; ++k) {
                Molecule mol;
                mol.position = {
                    i * spacing + spacing / 2,
                    j * spacing + spacing / 2,
                    k * spacing + spacing / 2
                };
                mol.velocity = {
                    vel_dist(rng),
                    vel_dist(rng),
                    vel_dist(rng)
                };
                mol.force = {0.0, 0.0, 0.0};
                mol.mass = 1.0;
                mol.type = 0;

                molecules.push_back(mol);
            }
        }
    }

    std::cout << "Initialized " << molecules.size() << " molecules\n";

    // Build initial grid
    grid.rebuild(molecules);

    auto stats = grid.stats();
    std::cout << "Grid statistics:\n";
    std::cout << "  Occupied cells: " << stats.occupied_cells << "\n";
    std::cout << "  Avg molecules/cell: " << stats.avg_entities_per_cell << "\n";
    std::cout << "  Max molecules/cell: " << stats.max_entities_per_cell << "\n\n";

    std::cout << "Running MD simulation (" << num_steps << " steps)...\n";

    double total_energy = 0.0;

    // MD loop
    for (int step = 0; step < num_steps; ++step) {
        // 1. Clear forces
        for (auto& mol : molecules) {
            mol.force = {0.0, 0.0, 0.0};
        }

        // 2. Calculate forces using spatial grid
        std::size_t pair_count = 0;

        grid.for_each_pair(lj_params.cutoff * lj_params.sigma,
            [&](std::size_t i, std::size_t j) {
                auto force = lj_force(molecules[i], molecules[j], lj_params, world_size);

                // Newton's third law
                molecules[i].force[0] += force[0];
                molecules[i].force[1] += force[1];
                molecules[i].force[2] += force[2];

                molecules[j].force[0] -= force[0];
                molecules[j].force[1] -= force[1];
                molecules[j].force[2] -= force[2];

                pair_count++;
            });

        // 3. Integrate equations of motion
        for (auto& mol : molecules) {
            mol.integrate(dt);
        }

        // Apply periodic boundaries
        for (auto& mol : molecules) {
            for (int d = 0; d < 3; ++d) {
                if (mol.position[d] < 0) mol.position[d] += world_size;
                if (mol.position[d] >= world_size) mol.position[d] -= world_size;
            }
        }

        // 4. Update velocities
        for (auto& mol : molecules) {
            mol.update_velocity(dt);
        }

        // 5. Update spatial grid (incremental)
        grid.update(molecules);

        // Calculate kinetic energy
        if (step % 10 == 0) {
            double ke = 0.0;
            for (const auto& mol : molecules) {
                double v2 = 0.0;
                for (int d = 0; d < 3; ++d) {
                    v2 += mol.velocity[d] * mol.velocity[d];
                }
                ke += 0.5 * mol.mass * v2;
            }

            std::cout << "  Step " << step << ": KE = " << ke
                      << ", pairs = " << pair_count << "\n";
        }
    }

    std::cout << "\nSimulation complete!\n";

    // Final statistics
    stats = grid.stats();
    std::cout << "\nFinal grid statistics:\n";
    std::cout << "  Occupied cells: " << stats.occupied_cells << "\n";
    std::cout << "  Avg molecules/cell: " << stats.avg_entities_per_cell << "\n";

    return 0;
}
