#include "test_helpers.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <memory>
#include <new>
#include <vector>

namespace {

// An index with observable comparison cost makes the complexity regression
// deterministic without depending on wall-clock timing or private helpers.
struct counting_index {
    std::size_t value;
    inline static std::size_t comparisons = 0;

    counting_index(std::size_t index = 0) : value(index) {}
    operator std::size_t() const { return value; }
    counting_index& operator++() { ++value; return *this; }
    friend bool operator==(counting_index a, counting_index b) {
        ++comparisons;
        return a.value == b.value;
    }
};

struct allocation_state {
    std::size_t allocations = 0;
    std::size_t bytes = 0;
    bool fail = false;
};

template<typename T>
struct tracking_allocator {
    using value_type = T;
    allocation_state* state = nullptr;

    tracking_allocator() = default;
    explicit tracking_allocator(allocation_state& value) : state(&value) {}
    template<typename U>
    tracking_allocator(const tracking_allocator<U>& other) : state(other.state) {}

    T* allocate(std::size_t count) {
        if (state) {
            ++state->allocations;
            if (state->fail) throw std::bad_alloc();
        }
        auto* result = std::allocator<T>{}.allocate(count);
        if (state) state->bytes += count * sizeof(T);
        return result;
    }

    void deallocate(T* pointer, std::size_t count) noexcept {
        if (state) state->bytes -= count * sizeof(T);
        std::allocator<T>{}.deallocate(pointer, count);
    }

    template<typename U>
    bool operator==(const tracking_allocator<U>& other) const { return state == other.state; }
};

const spatial::grid_config<2> config{
    .cell_size = {1.f, 1.f},
    .world_size = {2.f, 2.f},
    .topology_type = spatial::topology::bounded
};

} // namespace

TEST_CASE("Dense-cell validation performs at most linear index comparisons",
          "[validate][regression][complexity]") {
    constexpr std::size_t count = 2048;
    spatial::sparse_spatial_hash<Particle, 2, float, counting_index> grid(config);
    grid.rebuild(std::vector<Particle>(count, Particle{-0.5f, -0.5f, 0.f, 0}));
    counting_index::comparisons = 0;
    REQUIRE(grid.validate());
    CHECK(counting_index::comparisons <= 4 * count);
}

TEST_CASE("Validation rejects inconsistent entity membership", "[validate][regression]") {
    spatial::sparse_spatial_hash<Particle, 2> grid(config);
    grid.rebuild(std::vector<Particle>{{-0.5f, -0.5f, 0.f, 0},
                                      {0.5f, -0.5f, 0.f, 1}});

    SECTION("Missing membership after growing the tracking vector") {
        grid.reserve_entities(3);
        REQUIRE_FALSE(grid.validate());
    }

    SECTION("Out-of-range membership after shrinking the tracking vector") {
        grid.reserve_entities(1);
        REQUIRE_FALSE(grid.validate());
    }

    SECTION("A stored index points to the wrong cell") {
        // The grid is mutable; deliberately corrupt its exposed read-only span
        // to test diagnostics without adding a production-only test hook.
        auto first = grid.cell_entities(*grid.entity_cell(0));
        auto second = grid.cell_entities(*grid.entity_cell(1));
        std::swap(const_cast<std::size_t&>(first[0]),
                  const_cast<std::size_t&>(second[0]));
        REQUIRE_FALSE(grid.validate());
    }

    SECTION("A stored index is out of range") {
        auto indices = grid.cell_entities(*grid.entity_cell(0));
        const_cast<std::size_t&>(indices[0]) = 99;
        REQUIRE_FALSE(grid.validate());
    }

    SECTION("Repeated indices caused by overflowing a narrow index type") {
        spatial::sparse_spatial_hash<Particle, 2, float, std::uint8_t> narrow(config);
        narrow.rebuild(std::vector<Particle>(257, Particle{-0.5f, -0.5f, 0.f, 0}));
        REQUIRE_FALSE(narrow.validate());
    }
}

TEST_CASE("Validation uses the configured allocator and propagates allocation failure",
          "[validate][regression][allocator]") {
    allocation_state state;
    using allocator = tracking_allocator<std::size_t>;
    using grid_type = spatial::sparse_spatial_hash<Particle, 2, float, std::size_t, 16,
                                                  spatial::cell_hash<2, int>, allocator>;
    grid_type grid(config, allocator(state));
    grid.rebuild(std::vector<Particle>{{-0.5f, -0.5f, 0.f, 0},
                                      {0.5f, -0.5f, 0.f, 1}});
    const auto cells = grid.cell_count();
    const auto first_cell = grid.entity_cell(0);
    const auto bytes_before = state.bytes;
    const auto allocations_before = state.allocations;

    REQUIRE(grid.validate());
    CHECK(state.allocations > allocations_before);
    CHECK(state.bytes == bytes_before);

    state.fail = true;
    REQUIRE_THROWS_AS(grid.validate(), std::bad_alloc);
    state.fail = false;

    CHECK(grid.cell_count() == cells);
    CHECK(grid.entity_count() == 2);
    CHECK(grid.entity_cell(0) == first_cell);
    CHECK(state.bytes == bytes_before);
    CHECK(grid.validate());
}
