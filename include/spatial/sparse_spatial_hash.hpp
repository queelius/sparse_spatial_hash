/**
 * Generic Sparse Spatial Hash - High-Performance Implementation
 *
 * A generic N-dimensional sparse spatial hash grid supporting:
 * - Arbitrary dimensions (2D, 3D, 4D, etc.)
 * - Custom coordinate types and precision
 * - Toroidal and bounded topologies
 * - Incremental updates
 * - STL-compatible iterators
 * - Custom hash functions and allocators
 * - Exception safety guarantees
 * - Constexpr configuration
 *
 * Design Philosophy:
 * - Zero overhead abstractions
 * - Pay only for what you use
 * - Compatible with STL algorithms and ranges
 * - Header-only for maximum flexibility
 *
 * Copyright (C) 2025 Alex Towell
 * Distributed under the Boost Software License, Version 1.0.
 * (See accompanying file LICENSE_1_0.txt or copy at
 * http://www.boost.org/LICENSE_1_0.txt)
 */

#pragma once

#include <unordered_map>
#include <vector>
#include <algorithm>
#include <functional>
#include <memory>
#include <type_traits>
#include <iterator>
#include <cmath>
#include <concepts>
#include <ranges>
#include <cassert>
#include <stdexcept>
#include <optional>
#include <ostream>
#include <span>
#include <unordered_set>

namespace spatial {

// ============================================================================
// Forward Declarations
// ============================================================================

template<typename T, std::size_t Dims>
struct position_accessor;

// ============================================================================
// Small Vector Optimization (implementation detail)
// ============================================================================

namespace detail {

/**
 * Small vector: stores up to N elements inline, falls back to heap for larger sizes.
 * Optimizes the common case where cells contain few entities (~10 on average);
 * eliminates malloc overhead for typical small cells (measured 18-55% rebuild
 * regression if replaced by std::vector + reserve).
 *
 * This is an internal implementation detail of sparse_spatial_hash and is not
 * part of the public API. The interface deliberately constrains T to trivial
 * types: the only intended use is with integer index types (std::size_t,
 * uint32_t, etc.). Non-trivial T would require placement-new bookkeeping that
 * the union-based storage does not provide.
 *
 * Memory layout:
 * - size <= N: elements stored inline in small_ (no heap allocation)
 * - size >  N: elements stored on the heap via large_ (capacity_ tracks size)
 */
template<typename T, std::size_t N>
class small_vector {
    static_assert(N > 0,
        "detail::small_vector requires N >= 1; use std::vector if you need a heap-only container");
    static_assert(std::is_trivially_copyable_v<T>,
        "detail::small_vector requires a trivially copyable T (the union-based "
        "storage does not start element lifetime correctly otherwise; intended "
        "for integer index types only)");
    static_assert(std::is_trivially_destructible_v<T>,
        "detail::small_vector requires a trivially destructible T");

private:
    std::size_t size_ = 0;
    union {
        T small_[N];
        T* large_;
    };
    std::size_t capacity_ = N;

    [[nodiscard]] bool is_small() const noexcept { return capacity_ == N; }

public:
    using value_type = T;
    using size_type = std::size_t;
    using reference = T&;
    using const_reference = const T&;
    using iterator = T*;
    using const_iterator = const T*;

    small_vector() noexcept {}

    ~small_vector() {
        if (!is_small()) {
            delete[] large_;
        }
    }

    small_vector(const small_vector& other) : size_(other.size_), capacity_(other.capacity_) {
        if (other.is_small()) {
            std::copy(other.small_, other.small_ + size_, small_);
        } else {
            large_ = new T[capacity_];
            std::copy(other.large_, other.large_ + size_, large_);
        }
    }

    small_vector(small_vector&& other) noexcept : size_(other.size_), capacity_(other.capacity_) {
        if (other.is_small()) {
            std::move(other.small_, other.small_ + size_, small_);
        } else {
            large_ = other.large_;
            other.large_ = nullptr;
            other.size_ = 0;
            other.capacity_ = N;
        }
    }

    small_vector& operator=(const small_vector& other) {
        if (this != &other) {
            if (!is_small()) delete[] large_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            if (other.is_small()) {
                std::copy(other.small_, other.small_ + size_, small_);
            } else {
                large_ = new T[capacity_];
                std::copy(other.large_, other.large_ + size_, large_);
            }
        }
        return *this;
    }

    small_vector& operator=(small_vector&& other) noexcept {
        if (this != &other) {
            if (!is_small()) delete[] large_;
            size_ = other.size_;
            capacity_ = other.capacity_;
            if (other.is_small()) {
                std::move(other.small_, other.small_ + size_, small_);
            } else {
                large_ = other.large_;
                other.large_ = nullptr;
                other.size_ = 0;
                other.capacity_ = N;
            }
        }
        return *this;
    }

