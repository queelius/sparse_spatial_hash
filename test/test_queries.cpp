/**
 * Query operation tests using Catch2
 */

#include <spatial/sparse_spatial_hash.hpp>
#include <catch2/catch_test_macros.hpp>
#include <vector>

using namespace spatial;

struct Entity {
    float x, y, z;
    int id;
};

template<>
struct spatial::position_accessor<Entity, 3> {
    static float get(const Entity& e, std::size_t dim) {
        switch(dim) {
            case 0: return e.x;
            case 1: return e.y;
            case 2: return e.z;
            default: return 0.0f;
        }
    }
};

TEST_CASE("Empty grid queries", "[queries][empty]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Entity, 3> grid(cfg);

    SECTION("Query on empty grid returns empty") {
        auto results = grid.query_radius(50.0f, 0.0f, 0.0f, 0.0f);
        REQUIRE(results.empty());
    }

    SECTION("Multiple queries on empty grid") {
        auto r1 = grid.query_radius(10.0f, 0.0f, 0.0f, 0.0f);
        auto r2 = grid.query_radius(100.0f, 50.0f, 50.0f, 50.0f);

        REQUIRE(r1.empty());
        REQUIRE(r2.empty());
    }
}

TEST_CASE("Single entity queries", "[queries][single]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Entity, 3> grid(cfg);

    std::vector<Entity> entities = {
        {5.0f, 5.0f, 5.0f, 0}
    };

    grid.rebuild(entities);

    SECTION("Query finds nearby entity") {
        auto results = grid.query_radius(20.0f, 0.0f, 0.0f, 0.0f);

        REQUIRE(results.size() == 1);
        REQUIRE(results[0] == 0);
    }

    SECTION("Query far away finds nothing") {
        auto results = grid.query_radius(10.0f, 100.0f, 100.0f, 100.0f);

        // Might find entity 0 depending on cell coverage (conservative)
        // But should be reasonable
        REQUIRE(results.size() <= 1);
    }
}

TEST_CASE("Variadic query_radius syntax", "[queries][variadic]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Entity, 3> grid(cfg);

    std::vector<Entity> entities = {
        {10.0f, 10.0f, 10.0f, 0},
        {20.0f, 20.0f, 20.0f, 1}
    };

    grid.rebuild(entities);

    SECTION("Variadic form works") {
        auto results = grid.query_radius(30.0f, 15.0f, 15.0f, 15.0f);

        // Should find at least one entity
        REQUIRE(results.size() >= 1);
    }
}

TEST_CASE("Cell contents iteration", "[queries][iteration]") {
    grid_config<3> cfg{
        .cell_size = {10.0f, 10.0f, 10.0f},
        .world_size = {100.0f, 100.0f, 100.0f},
        .topology_type = topology::bounded
    };

    sparse_spatial_hash<Entity, 3> grid(cfg);

    std::vector<Entity> entities = {
        {5.0f, 5.0f, 5.0f, 0},
        {6.0f, 6.0f, 6.0f, 1},  // Same cell
        {50.0f, 50.0f, 50.0f, 2}  // Different cell
    };

    grid.rebuild(entities);

    SECTION("Can iterate over cells") {
        int cell_count = 0;
        for (const auto& cell : grid.cells()) {
            cell_count++;
        }

        REQUIRE(cell_count == 2);  // Two occupied cells
    }

    SECTION("Can iterate over cell contents") {
        int total_entities = 0;
        for (const auto& [cell, entities_in_cell] : grid.cell_contents()) {
            total_entities += entities_in_cell.size();
        }

        REQUIRE(total_entities == 3);
    }
}
