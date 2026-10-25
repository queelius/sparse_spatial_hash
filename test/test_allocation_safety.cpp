/**
 * Deterministic allocation instrumentation, isolated in its own executable.
 * Assertions run after each probe so Catch2's own allocations are unaffected.
 */
#include "test_helpers.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace {

class allocation_probe {
public:
    explicit allocation_probe(
        std::size_t allowed = std::numeric_limits<std::size_t>::max()) noexcept
        : previous_(active_), allowed_(allowed) {
        active_ = this;
    }

    ~allocation_probe() { active_ = previous_; }
    allocation_probe(const allocation_probe&) = delete;
    allocation_probe& operator=(const allocation_probe&) = delete;

    std::size_t count() const noexcept { return count_; }

    static void record() {
        if (active_ && ++active_->count_ > active_->allowed_) {
            throw std::bad_alloc();
        }
    }

private:
    static thread_local allocation_probe* active_;
    allocation_probe* previous_;
    std::size_t allowed_;
    std::size_t count_ = 0;
};

thread_local allocation_probe* allocation_probe::active_ = nullptr;

void* allocate(std::size_t size) {
    allocation_probe::record();
    if (void* result = std::malloc(size == 0 ? 1 : size)) return result;
    throw std::bad_alloc();
}

template<typename Grid>
auto membership(const Grid& grid) {
    using entry = std::pair<typename Grid::cell_type,
                            std::vector<typename Grid::index_type>>;
    std::vector<entry> result;
    for (const auto& [cell, indices] : grid.cell_contents()) {
        auto& copy = result.emplace_back(cell,
            std::vector<typename Grid::index_type>(indices.begin(), indices.end()));
        std::sort(copy.second.begin(), copy.second.end());
    }
    std::sort(result.begin(), result.end(),
        [](const entry& a, const entry& b) { return a.first < b.first; });
    return result;
}

} // namespace

void* operator new(std::size_t size) { return allocate(size); }
void* operator new[](std::size_t size) { return allocate(size); }
void operator delete(void* ptr) noexcept { std::free(ptr); }
void operator delete[](void* ptr) noexcept { std::free(ptr); }
void operator delete(void* ptr, std::size_t) noexcept { std::free(ptr); }
void operator delete[](void* ptr, std::size_t) noexcept { std::free(ptr); }

TEST_CASE("Rebucketing preserves membership at every allocation failure",
          "[allocation][regression][update_one][update]") {
    using grid_type = spatial::sparse_spatial_hash<Particle, 2, float, std::size_t, 1>;
    const spatial::grid_config<2> config{
        .cell_size = {1.f, 1.f},
        .world_size = {2.f, 2.f},
        .topology_type = spatial::topology::infinite
    };

    for (bool full_destination : {false, true}) {
        for (bool bulk_update : {false, true}) {
            DYNAMIC_SECTION("full destination: " << full_destination << ", bulk: " << bulk_update) {
                std::vector<Particle> original{{-0.5f, -0.5f, 0.f, 0}};
                if (full_destination) original.push_back({0.5f, -0.5f, 0.f, 1});
                auto moved = original;
                moved[0].x = 0.5f;

                std::size_t allocations;
                {
                    grid_type grid(config);
                    grid.rebuild(original);
                    allocation_probe probe;
                    if (bulk_update) grid.update(moved);
                    else grid.update_one(0, moved[0]);
                    allocations = probe.count();
                }
                REQUIRE(allocations > 0);

                for (std::size_t allowed = 0; allowed < allocations; ++allowed) {
                    INFO("successful allocations before failure: " << allowed);
                    grid_type grid(config);
                    grid.rebuild(original);
                    const auto before = membership(grid);
                    const auto old_cell = grid.entity_cell(0);
                    bool failed = false;
                    {
                        allocation_probe probe(allowed);
                        try {
                            if (bulk_update) grid.update(moved);
                            else grid.update_one(0, moved[0]);
                        } catch (const std::bad_alloc&) {
                            failed = true;
                        }
                    }
                    REQUIRE(failed);
                    REQUIRE(grid.entity_count() == original.size());
                    REQUIRE(grid.entity_cell(0) == old_cell);
                    REQUIRE(membership(grid) == before);
                    REQUIRE(grid.validate());

                    // The same operation can be retried successfully after failure.
                    if (bulk_update) grid.update(moved);
                    else grid.update_one(0, moved[0]);
                    REQUIRE(grid.validate());
                    REQUIRE(grid.entity_cell(0) != old_cell);
                }
            }
        }
    }
}

TEST_CASE("Rebucketing remains valid across map rehashes",
          "[allocation][regression][update_one]") {
    spatial::sparse_spatial_hash<Particle, 2> grid(1.f, {2.f, 2.f},
                                                spatial::topology::infinite);
    std::vector<Particle> entities(64, Particle{-0.5f, -0.5f, 0.f, 0});
    grid.rebuild(entities);
    const auto original_buckets = grid.cell_contents().base().bucket_count();
    for (std::size_t i = 0; i < entities.size(); ++i) {
        entities[i].x = static_cast<float>(i) + 10.f;
        grid.update_one(i, entities[i]);
        REQUIRE(grid.validate());
    }
    REQUIRE(grid.cell_count() == entities.size());
    REQUIRE(grid.cell_contents().base().bucket_count() > original_buckets);
}