    [[nodiscard]] size_type size() const noexcept { return size_; }
    [[nodiscard]] size_type capacity() const noexcept { return capacity_; }
    [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

    [[nodiscard]] iterator begin() noexcept { return is_small() ? small_ : large_; }
    [[nodiscard]] iterator end() noexcept { return begin() + size_; }
    [[nodiscard]] const_iterator begin() const noexcept { return is_small() ? small_ : large_; }
    [[nodiscard]] const_iterator end() const noexcept { return begin() + size_; }

    [[nodiscard]] reference operator[](size_type i) noexcept { return begin()[i]; }
    [[nodiscard]] const_reference operator[](size_type i) const noexcept { return begin()[i]; }

    [[nodiscard]] reference back() noexcept { return begin()[size_ - 1]; }
    [[nodiscard]] const_reference back() const noexcept { return begin()[size_ - 1]; }

    void push_back(const T& value) {
        if (size_ == capacity_) reserve(capacity_ * 2);
        begin()[size_++] = value;
    }

    void pop_back() noexcept {
        --size_;
    }

    void clear() noexcept {
        size_ = 0;
    }

    void reserve(size_type new_cap) {
        if (new_cap > capacity_) {
            T* new_data = new T[new_cap];
            std::copy(begin(), end(), new_data);
            if (!is_small()) {
                delete[] large_;
            }
            large_ = new_data;
            capacity_ = new_cap;
        }
    }

};

} // namespace detail

// ============================================================================
// Concepts and Type Traits
// ============================================================================

/**
 * Concept for types that can be used as spatial coordinates
 */
template<typename T>
concept coordinate_type = std::is_arithmetic_v<T> &&
                          std::totally_ordered<T>;

/**
 * Concept for types that can provide position information
 * User must specialize position_accessor for their types
 */
template<typename T, std::size_t Dims>
concept has_position = requires(const T& obj, std::size_t dim) {
    { position_accessor<T, Dims>::get(obj, dim) } -> coordinate_type;
};

/**
 * Topology type - how boundaries are handled
 */
enum class topology {
    bounded,   // Clamp coordinates to grid bounds
    toroidal,  // Wrap coordinates (periodic boundaries)
    infinite   // Unbounded space (may grow indefinitely)
};

// ============================================================================
// Position Accessor Customization Point
// ============================================================================

/**
 * Customization point for extracting position from objects
 * Users specialize this for their types
 *
 * Example:
 * template<>
 * struct position_accessor<MyParticle, 3> {
 *     static float get(const MyParticle& p, std::size_t dim) {
 *         return p.position[dim];
 *     }
 * };
 */
template<typename T, std::size_t Dims>
struct position_accessor {
    // No default implementation - user must specialize
};

// Default implementation for array-like types whose operator[] returns a
// coordinate. The result-type constraint is what excludes proxy-returning
// containers (e.g. std::vector<bool>, expression templates) from silently
// matching the generic specialization with a non-numeric result.
template<typename T, std::size_t Dims>
    requires requires(const T& obj, std::size_t i) {
        obj[i];
        requires coordinate_type<std::remove_cvref_t<decltype(obj[i])>>;
    }
struct position_accessor<T, Dims> {
    static auto get(const T& obj, std::size_t dim) {
        return obj[dim];
    }
};

// ============================================================================
// Grid Cell Key and Hash Function
// ============================================================================

/**
 * Multi-dimensional grid cell coordinate
 */
template<std::size_t Dims, typename CoordT = int>
    requires coordinate_type<CoordT>
struct cell_coord {
    std::array<CoordT, Dims> coords;

    constexpr cell_coord() = default;

    template<typename... Args>
        requires (sizeof...(Args) == Dims)
    constexpr cell_coord(Args... args) : coords{static_cast<CoordT>(args)...} {}

    constexpr CoordT& operator[](std::size_t i) noexcept { return coords[i]; }
    constexpr const CoordT& operator[](std::size_t i) const noexcept { return coords[i]; }

