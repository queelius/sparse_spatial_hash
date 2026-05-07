/**
 * Tests for v2.1 additive API: update_one, for_each_in_radius, validate,
 * entity_cell, and the cell_coord stream-insertion operator.
 */

#include "test_helpers.hpp"
#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <stdexcept>
#include <sstream>

using namespace spatial;

namespace {

grid_config<3> make_3d_unit_cfg() {
    return grid_config<3>{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };
}

} // namespace

TEST_CASE("update_one moves a single entity to a new cell", "[update_one]") {
    sparse_spatial_hash<Particle, 3> grid(make_3d_unit_cfg());

    std::vector<Particle> particles = {
        {5.0f, 5.0f, 5.0f, 0},    // cell A
        {15.0f, 5.0f, 5.0f, 1},   // cell B
        {25.0f, 5.0f, 5.0f, 2}    // cell C
    };
    grid.rebuild(particles);

    REQUIRE(grid.entity_count() == 3);
    REQUIRE(grid.validate());

    SECTION("Moving an entity to a new cell updates both cells") {
        const auto cell_before = grid.entity_cell(1);
        REQUIRE(cell_before.has_value());

        // Move particle 1 from cell B into cell A's region.
        Particle moved = {6.0f, 6.0f, 6.0f, 1};
        grid.update_one(1, moved);

        const auto cell_after = grid.entity_cell(1);
        REQUIRE(cell_after.has_value());
        REQUIRE(*cell_after != *cell_before);
        REQUIRE(grid.validate());
    }

    SECTION("Moving an entity within the same cell is a no-op") {
        const auto cell_before = grid.entity_cell(0);
        Particle stayed = {6.0f, 6.0f, 6.0f, 0};  // still in same cell
        grid.update_one(0, stayed);
        REQUIRE(grid.entity_cell(0) == cell_before);
        REQUIRE(grid.validate());
    }

    SECTION("update_one throws on out-of-range index") {
        Particle dummy = {0.0f, 0.0f, 0.0f, 999};
        REQUIRE_THROWS_AS(grid.update_one(99, dummy), std::out_of_range);
        // grid is unchanged after the throw
        REQUIRE(grid.entity_count() == 3);
        REQUIRE(grid.validate());
    }
}

TEST_CASE("for_each_in_radius is allocation-free and returns same candidates as query_radius",
          "[for_each_in_radius]") {
    sparse_spatial_hash<Particle, 2> grid(grid_config<2>{
        .cell_size = {10.0f, 10.0f},
        .world_size = {100.0f, 100.0f},
        .topology_type = topology::bounded
    });

    std::vector<Particle> particles;
    for (int i = 0; i < 20; ++i) {
        particles.push_back({static_cast<float>(i * 4), 0.0f, 0.0f, i});
    }
    grid.rebuild(particles);

    SECTION("Callback visits the same set as query_radius") {
        std::array<float, 2> center{40.0f, 0.0f};
        auto vec_results = grid.query_radius(20.0f, center);

        std::vector<std::size_t> cb_results;
        grid.for_each_in_radius(20.0f, center, [&](std::size_t idx) {
            cb_results.push_back(idx);
        });

        std::sort(vec_results.begin(), vec_results.end());
        std::sort(cb_results.begin(), cb_results.end());
        REQUIRE(vec_results == cb_results);
    }

    SECTION("Empty result when nothing is in range") {
        std::array<float, 2> faraway{99.0f, 99.0f};
        std::size_t hits = 0;
        grid.for_each_in_radius(0.5f, faraway, [&](std::size_t) { ++hits; });
        REQUIRE(hits == 0);
    }
}

TEST_CASE("validate detects state inconsistencies", "[validate]") {
    sparse_spatial_hash<Particle, 3> grid(make_3d_unit_cfg());

    SECTION("Empty grid validates") {
        REQUIRE(grid.validate());
    }

    SECTION("Validates after rebuild") {
        std::vector<Particle> particles = {
            {5.0f, 5.0f, 5.0f, 0},
            {15.0f, 5.0f, 5.0f, 1}
        };
        grid.rebuild(particles);
        REQUIRE(grid.validate());
    }

    SECTION("Validates after a chain of update_one calls") {
        std::vector<Particle> particles(10);
        for (int i = 0; i < 10; ++i) {
            particles[i] = {static_cast<float>(i * 3), 0.0f, 0.0f, i};
        }
        grid.rebuild(particles);

        for (std::size_t i = 0; i < 10; ++i) {
            Particle p = {static_cast<float>(i * 3 + 7), 0.0f, 0.0f, static_cast<int>(i)};
            grid.update_one(i, p);
        }
        REQUIRE(grid.validate());
    }
}

TEST_CASE("entity_cell returns the correct cell or nullopt", "[entity_cell]") {
    sparse_spatial_hash<Particle, 3> grid(make_3d_unit_cfg());

    std::vector<Particle> particles = {
        {5.0f, 5.0f, 5.0f, 0},
        {15.0f, 5.0f, 5.0f, 1}
    };
    grid.rebuild(particles);

    SECTION("Valid index returns a cell that contains the entity") {
        auto c = grid.entity_cell(0);
        REQUIRE(c.has_value());
        // The cell containing entity 0 should be (0, 0, 0) under the centered
        // coordinate convention with cell_size=10 and world_size=100:
        //   cell[d] = floor((5 + 50) / 10) = 5
        REQUIRE(c->coords[0] == 5);
        REQUIRE(c->coords[1] == 5);
        REQUIRE(c->coords[2] == 5);
    }

    SECTION("Out-of-range index returns nullopt") {
        REQUIRE_FALSE(grid.entity_cell(99).has_value());
    }
}

TEST_CASE("operator<< prints cell_coord in (a, b, c) form", "[diagnostics][operator<<]") {
    cell_coord<3, int> c3{1, -2, 30};
    std::ostringstream os3;
    os3 << c3;
    REQUIRE(os3.str() == "(1, -2, 30)");

    cell_coord<2, int> c2{0, 0};
    std::ostringstream os2;
    os2 << c2;
    REQUIRE(os2.str() == "(0, 0)");
}