TEST_CASE("Bulk update retains a consistent grid after a later allocation failure",
          "[allocation][regression][update]") {
    const spatial::grid_config<2> config{
        .cell_size = {1.f, 1.f},
        .world_size = {2.f, 2.f},
        .topology_type = spatial::topology::infinite
    };
    const std::vector<Particle> original(3, Particle{-0.5f, -0.5f, 0.f, 0});
    auto moved = original;
    for (std::size_t i = 0; i < moved.size(); ++i) {
        moved[i].x = static_cast<float>(i) + 10.f;
    }
    std::size_t allocations;
    {
        spatial::sparse_spatial_hash<Particle, 2> grid(config);
        grid.rebuild(original);
        allocation_probe probe;
        grid.update(moved);
        allocations = probe.count();
    }
    REQUIRE(allocations >= moved.size());
    for (std::size_t allowed = 0; allowed < allocations; ++allowed) {
        INFO("successful allocations before failure: " << allowed);
        spatial::sparse_spatial_hash<Particle, 2> grid(config);
        grid.rebuild(original);
        bool failed = false;
        {
            allocation_probe probe(allowed);
            try { grid.update(moved); }
            catch (const std::bad_alloc&) { failed = true; }
        }
        REQUIRE(failed);
        REQUIRE(grid.entity_count() == original.size());
        REQUIRE(grid.validate());
        grid.update(moved);
        REQUIRE(grid.validate());
        REQUIRE(grid.cell_count() == moved.size());
    }
}

namespace {

struct hash_failure {};

struct throwing_hash {
    inline static bool fail = false;
    std::size_t operator()(const spatial::cell_coord<2, int>& cell) const {
        if (fail && cell.coords == std::array<int, 2>{0, 0}) throw hash_failure{};
        return spatial::cell_hash<2, int>{}(cell);
    }
};

} // namespace

TEST_CASE("Rebucketing rolls back a prepared destination if hashing fails",
          "[allocation][regression][update_one][hash]") {
    using grid_type = spatial::sparse_spatial_hash<Particle, 2, float, std::size_t, 16,
                                                 throwing_hash>;
    grid_type grid(1.f, {2.f, 2.f}, spatial::topology::infinite);
    grid.rebuild(std::vector<Particle>{{-0.5f, -0.5f, 0.f, 0}});
    const auto before = membership(grid);
    const auto old_cell = grid.entity_cell(0);
    bool failed = false;
    throwing_hash::fail = true;
    try { grid.update_one(0, Particle{10.f, -0.5f, 0.f, 0}); }
    catch (const hash_failure&) { failed = true; }
    throwing_hash::fail = false;
    REQUIRE(failed);
    REQUIRE(membership(grid) == before);
    REQUIRE(grid.entity_cell(0) == old_cell);
    REQUIRE(grid.validate());
}

namespace {

struct allocation_point {
    std::array<float, 4> coordinates{};
    float operator[](std::size_t dim) const { return coordinates[dim]; }
};

template<std::size_t Dims>
void check_query_allocations() {
    std::array<float, Dims> world;
    world.fill(2.f);
    std::array<float, Dims> center;
    center.fill(-0.5f);
    std::vector<allocation_point> points(3);
    for (auto& point : points) point.coordinates.fill(-0.5f);
    points[1].coordinates[0] = 0.5f;
    points[2].coordinates[1] = 0.5f;

    for (auto topology : {spatial::topology::bounded, spatial::topology::toroidal,
                          spatial::topology::infinite}) {
        DYNAMIC_SECTION("topology " << static_cast<int>(topology)) {
            spatial::sparse_spatial_hash<allocation_point, Dims> grid(1.f, world, topology);
            grid.rebuild(points);
            std::array<unsigned, 3> visits{};
            unsigned pairs = 0;
            std::size_t allocations;
            {
                allocation_probe probe;
                grid.for_each_in_radius(1.f, center,
                    [&](std::size_t index) { ++visits[index]; });
                grid.for_each_pair(1.f, [&](std::size_t, std::size_t) { ++pairs; });
                allocations = probe.count();
            }
            CHECK(allocations == 0);
            CHECK(visits == std::array<unsigned, 3>{1, 1, 1});
            CHECK(pairs == 3);

            grid.clear();
            unsigned empty_visits = 0;
            {
                allocation_probe probe;
                grid.for_each_in_radius(1.f, center,
                    [&](std::size_t) { ++empty_visits; });
                grid.for_each_pair(1.f, [&](std::size_t, std::size_t) { ++empty_visits; });
                allocations = probe.count();
            }
            CHECK(allocations == 0);
            CHECK(empty_visits == 0);
        }
    }
}

} // namespace

TEST_CASE("2D callback neighborhoods do not allocate", "[allocation][neighborhood]") {
    check_query_allocations<2>();
}

TEST_CASE("3D callback neighborhoods do not allocate", "[allocation][neighborhood]") {
    check_query_allocations<3>();
}

TEST_CASE("4D callback neighborhoods do not allocate", "[allocation][neighborhood]") {
    check_query_allocations<4>();
}