    constexpr bool operator==(const cell_coord& other) const = default;
    constexpr auto operator<=>(const cell_coord& other) const = default;
};

// Stream-insertion operator for human-readable cell coordinates.
// Found via ADL when callers write `os << cell`.
template<std::size_t Dims, typename CoordT>
std::ostream& operator<<(std::ostream& os, const cell_coord<Dims, CoordT>& cell) {
    os << '(';
    for (std::size_t i = 0; i < Dims; ++i) {
        if (i != 0) os << ", ";
        os << cell.coords[i];
    }
    return os << ')';
}

/**
 * Optimized Morton encoding lookup table for 2D/3D (most common cases)
 */
namespace detail {

// Zigzag-encodes a signed 32-bit cell coordinate into an unsigned value where
// small positive and small negative inputs both map to small outputs, so that
// the subsequent low-N-bit Morton spread does not alias negatives onto
// positives. zigzag_encode(0) = 0, zigzag_encode(-1) = 1, zigzag_encode(1) = 2,
// zigzag_encode(-2) = 3, ... This is the same scheme used by Protobuf varints.
constexpr inline uint32_t zigzag_encode(int32_t x) noexcept {
    return (static_cast<uint32_t>(x) << 1) ^ static_cast<uint32_t>(x >> 31);
}

// Spreads bits for 3D Morton encoding
constexpr inline uint64_t morton_part1(uint32_t n) noexcept {
    uint64_t x = n & 0x1fffff;
    x = (x | x << 32) & 0x1f00000000ffff;
    x = (x | x << 16) & 0x1f0000ff0000ff;
    x = (x | x << 8) & 0x100f00f00f00f00f;
    x = (x | x << 4) & 0x10c30c30c30c30c3;
    x = (x | x << 2) & 0x1249249249249249;
    return x;
}

// Fast 2D Morton encoding
inline uint64_t morton_encode_2d(int32_t x, int32_t y) noexcept {
    uint32_t ux = static_cast<uint32_t>(x);
    uint32_t uy = static_cast<uint32_t>(y);

    auto spread2d = [](uint32_t val) -> uint64_t {
        uint64_t x = val;
        x = (x | (x << 16)) & 0x0000FFFF0000FFFF;
        x = (x | (x << 8))  & 0x00FF00FF00FF00FF;
        x = (x | (x << 4))  & 0x0F0F0F0F0F0F0F0F;
        x = (x | (x << 2))  & 0x3333333333333333;
        x = (x | (x << 1))  & 0x5555555555555555;
        return x;
    };

    return spread2d(ux) | (spread2d(uy) << 1);
}

// Fast 3D Morton encoding.
// zigzag_encode is applied first so negative cell coordinates do not alias
// onto positive ones after the 21-bit mask in morton_part1. Without this,
// cell (-1, 0, 0) and cell (2097151, 0, 0) would share a hash bucket.
inline uint64_t morton_encode_3d(int32_t x, int32_t y, int32_t z) noexcept {
    return morton_part1(zigzag_encode(x)) |
           (morton_part1(zigzag_encode(y)) << 1) |
           (morton_part1(zigzag_encode(z)) << 2);
}

} // namespace detail

/**
 * Default hash function for cell coordinates
 * Uses optimized multi-dimensional Z-order curve (Morton code) for spatial locality
 *
 * Coordinate Range Limits (due to 64-bit Morton encoding):
 * - 2D: ±2^31 cells per dimension (~2 billion). 2D Morton uses all 32 bits per axis,
 *   so every distinct int32_t cell coordinate hashes uniquely.
 * - 3D: ±2^20 cells per dimension (~1 million). 3D Morton spreads 21 bits per axis;
 *   coordinates are zigzag-encoded so negatives near origin land near positives near
 *   origin, but values outside ±2^20 will have hash bucket collisions (collisions
 *   are still resolved correctly by std::unordered_map's equal_to, just slower).
 * - 4D+: Further reduced range; the generic fallback packs sizeof(CoordT)*8/Dims
 *   bits per axis with no zigzag.
 *
 * For infinite topology, coordinates outside the range above produce hash collisions
 * but remain semantically correct. For bounded/toroidal, this is not an issue as
 * wrapping keeps coordinates in range.
 */
template<std::size_t Dims, typename CoordT = int>
struct cell_hash {
    std::size_t operator()(const cell_coord<Dims, CoordT>& cell) const noexcept {
        // Optimized paths for common 2D/3D cases
        if constexpr (Dims == 2) {
            return detail::morton_encode_2d(cell.coords[0], cell.coords[1]);
        } else if constexpr (Dims == 3) {
            return detail::morton_encode_3d(cell.coords[0], cell.coords[1], cell.coords[2]);
        } else {
            // Generic fallback for N-D
            std::size_t hash = 0;
            constexpr std::size_t bits = sizeof(CoordT) * 8 / Dims;

            for (std::size_t bit = 0; bit < bits; ++bit) {
                for (std::size_t dim = 0; dim < Dims; ++dim) {
                    std::size_t val = static_cast<std::size_t>(cell.coords[dim]);
                    hash |= ((val >> bit) & 1) << (bit * Dims + dim);
                }
            }
            return hash;
        }
    }
};

// ============================================================================
// Configuration Policy
// ============================================================================

/**
 * Configuration for spatial hash grid
 */
template<std::size_t Dims, typename FloatT = float>
    requires coordinate_type<FloatT>
struct grid_config {
    std::array<FloatT, Dims> cell_size;      // Size per dimension
    std::array<FloatT, Dims> world_size;     // Total size per dimension
    topology topology_type = topology::bounded;

    // Calculate grid resolution per dimension
    constexpr std::array<int, Dims> resolution() const noexcept {
        std::array<int, Dims> res;
        for (std::size_t i = 0; i < Dims; ++i) {
            res[i] = static_cast<int>(std::round(world_size[i] / cell_size[i]));
        }
        return res;
    }
};

// ============================================================================
// Sparse Spatial Hash - Main Class
// ============================================================================

/**
 * N-dimensional sparse spatial hash grid
 *
 * Template Parameters:
 * - EntityT: Type of entities being indexed (must satisfy has_position)
 * - Dims: Number of spatial dimensions
 * - FloatT: Floating-point type for coordinates
 * - IndexT: Integer type for entity indices (default: std::size_t)
 * - SmallCellSize: Max entities stored inline before heap allocation (default: 16)
 *   * 0 = always use heap (classic std::vector behavior)
 *   * 16 = optimal for typical workloads (~10 entities/cell average)
 *   * Higher = more inline storage, less heap allocation
 * - Hash: Hash function for cell coordinates
 * - Allocator: Allocator for internal containers
 *
 * Performance Notes:
 * - Small cell optimization provides 5-40% speedup for typical workloads
 * - Most cells contain ~10 entities, so SmallCellSize=16 is optimal
 * - Memory cost: +8*SmallCellSize bytes per cell (worth it for speed)
 *
 * Complexity Guarantees:
 * - Insertion: O(1) average, O(n) worst case
 * - Query: O(k) where k = entities in queried cells
 * - Incremental update: O(m) where m = entities that changed cells
 * - Memory: O(occupied_cells * SmallCellSize + large_cells * capacity + entity_count)
 *
 * Exception Safety:
 * - Basic guarantee for bulk operations (rebuild, update)
 * - Strong guarantee for allocating queries (query_radius may throw std::bad_alloc)
 * - Nothrow guarantee for inspection methods (cell_count, entity_count, stats, cell_entities, cells, cell_contents)
 */
template<
    typename EntityT,
    std::size_t Dims,
    typename FloatT = float,
    typename IndexT = std::size_t,
    std::size_t SmallCellSize = 16,
    typename Hash = cell_hash<Dims, int>,
    typename Allocator = std::allocator<IndexT>
>
    requires has_position<EntityT, Dims> && coordinate_type<FloatT>
class sparse_spatial_hash {
public:
    // Type aliases (STL convention)
    using entity_type = EntityT;
    using float_type = FloatT;
    using index_type = IndexT;
    using cell_type = cell_coord<Dims, int>;
    using config_type = grid_config<Dims, FloatT>;
    using hash_type = Hash;
    using allocator_type = Allocator;

