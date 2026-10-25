#include <spatial/sparse_spatial_hash.hpp>
#include <catch2/catch_test_macros.hpp>
#include <array>
#include <limits>
#include <numeric>
#include <vector>

namespace {

struct point {
    std::array<double, 4> coordinates{};
    double operator[](std::size_t dim) const { return coordinates[dim]; }
};

template<std::size_t Dims>
void check_neighborhoods() {
    std::array<double, Dims> world;
    world.fill(2.);
    std::array<double, Dims> center;
    center.fill(-0.5);
    const std::size_t count = std::size_t{1} << Dims;
    std::vector<point> points(count);
    for (std::size_t i = 0; i < count; ++i) {
        for (std::size_t d = 0; d < Dims; ++d) {
            points[i].coordinates[d] = (i & (std::size_t{1} << d)) ? 0.5 : -0.5;
        }
    }

    for (auto topology : {spatial::topology::bounded, spatial::topology::toroidal,
                          spatial::topology::infinite}) {
        DYNAMIC_SECTION("topology " << static_cast<int>(topology)) {
            spatial::sparse_spatial_hash<point, Dims, double> grid(1., world, topology);
            grid.rebuild(points);

            SECTION("Each query candidate is returned exactly once") {
                std::vector<unsigned> visits(count);
                grid.for_each_in_radius(1., center, [&](std::size_t index) {
                    REQUIRE(index < count);
                    ++visits[index];
                });
                for (auto visits_to_entity : visits) CHECK(visits_to_entity == 1);

                auto actual = grid.query_radius(1., center);
                std::sort(actual.begin(), actual.end());
                std::vector<std::size_t> expected(count);
                std::iota(expected.begin(), expected.end(), std::size_t{0});
                CHECK(actual == expected);
            }

            SECTION("Every pair of adjacent cells is emitted exactly once") {
                std::vector<std::vector<unsigned>> visits(count,
                                                         std::vector<unsigned>(count));
                grid.for_each_pair(1., [&](std::size_t i, std::size_t j) {
                    REQUIRE(i < count);
                    REQUIRE(j < count);
                    REQUIRE(i != j);
                    ++visits[std::min(i, j)][std::max(i, j)];
                });
                for (std::size_t i = 0; i < count; ++i) {
                    for (std::size_t j = i + 1; j < count; ++j) {
                        CHECK(visits[i][j] == 1);
                    }
                }
            }
        }
    }
}

} // namespace

TEST_CASE("2D neighborhoods visit entities and pairs once", "[neighborhood][regression]") {
    check_neighborhoods<2>();
}

TEST_CASE("3D neighborhoods visit entities and pairs once", "[neighborhood][regression]") {
    check_neighborhoods<3>();
}

TEST_CASE("4D neighborhoods visit entities and pairs once", "[neighborhood][regression]") {
    check_neighborhoods<4>();
}

TEST_CASE("Toroidal neighborhoods can cover only one complete axis",
          "[neighborhood][toroidal][regression]") {
    spatial::sparse_spatial_hash<point, 2, double> grid(1., {2., 5.},
                                                      spatial::topology::toroidal);
    const std::vector<point> points{
        {{{-0.5, -2.}}}, {{{-0.5, -1.}}}, {{{-0.5, 0.}}},
        {{{-0.5, 1.}}}, {{{-0.5, 2.}}},
        {{{0.5, -2.}}}, {{{0.5, -1.}}}, {{{0.5, 0.}}},
        {{{0.5, 1.}}}, {{{0.5, 2.}}}
    };
    grid.rebuild(points);
    auto actual = grid.query_radius(1., -0.5, 0.);
    std::sort(actual.begin(), actual.end());
    REQUIRE(actual == std::vector<std::size_t>{1, 2, 3, 6, 7, 8});
}

TEST_CASE("Bounded queries clamp far outside the world without repeating entities",
          "[neighborhood][bounded][regression]") {
    spatial::sparse_spatial_hash<point, 2, double> grid(1., {2., 2.});
    grid.rebuild(std::vector<point>{{{{0.5, 0.5}}}});
    REQUIRE(grid.query_radius(1., 1000., 1000.) == std::vector<std::size_t>{0});
    REQUIRE(grid.query_radius(1., -1000., -1000.).empty());
}

TEST_CASE("Large toroidal radii visit only distinct cells", "[neighborhood][large_radius]") {
    spatial::sparse_spatial_hash<point, 2, double> grid(1., {1., 1.},
                                                      spatial::topology::toroidal);
    grid.rebuild(std::vector<point>{{{{-0.5, -0.5}}}, {{{-0.5, -0.5}}}});
    std::array<unsigned, 2> visits{};
    grid.for_each_in_radius(1.1e9, {-0.5, -0.5}, [&](std::size_t index) {
        REQUIRE(index < visits.size());
        ++visits[index];
    });
    REQUIRE(visits == std::array<unsigned, 2>{1, 1});
    unsigned pairs = 0;
    grid.for_each_pair(1.1e9, [&](std::size_t i, std::size_t j) {
        REQUIRE(i != j);
        ++pairs;
    });
    REQUIRE(pairs == 1);
}

TEST_CASE("Infinite neighborhoods stay within representable cell coordinates",
          "[neighborhood][infinite][regression]") {
    for (bool upper : {false, true}) {
        INFO("upper limit: " << upper);
        spatial::sparse_spatial_hash<point, 2, double> grid(1., {2., 2.},
                                                          spatial::topology::infinite);
        const double edge = upper ? std::numeric_limits<int>::max()
                                  : std::numeric_limits<int>::min();
        const double position = edge - 1.; // The centered grid adds one.
        const double adjacent = position + (upper ? -1. : 1.);
        grid.rebuild(std::vector<point>{{{{position, -0.5}}}, {{{adjacent, -0.5}}}});
        auto actual = grid.query_radius(1., position, -0.5);
        std::sort(actual.begin(), actual.end());
        REQUIRE(actual == std::vector<std::size_t>{0, 1});
    }
}