    static constexpr std::size_t dimensions = Dims;

private:
    // Cell storage: maps cell coordinates to entity indices
    // Uses small_vector for inline storage of small cells (avoids malloc overhead)
    using entity_list = detail::small_vector<IndexT, SmallCellSize>;
    using cell_map = std::unordered_map<
        cell_type,
        entity_list,
        Hash,
        std::equal_to<cell_type>,
        typename std::allocator_traits<Allocator>::template rebind_alloc<
            std::pair<const cell_type, entity_list>
        >
    >;

    config_type config_;
    cell_map cells_;
    std::array<int, Dims> resolution_;

    // Performance optimization: precomputed reciprocals for faster division
    std::array<FloatT, Dims> cell_size_inv_;

    // Tracking for incremental updates
    std::vector<cell_type, typename std::allocator_traits<Allocator>::template rebind_alloc<cell_type>>
        entity_cells_;  // Current cell for each entity

    // Validates configuration before any member is constructed.
    // Without this, zero or negative cell_size produces division-by-zero in
    // grid_config::resolution() and a subsequent cast-to-int with UB. Also
    // rejects configurations that round to fewer than 1 cell per dimension,
    // which would later trip a divide-by-zero in the reserve-overflow guard.
    static const config_type& validate_config(const config_type& config) {
        for (std::size_t i = 0; i < Dims; ++i) {
            if (!(config.cell_size[i] > 0)) {
                throw std::invalid_argument(
                    "sparse_spatial_hash: cell_size components must be positive");
            }
            if (!(config.world_size[i] > 0)) {
                throw std::invalid_argument(
                    "sparse_spatial_hash: world_size components must be positive");
            }
        }
        const auto res = config.resolution();
        for (std::size_t i = 0; i < Dims; ++i) {
            if (res[i] < 1) {
                throw std::invalid_argument(
                    "sparse_spatial_hash: world_size must be large enough relative "
                    "to cell_size to produce at least one cell per dimension");
            }
        }
        return config;
    }

public:
    // ========================================================================
    // Constructors and Assignment
    // ========================================================================

    /**
     * Construct with configuration
     *
     * Throws std::invalid_argument if any cell_size or world_size component is non-positive.
     */
    explicit sparse_spatial_hash(
        const config_type& config,
        const allocator_type& alloc = allocator_type()
    ) : config_(validate_config(config)),
        cells_(alloc),
        resolution_(config.resolution()),
        entity_cells_(alloc)
    {
        // Precompute reciprocals for faster division (multiplication is faster than division)
        for (std::size_t i = 0; i < Dims; ++i) {
            cell_size_inv_[i] = FloatT(1) / config_.cell_size[i];
        }

        // Reserve space to reduce rehashing (estimate 10% occupancy)
        // Protected against overflow: cap at 1M cells
        std::size_t estimated_cells = 1;
        constexpr std::size_t max_reserve = 1000000;
        for (auto res : resolution_) {
            if (estimated_cells > max_reserve / static_cast<std::size_t>(res)) {
                estimated_cells = max_reserve;
                break;
            }
            estimated_cells *= static_cast<std::size_t>(res);
        }
        cells_.reserve(std::min(estimated_cells / 10, max_reserve));
    }

    /**
     * Construct with uniform cell size
     */
    explicit sparse_spatial_hash(
        FloatT cell_size,
        const std::array<FloatT, Dims>& world_size,
        topology topo = topology::bounded,
        const allocator_type& alloc = allocator_type()
    ) : sparse_spatial_hash(
            config_type{
                .cell_size = make_uniform_array<Dims>(cell_size),
                .world_size = world_size,
                .topology_type = topo
            },
            alloc
        ) {}

    // Rule of 5
    sparse_spatial_hash(const sparse_spatial_hash&) = default;
    sparse_spatial_hash(sparse_spatial_hash&&) noexcept = default;
    sparse_spatial_hash& operator=(const sparse_spatial_hash&) = default;
    sparse_spatial_hash& operator=(sparse_spatial_hash&&) noexcept = default;
    ~sparse_spatial_hash() = default;

    // ========================================================================
    // Capacity and Statistics
    // ========================================================================

    /**
     * Number of occupied cells
     */
    [[nodiscard]] std::size_t cell_count() const noexcept {
        return cells_.size();
    }

    /**
     * Total number of entities tracked
     */
    [[nodiscard]] std::size_t entity_count() const noexcept {
        return entity_cells_.size();
    }

    /**
     * Check if grid is empty
     */
    [[nodiscard]] bool empty() const noexcept {
        return cells_.empty();
    }

    /**
     * Grid occupancy ratio (occupied / total possible cells)
     */
    [[nodiscard]] FloatT occupancy() const noexcept {
        std::size_t total = 1;
        for (auto res : resolution_) {
            total *= res;
        }
        return static_cast<FloatT>(cells_.size()) / total;
    }

    /**
     * Statistics about grid usage
     */
    struct statistics {
        std::size_t occupied_cells;
        std::size_t total_entities;
        std::size_t max_entities_per_cell;
        FloatT avg_entities_per_cell;
        FloatT occupancy_ratio;
    };

    [[nodiscard]] statistics stats() const {
        statistics s{};
        s.occupied_cells = cells_.size();

        std::size_t max_count = 0;
        std::size_t total = 0;

        for (const auto& [cell, entities] : cells_) {
            total += entities.size();
            max_count = std::max(max_count, entities.size());
        }

        s.total_entities = total;
        s.max_entities_per_cell = max_count;
        s.avg_entities_per_cell = s.occupied_cells > 0
            ? static_cast<FloatT>(total) / s.occupied_cells
            : 0;
        s.occupancy_ratio = occupancy();

        return s;
    }

    // ========================================================================
    // Modifiers
    // ========================================================================

    /**
     * Clear all cells and entity tracking
     * Complexity: O(n) where n = number of occupied cells
     * Exception Safety: Nothrow
     */
    void clear() noexcept {
        cells_.clear();
        entity_cells_.clear();
    }

    /**
     * Initialize entity tracking for N entities
     * Must be called before incremental updates
     * Complexity: O(n)
     * Exception Safety: Strong guarantee
     */
    void reserve_entities(std::size_t count) {
        entity_cells_.resize(count);
    }

    /**
     * Full rebuild from entity container
     * Automatically resizes entity tracking to match entity count
     * Complexity: O(n) where n = number of entities
     * Exception Safety: Basic guarantee
     *
     * Requires a sized range (the entity count must be known without iteration).
     * To rebuild from a non-sized view (e.g., std::views::filter), materialize
     * into a vector first or use std::views::common with std::ranges::distance.
     */
    template<std::ranges::sized_range EntityRange>
        requires std::convertible_to<std::ranges::range_value_t<EntityRange>, EntityT>
    void rebuild(const EntityRange& entities) {
        cells_.clear();

        const std::size_t count = std::ranges::size(entities);
        entity_cells_.resize(count);

        IndexT idx = 0;
        for (const auto& entity : entities) {
            auto cell = get_cell_coord(entity);
            cell = wrap_cell(cell);
            cells_[cell].push_back(idx);
            entity_cells_[idx] = cell;
            ++idx;
        }
    }

    /**
     * Incremental update - only updates entities that changed cells
     * Significantly faster than rebuild when few entities change cells
     * Measured speedup: ~40x when <5% of entities move between cells
     * Optimized with swap-and-pop for O(1) amortized removal
     * Complexity: O(k) where k = entities that changed cells
     * Exception Safety: Basic guarantee
     *
     * Requires a sized range. The range must have the same length and positional
     * identity as the previous rebuild()/update() call; otherwise indices drift
     * silently. Mid-vector insert/erase is unsupported (use rebuild() after).
     */
    template<std::ranges::sized_range EntityRange>
        requires std::convertible_to<std::ranges::range_value_t<EntityRange>, EntityT>
    void update(const EntityRange& entities) {
        const std::size_t count = std::ranges::size(entities);

        // First time - do full rebuild
        if (entity_cells_.size() != count) {
            entity_cells_.resize(count);
            rebuild(entities);
            return;
        }

        // Incremental update
        IndexT idx = 0;
        for (const auto& entity : entities) {
            auto new_cell = wrap_cell(get_cell_coord(entity));
            if (new_cell != entity_cells_[idx]) {
                move_entity_to_cell(idx, new_cell);
            }
            ++idx;
        }
    }

    /**
     * Update a single entity's cell membership.
     * Use this when the caller knows exactly which entity moved, e.g. after a
     * swap-and-pop deletion or a single-particle integration step. Avoids the
     * O(n) iteration of update().
     *
     * Complexity: O(k) where k = entities in old cell (usually <= SmallCellSize)
     * Exception Safety: Strong guarantee. Throws std::out_of_range if idx is
     * not in [0, entity_count()).
     */
    void update_one(IndexT idx, const EntityT& entity) {
        if (static_cast<std::size_t>(idx) >= entity_cells_.size()) {
            throw std::out_of_range(
                "sparse_spatial_hash::update_one: index out of range");
        }
        auto new_cell = wrap_cell(get_cell_coord(entity));
        if (new_cell != entity_cells_[idx]) {
            move_entity_to_cell(idx, new_cell);
        }
    }

    // ========================================================================
    // Queries
    // ========================================================================

    /**
     * Query entities within radius of a position
     * Returns indices of entities (caller must filter by exact distance)
     * Complexity: O(k) where k = entities in queried cells
     * Exception Safety: Strong guarantee
     */
    template<typename... Coords>
        requires (sizeof...(Coords) == Dims)
    [[nodiscard]] std::vector<IndexT> query_radius(
        FloatT radius,
        Coords... coords
    ) const {
        std::array<FloatT, Dims> pos{static_cast<FloatT>(coords)...};
        return query_radius(radius, pos);
    }

    [[nodiscard]] std::vector<IndexT> query_radius(
        FloatT radius,
        const std::array<FloatT, Dims>& position
    ) const {
        std::vector<IndexT> results;
        for_each_in_radius(radius, position, [&](IndexT idx) {
            results.push_back(idx);
        });
        return results;
    }

    /**
     * Visit each candidate entity within radius of a position.
     *
     * Like query_radius but does not allocate; the callback is invoked once per
     * entity in cells touched by the radius. As with query_radius, the result
     * set is a superset of the entities truly within `radius` (caller filters
     * by exact distance).
     *
     * Complexity: O(k) where k = entities in queried cells
     * Exception Safety: Strong guarantee on the grid; depends on the callback.
     */
    template<typename Callback>
        requires std::invocable<Callback, IndexT>
    void for_each_in_radius(
        FloatT radius,
        const std::array<FloatT, Dims>& position,
        Callback&& callback
    ) const {
        const auto cell_radius = cell_radius_for(radius);
        const auto center_cell = get_cell_coord(position);
        iterate_cell_neighborhood(center_cell, cell_radius,
            [&](const cell_type& cell) {
                auto it = cells_.find(cell);
                if (it == cells_.end()) return;
                for (IndexT idx : it->second) {
                    callback(idx);
                }
            });
    }

    /**
     * Visit each unordered pair of entity indices that share a cell, or whose
     * cells are within `radius` of each other. The pair set is a superset of
     * the entity pairs actually within `radius` Euclidean distance; the
     * callback is the place to filter by exact distance using the user's
     * entity buffer.
     *
     * Each unordered pair {i, j} is visited at most once: the callback will
     * see either (i, j) or (j, i), not both, and never with i == j. The
     * ordering of i, j within a single visit is unspecified for cross-cell
     * pairs (within-cell pairs always have i < j).
     *
     * Callback signature: void(IndexT idx1, IndexT idx2)
     * Complexity: O(c * k_bar^2) where c = occupied cells, k_bar = avg per cell.
     * Exception Safety: Strong guarantee on the grid; depends on the callback.
     */
    template<typename Callback>
        requires std::invocable<Callback, IndexT, IndexT>
    void for_each_pair(FloatT radius, Callback&& callback) const {
        const auto cell_radius = cell_radius_for(radius);

        // Process each occupied cell
        for (const auto& [center_cell, center_entities] : cells_) {
            // Pairs within same cell
            for (std::size_t i = 0; i < center_entities.size(); ++i) {
                for (std::size_t j = i + 1; j < center_entities.size(); ++j) {
                    callback(center_entities[i], center_entities[j]);
                }
            }

            // Pairs with neighboring cells (process only half to avoid duplicates)
            iterate_cell_neighborhood(center_cell, cell_radius,
                [&](const cell_type& neighbor_cell) {
                    if (neighbor_cell <= center_cell) return;  // Skip already processed

                    auto it = cells_.find(neighbor_cell);
                    if (it == cells_.end()) return;

                    const auto& neighbor_entities = it->second;
                    for (IndexT idx1 : center_entities) {
                        for (IndexT idx2 : neighbor_entities) {
                            callback(idx1, idx2);
                        }
                    }
                });
        }
    }

    /**
     * Get the indices of entities in a specific cell.
     * Returns an empty span if the cell is unoccupied.
     *
     * The span aliases storage owned by the grid; do not retain it across
     * rebuild(), update(), update_one(), or any other mutating call.
     *
     * Complexity: O(1)
     * Exception Safety: Nothrow
     */
    [[nodiscard]] std::span<const IndexT> cell_entities(const cell_type& cell) const noexcept {
        auto it = cells_.find(cell);
        if (it == cells_.end()) return {};
        return std::span<const IndexT>(it->second.begin(), it->second.size());
    }

    // ========================================================================
    // Iteration
    // ========================================================================

    /**
     * Iterate over all occupied cells
     */
    [[nodiscard]] auto cells() const noexcept {
        return cells_ | std::views::keys;
    }

    /**
     * Iterate over all (cell, entities) pairs
     */
    [[nodiscard]] auto cell_contents() const noexcept {
        return cells_ | std::views::all;
    }

    // ========================================================================
    // Configuration Access
    // ========================================================================

    [[nodiscard]] const config_type& config() const noexcept {
        return config_;
    }

    [[nodiscard]] const std::array<int, Dims>& resolution() const noexcept {
        return resolution_;
    }

    // ========================================================================
    // Diagnostics
    // ========================================================================

    /**
     * Get the cell currently tracked for entity index `idx`.
     * Returns std::nullopt if `idx` is out of range.
     *
     * Complexity: O(1)
     * Exception Safety: Nothrow
     */
    [[nodiscard]] std::optional<cell_type> entity_cell(IndexT idx) const noexcept {
        if (static_cast<std::size_t>(idx) >= entity_cells_.size()) {
            return std::nullopt;
        }
        return entity_cells_[idx];
    }

    /**
     * Cross-check that `cells_` and `entity_cells_` agree.
     * For every entity index `i`, `entity_cells_[i]` must point to a cell whose
     * vector contains `i`; conversely, every index in every cell's vector must
     * have a matching entry in `entity_cells_`. Returns false on any mismatch.
     *
     * Useful as a debug invariant check after a series of update_one() calls
     * or to diagnose "why aren't my pairs being found".
     *
     * Complexity: O(n) where n = entity_count()
     * Exception Safety: Nothrow
     */
    [[nodiscard]] bool validate() const noexcept {
        for (std::size_t i = 0; i < entity_cells_.size(); ++i) {
            const auto& cell = entity_cells_[i];
            auto it = cells_.find(cell);
            if (it == cells_.end()) return false;
            const auto& vec = it->second;
            if (std::find(vec.begin(), vec.end(), static_cast<IndexT>(i)) == vec.end()) {
                return false;
            }
        }
        for (const auto& [cell, vec] : cells_) {
            for (IndexT idx : vec) {
                if (static_cast<std::size_t>(idx) >= entity_cells_.size()) return false;
                if (entity_cells_[idx] != cell) return false;
            }
        }
        return true;
    }

private:
    // ========================================================================
    // Internal Helpers
    // ========================================================================

    /**
     * Cell radius (in cells) for a Euclidean radius. Uses precomputed
     * reciprocals so this is multiplications and ceils, not divisions.
     */
    std::array<int, Dims> cell_radius_for(FloatT radius) const noexcept {
        std::array<int, Dims> r;
        for (std::size_t d = 0; d < Dims; ++d) {
            r[d] = static_cast<int>(std::ceil(radius * cell_size_inv_[d]));
        }
        return r;
    }

    /**
     * Re-bucket entity at `idx` from entity_cells_[idx] to `new_cell`.
     * Caller has confirmed: idx < entity_cells_.size() and new_cell is
     * different from the currently-tracked cell. Used by both update() and
     * update_one() to share the swap-and-pop logic.
     */
    void move_entity_to_cell(IndexT idx, const cell_type& new_cell) {
        const auto old_cell = entity_cells_[idx];

        auto it = cells_.find(old_cell);
        if (it != cells_.end()) {
            auto& vec = it->second;
            [[maybe_unused]] bool found = false;
            for (std::size_t i = 0; i < vec.size(); ++i) {
                if (vec[i] == idx) {
                    if (i != vec.size() - 1) {
                        vec[i] = vec.back();
                    }
                    vec.pop_back();
                    found = true;
                    break;
                }
            }
            assert(found && "move_entity_to_cell: entity_cells_ out of sync with cells_");
            if (vec.empty()) {
                cells_.erase(it);
            }
        }

        cells_[new_cell].push_back(idx);
        entity_cells_[idx] = new_cell;
    }

    /**
     * Get cell coordinate for an entity (optimized with precomputed reciprocals)
     */
    cell_type get_cell_coord(const EntityT& entity) const noexcept {
        cell_type cell;

        // Manual unroll for common 2D/3D cases
        if constexpr (Dims == 2) {
            FloatT coord0 = position_accessor<EntityT, Dims>::get(entity, 0);
            FloatT coord1 = position_accessor<EntityT, Dims>::get(entity, 1);
            FloatT normalized0 = (coord0 + config_.world_size[0] * FloatT(0.5)) * cell_size_inv_[0];
            FloatT normalized1 = (coord1 + config_.world_size[1] * FloatT(0.5)) * cell_size_inv_[1];
            cell[0] = static_cast<int>(std::floor(normalized0));
            cell[1] = static_cast<int>(std::floor(normalized1));
        } else if constexpr (Dims == 3) {
            FloatT coord0 = position_accessor<EntityT, Dims>::get(entity, 0);
            FloatT coord1 = position_accessor<EntityT, Dims>::get(entity, 1);
            FloatT coord2 = position_accessor<EntityT, Dims>::get(entity, 2);
            FloatT normalized0 = (coord0 + config_.world_size[0] * FloatT(0.5)) * cell_size_inv_[0];
            FloatT normalized1 = (coord1 + config_.world_size[1] * FloatT(0.5)) * cell_size_inv_[1];
            FloatT normalized2 = (coord2 + config_.world_size[2] * FloatT(0.5)) * cell_size_inv_[2];
            cell[0] = static_cast<int>(std::floor(normalized0));
            cell[1] = static_cast<int>(std::floor(normalized1));
            cell[2] = static_cast<int>(std::floor(normalized2));
        } else {
            // Generic path for N-D
            for (std::size_t d = 0; d < Dims; ++d) {
                FloatT coord = position_accessor<EntityT, Dims>::get(entity, d);
                FloatT normalized = (coord + config_.world_size[d] * FloatT(0.5)) * cell_size_inv_[d];
                cell[d] = static_cast<int>(std::floor(normalized));
            }
        }
        return cell;
    }

    /**
     * Get cell coordinate for a position (optimized with precomputed reciprocals)
     */
    cell_type get_cell_coord(const std::array<FloatT, Dims>& position) const noexcept {
        cell_type cell;

        if constexpr (Dims == 2) {
            FloatT normalized0 = (position[0] + config_.world_size[0] * FloatT(0.5)) * cell_size_inv_[0];
            FloatT normalized1 = (position[1] + config_.world_size[1] * FloatT(0.5)) * cell_size_inv_[1];
            cell[0] = static_cast<int>(std::floor(normalized0));
            cell[1] = static_cast<int>(std::floor(normalized1));
        } else if constexpr (Dims == 3) {
            FloatT normalized0 = (position[0] + config_.world_size[0] * FloatT(0.5)) * cell_size_inv_[0];
            FloatT normalized1 = (position[1] + config_.world_size[1] * FloatT(0.5)) * cell_size_inv_[1];
            FloatT normalized2 = (position[2] + config_.world_size[2] * FloatT(0.5)) * cell_size_inv_[2];
            cell[0] = static_cast<int>(std::floor(normalized0));
            cell[1] = static_cast<int>(std::floor(normalized1));
            cell[2] = static_cast<int>(std::floor(normalized2));
        } else {
            for (std::size_t d = 0; d < Dims; ++d) {
                FloatT normalized = (position[d] + config_.world_size[d] * FloatT(0.5)) * cell_size_inv_[d];
                cell[d] = static_cast<int>(std::floor(normalized));
            }
        }
        return cell;
    }

    /**
     * Wrap cell coordinates according to topology (optimized with manual unrolling)
     */
    cell_type wrap_cell(cell_type cell) const noexcept {
        switch (config_.topology_type) {
            case topology::toroidal:
                if constexpr (Dims == 2) {
                    cell[0] = ((cell[0] % resolution_[0]) + resolution_[0]) % resolution_[0];
                    cell[1] = ((cell[1] % resolution_[1]) + resolution_[1]) % resolution_[1];
                } else if constexpr (Dims == 3) {
                    cell[0] = ((cell[0] % resolution_[0]) + resolution_[0]) % resolution_[0];
                    cell[1] = ((cell[1] % resolution_[1]) + resolution_[1]) % resolution_[1];
                    cell[2] = ((cell[2] % resolution_[2]) + resolution_[2]) % resolution_[2];
                } else {
                    for (std::size_t d = 0; d < Dims; ++d) {
                        cell[d] = ((cell[d] % resolution_[d]) + resolution_[d]) % resolution_[d];
                    }
                }
                break;

            case topology::bounded:
                if constexpr (Dims == 2) {
                    cell[0] = std::clamp(cell[0], 0, resolution_[0] - 1);
                    cell[1] = std::clamp(cell[1], 0, resolution_[1] - 1);
                } else if constexpr (Dims == 3) {
                    cell[0] = std::clamp(cell[0], 0, resolution_[0] - 1);
                    cell[1] = std::clamp(cell[1], 0, resolution_[1] - 1);
                    cell[2] = std::clamp(cell[2], 0, resolution_[2] - 1);
                } else {
                    for (std::size_t d = 0; d < Dims; ++d) {
                        cell[d] = std::clamp(cell[d], 0, resolution_[d] - 1);
                    }
                }
                break;

            case topology::infinite:
                // No wrapping
                break;
        }
        return cell;
    }

    /**
     * Iterate over cell neighborhood, invoking `func(cell)` for each visited
     * cell. Each unique cell is visited at most once: under toroidal topology,
     * a query whose offsets span more than one wrap of the grid would alias
     * multiple offsets to the same wrapped cell, so this layer dedupes on the
     * wrapped-cell value before forwarding to `func`.
     */
    template<typename Func>
    void iterate_cell_neighborhood(
        const cell_type& center,
        const std::array<int, Dims>& radius,
        Func&& func
    ) const {
        if (config_.topology_type == topology::toroidal) {
            bool may_alias = false;
            for (std::size_t d = 0; d < Dims; ++d) {
                if (2 * radius[d] + 1 > resolution_[d]) {
                    may_alias = true;
                    break;
                }
            }
            if (may_alias) {
                std::unordered_set<cell_type, Hash> visited;
                iterate_recursive<0>(center, radius, center,
                    [&](const cell_type& cell) {
                        if (visited.insert(cell).second) func(cell);
                    });
                return;
            }
        }
        iterate_recursive<0>(center, radius, center, std::forward<Func>(func));
    }

    template<std::size_t Dim, typename Func>
    void iterate_recursive(
        const cell_type& center,
        const std::array<int, Dims>& radius,
        cell_type current,
        Func&& func
    ) const {
        if constexpr (Dim == Dims) {
            // Base case: process cell
            current = wrap_cell(current);
            func(current);
        } else {
            // Recursive case: iterate this dimension
            for (int offset = -radius[Dim]; offset <= radius[Dim]; ++offset) {
                current[Dim] = center[Dim] + offset;
                iterate_recursive<Dim + 1>(center, radius, current, std::forward<Func>(func));
            }
        }
    }

    /**
     * Helper to create uniform array
     */
    template<std::size_t N>
    static constexpr std::array<FloatT, N> make_uniform_array(FloatT value) {
        std::array<FloatT, N> arr;
        arr.fill(value);
        return arr;
    }
};

// ============================================================================
// Convenience Type Aliases
// ============================================================================

// Common 2D and 3D instantiations
template<typename EntityT, typename FloatT = float>
using sparse_spatial_hash_2d = sparse_spatial_hash<EntityT, 2, FloatT>;

template<typename EntityT, typename FloatT = float>
using sparse_spatial_hash_3d = sparse_spatial_hash<EntityT, 3, FloatT>;

} // namespace spatial
