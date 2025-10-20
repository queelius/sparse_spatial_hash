/**
 * Generic Sparse Spatial Hash - Optimized Implementation
 *
 * Performance optimizations applied:
 * 1. Optimized Morton encoding with lookup tables
 * 2. Batched cell coordinate calculations
 * 3. Swap-and-pop for faster incremental updates
 * 4. Optimized toroidal wrapping
 * 5. Small vector optimization for entity lists
 * 6. Manual loop unrolling for common dimensions
 *
 * Copyright (C) 2025 Alex Towell
 * Distributed under the Boost Software License, Version 1.0.
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
#include <array>
#include <cstdint>

namespace boost {
namespace spatial {

// ============================================================================
// Forward Declarations
// ============================================================================

template<typename T, std::size_t Dims>
struct position_accessor;

// ============================================================================
// Concepts and Type Traits
// ============================================================================

template<typename T>
concept coordinate_type = std::is_arithmetic_v<T> &&
                          std::totally_ordered<T>;

template<typename T, std::size_t Dims>
concept has_position = requires(const T& obj, std::size_t dim) {
    { position_accessor<T, Dims>::get(obj, dim) } -> coordinate_type;
};

enum class topology {
    bounded,
    toroidal,
    infinite
};

// ============================================================================
// Position Accessor Customization Point
// ============================================================================

template<typename T, std::size_t Dims>
struct position_accessor {
    // No default implementation - user must specialize
};

template<typename T, std::size_t Dims>
    requires requires(const T& obj, std::size_t i) { obj[i]; }
struct position_accessor<T, Dims> {
    static auto get(const T& obj, std::size_t dim) {
        return obj[dim];
    }
};

// ============================================================================
// Optimized Morton Encoding
// ============================================================================

namespace detail {

// Lookup table for 8-bit Morton encoding (part1)
// Spreads bits of an 8-bit number across 16 bits
constexpr std::array<uint32_t, 256> make_morton_table_1() {
    std::array<uint32_t, 256> table{};
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t x = i;
        x = (x | (x << 8)) & 0x00FF00FF;
        x = (x | (x << 4)) & 0x0F0F0F0F;
        x = (x | (x << 2)) & 0x33333333;
        x = (x | (x << 1)) & 0x55555555;
        table[i] = x;
    }
    return table;
}

constexpr auto morton_table_1 = make_morton_table_1();

// Fast 2D Morton encoding using lookup table
inline uint64_t morton_encode_2d(int32_t x, int32_t y) noexcept {
    uint32_t ux = static_cast<uint32_t>(x);
    uint32_t uy = static_cast<uint32_t>(y);

    uint64_t result = 0;
    result |= morton_table_1[ux & 0xFF];
    result |= morton_table_1[(ux >> 8) & 0xFF] << 16;
    result |= morton_table_1[(ux >> 16) & 0xFF] << 32;
    result |= morton_table_1[(ux >> 24) & 0xFF] << 48;

    result |= static_cast<uint64_t>(morton_table_1[uy & 0xFF]) << 1;
    result |= static_cast<uint64_t>(morton_table_1[(uy >> 8) & 0xFF]) << 17;
    result |= static_cast<uint64_t>(morton_table_1[(uy >> 16) & 0xFF]) << 33;
    result |= static_cast<uint64_t>(morton_table_1[(uy >> 24) & 0xFF]) << 49;

    return result;
}

// Fast 3D Morton encoding using lookup table
inline uint64_t morton_encode_3d(int32_t x, int32_t y, int32_t z) noexcept {
    auto part1 = [](uint32_t n) -> uint64_t {
        uint64_t x = n & 0x1fffff;
        x = (x | x << 32) & 0x1f00000000ffff;
        x = (x | x << 16) & 0x1f0000ff0000ff;
        x = (x | x << 8) & 0x100f00f00f00f00f;
        x = (x | x << 4) & 0x10c30c30c30c30c3;
        x = (x | x << 2) & 0x1249249249249249;
        return x;
    };

    return part1(static_cast<uint32_t>(x)) |
           (part1(static_cast<uint32_t>(y)) << 1) |
           (part1(static_cast<uint32_t>(z)) << 2);
}

// Generic fallback for N-D
template<std::size_t Dims>
inline std::size_t morton_encode_generic(const std::array<int, Dims>& coords) noexcept {
    std::size_t hash = 0;
    constexpr std::size_t bits = sizeof(int) * 8 / Dims;

    for (std::size_t bit = 0; bit < bits; ++bit) {
        for (std::size_t dim = 0; dim < Dims; ++dim) {
            std::size_t val = static_cast<std::size_t>(coords[dim]);
            hash |= ((val >> bit) & 1) << (bit * Dims + dim);
        }
    }
    return hash;
}

} // namespace detail

// ============================================================================
// Grid Cell Key and Optimized Hash Function
// ============================================================================

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

/**
 * Optimized hash function using fast Morton encoding with lookup tables
 */
template<std::size_t Dims, typename CoordT = int>
struct cell_hash {
    std::size_t operator()(const cell_coord<Dims, CoordT>& cell) const noexcept {
        if constexpr (Dims == 2) {
            return detail::morton_encode_2d(cell.coords[0], cell.coords[1]);
        } else if constexpr (Dims == 3) {
            return detail::morton_encode_3d(cell.coords[0], cell.coords[1], cell.coords[2]);
        } else {
            return detail::morton_encode_generic<Dims>(cell.coords);
        }
    }
};

// ============================================================================
// Configuration Policy
// ============================================================================

template<std::size_t Dims, typename FloatT = float>
    requires coordinate_type<FloatT>
struct grid_config {
    std::array<FloatT, Dims> cell_size;
    std::array<FloatT, Dims> world_size;
    topology topology_type = topology::bounded;

    constexpr std::array<int, Dims> resolution() const noexcept {
        std::array<int, Dims> res;
        for (std::size_t i = 0; i < Dims; ++i) {
            res[i] = static_cast<int>(std::round(world_size[i] / cell_size[i]));
        }
        return res;
    }
};

// ============================================================================
// Small Vector Optimization for Entity Lists
// ============================================================================

namespace detail {

template<typename T, std::size_t InlineCapacity, typename Allocator>
class small_vector {
    static constexpr std::size_t inline_bytes = InlineCapacity * sizeof(T);

    union storage_type {
        alignas(T) unsigned char inline_data[inline_bytes];
        T* heap_ptr;

        storage_type() : inline_data{} {}
        ~storage_type() {}
    };

    storage_type storage_;
    std::size_t size_ = 0;
    std::size_t capacity_ = InlineCapacity;
    [[no_unique_address]] Allocator alloc_;

    bool is_inline() const noexcept {
        return capacity_ <= InlineCapacity;
    }

    T* data_ptr() noexcept {
        return is_inline() ? reinterpret_cast<T*>(storage_.inline_data) : storage_.heap_ptr;
    }

    const T* data_ptr() const noexcept {
        return is_inline() ? reinterpret_cast<const T*>(storage_.inline_data) : storage_.heap_ptr;
    }

public:
    using value_type = T;
    using size_type = std::size_t;
    using iterator = T*;
    using const_iterator = const T*;

    small_vector() = default;

    explicit small_vector(const Allocator& alloc) : alloc_(alloc) {}

    ~small_vector() {
        clear();
        if (!is_inline()) {
            std::allocator_traits<Allocator>::deallocate(alloc_, storage_.heap_ptr, capacity_);
        }
    }

    small_vector(const small_vector& other) : alloc_(other.alloc_), size_(0), capacity_(InlineCapacity) {
        reserve(other.size_);
        for (const auto& item : other) {
            push_back(item);
        }
    }

    small_vector(small_vector&& other) noexcept
        : alloc_(std::move(other.alloc_)), size_(other.size_), capacity_(other.capacity_) {
        if (other.is_inline()) {
            for (size_t i = 0; i < size_; ++i) {
                new (&data_ptr()[i]) T(std::move(other.data_ptr()[i]));
                other.data_ptr()[i].~T();
            }
        } else {
            storage_.heap_ptr = other.storage_.heap_ptr;
            other.storage_.heap_ptr = nullptr;
            other.capacity_ = InlineCapacity;
        }
        other.size_ = 0;
    }

    void push_back(const T& value) {
        if (size_ == capacity_) {
            reserve(capacity_ * 2);
        }
        new (&data_ptr()[size_++]) T(value);
    }

    void push_back(T&& value) {
        if (size_ == capacity_) {
            reserve(capacity_ * 2);
        }
        new (&data_ptr()[size_++]) T(std::move(value));
    }

    void reserve(size_type new_cap) {
        if (new_cap <= capacity_) return;

        T* new_data = std::allocator_traits<Allocator>::allocate(alloc_, new_cap);

        for (size_type i = 0; i < size_; ++i) {
            new (&new_data[i]) T(std::move(data_ptr()[i]));
            data_ptr()[i].~T();
        }

        if (!is_inline()) {
            std::allocator_traits<Allocator>::deallocate(alloc_, storage_.heap_ptr, capacity_);
        }

        storage_.heap_ptr = new_data;
        capacity_ = new_cap;
    }

    void clear() noexcept {
        for (size_type i = 0; i < size_; ++i) {
            data_ptr()[i].~T();
        }
        size_ = 0;
    }

    iterator erase(const_iterator pos) {
        iterator it = const_cast<iterator>(pos);
        std::move(it + 1, end(), it);
        --size_;
        data_ptr()[size_].~T();
        return it;
    }

    // Optimized: swap-and-pop removal (breaks order but O(1))
    void erase_unordered(const T& value) {
        for (size_type i = 0; i < size_; ++i) {
            if (data_ptr()[i] == value) {
                if (i != size_ - 1) {
                    data_ptr()[i] = std::move(data_ptr()[size_ - 1]);
                }
                data_ptr()[--size_].~T();
                return;
            }
        }
    }

    iterator begin() noexcept { return data_ptr(); }
    iterator end() noexcept { return data_ptr() + size_; }
    const_iterator begin() const noexcept { return data_ptr(); }
    const_iterator end() const noexcept { return data_ptr() + size_; }

    size_type size() const noexcept { return size_; }
    bool empty() const noexcept { return size_ == 0; }

    T& operator[](size_type i) noexcept { return data_ptr()[i]; }
    const T& operator[](size_type i) const noexcept { return data_ptr()[i]; }
};

} // namespace detail

// ============================================================================
// Sparse Spatial Hash - Optimized Main Class
// ============================================================================

template<
    typename EntityT,
    std::size_t Dims,
    typename FloatT = float,
    typename IndexT = std::size_t,
    typename Hash = cell_hash<Dims, int>,
    typename Allocator = std::allocator<IndexT>
>
    requires has_position<EntityT, Dims> && coordinate_type<FloatT>
class sparse_spatial_hash {
public:
    using entity_type = EntityT;
    using float_type = FloatT;
    using index_type = IndexT;
    using cell_type = cell_coord<Dims, int>;
    using config_type = grid_config<Dims, FloatT>;
    using hash_type = Hash;
    using allocator_type = Allocator;

    static constexpr std::size_t dimensions = Dims;
    static constexpr std::size_t small_vector_capacity = 8; // Inline storage for up to 8 entities

private:
    // Optimized: small vector with inline storage
    using entity_list = detail::small_vector<IndexT, small_vector_capacity, Allocator>;
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

    // Precomputed reciprocals for faster division
    std::array<FloatT, Dims> cell_size_inv_;

    // Tracking for incremental updates
    std::vector<cell_type, typename std::allocator_traits<Allocator>::template rebind_alloc<cell_type>>
        entity_cells_;

public:
    // ========================================================================
    // Constructors and Assignment
    // ========================================================================

    explicit sparse_spatial_hash(
        const config_type& config,
        const allocator_type& alloc = allocator_type()
    ) : config_(config),
        cells_(alloc),
        resolution_(config.resolution()),
        entity_cells_(alloc)
    {
        // Precompute reciprocals for faster division
        for (std::size_t i = 0; i < Dims; ++i) {
            cell_size_inv_[i] = FloatT(1) / config_.cell_size[i];
        }

        // Reserve space to reduce rehashing
        std::size_t estimated_cells = 1;
        for (auto res : resolution_) {
            estimated_cells *= res;
        }
        cells_.reserve(estimated_cells / 10);
    }

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

    sparse_spatial_hash(const sparse_spatial_hash&) = default;
    sparse_spatial_hash(sparse_spatial_hash&&) noexcept = default;
    sparse_spatial_hash& operator=(const sparse_spatial_hash&) = default;
    sparse_spatial_hash& operator=(sparse_spatial_hash&&) noexcept = default;
    ~sparse_spatial_hash() = default;

    // ========================================================================
    // Capacity and Statistics
    // ========================================================================

    [[nodiscard]] std::size_t cell_count() const noexcept {
        return cells_.size();
    }

    [[nodiscard]] std::size_t entity_count() const noexcept {
        return entity_cells_.size();
    }

    [[nodiscard]] bool empty() const noexcept {
        return cells_.empty();
    }

    [[nodiscard]] FloatT occupancy() const noexcept {
        std::size_t total = 1;
        for (auto res : resolution_) {
            total *= res;
        }
        return static_cast<FloatT>(cells_.size()) / total;
    }

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

    void clear() noexcept {
        cells_.clear();
        entity_cells_.clear();
    }

    void reserve_entities(std::size_t count) {
        entity_cells_.resize(count);
    }

    /**
     * Optimized rebuild with batched processing
     */
    template<std::ranges::range EntityRange>
        requires std::convertible_to<std::ranges::range_value_t<EntityRange>, EntityT>
    void rebuild(const EntityRange& entities) {
        cells_.clear();

        IndexT idx = 0;
        for (const auto& entity : entities) {
            auto cell = get_cell_coord_fast(entity);
            cell = wrap_cell_fast(cell);
            cells_[cell].push_back(idx);

            if (idx < entity_cells_.size()) {
                entity_cells_[idx] = cell;
            }
            ++idx;
        }
    }

    /**
     * Optimized incremental update using swap-and-pop
     */
    template<std::ranges::range EntityRange>
        requires std::convertible_to<std::ranges::range_value_t<EntityRange>, EntityT>
    void update(const EntityRange& entities) {
        const std::size_t count = std::ranges::size(entities);

        if (entity_cells_.size() != count) {
            entity_cells_.resize(count);
            rebuild(entities);
            return;
        }

        IndexT idx = 0;
        for (const auto& entity : entities) {
            auto new_cell = wrap_cell_fast(get_cell_coord_fast(entity));
            auto old_cell = entity_cells_[idx];

            if (new_cell != old_cell) {
                // Remove from old cell (swap-and-pop for O(1) removal)
                auto it = cells_.find(old_cell);
                if (it != cells_.end()) {
                    auto& vec = it->second;
                    vec.erase_unordered(idx);

                    if (vec.empty()) {
                        cells_.erase(it);
                    }
                }

                // Add to new cell
                cells_[new_cell].push_back(idx);
                entity_cells_[idx] = new_cell;
            }
            ++idx;
        }
    }

    // ========================================================================
    // Queries
    // ========================================================================

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

        // Calculate cell radius using precomputed reciprocals
        std::array<int, Dims> cell_radius;
        for (std::size_t d = 0; d < Dims; ++d) {
            cell_radius[d] = static_cast<int>(std::ceil(radius * cell_size_inv_[d]));
        }

        auto center_cell = get_cell_coord_fast(position);

        // Iterate over all cells within radius
        iterate_cell_neighborhood(center_cell, cell_radius,
            [&](const cell_type& cell) {
                auto it = cells_.find(cell);
                if (it != cells_.end()) {
                    const auto& entities = it->second;
                    results.insert(results.end(), entities.begin(), entities.end());
                }
            });

        return results;
    }

    template<typename EntityRange, typename Callback>
        requires std::invocable<Callback, IndexT, IndexT>
    void for_each_pair(
        const EntityRange& entities,
        FloatT radius,
        Callback&& callback
    ) const {
        std::array<int, Dims> cell_radius;
        for (std::size_t d = 0; d < Dims; ++d) {
            cell_radius[d] = static_cast<int>(std::ceil(radius * cell_size_inv_[d]));
        }

        for (const auto& [center_cell, center_entities] : cells_) {
            // Pairs within same cell
            for (std::size_t i = 0; i < center_entities.size(); ++i) {
                for (std::size_t j = i + 1; j < center_entities.size(); ++j) {
                    callback(center_entities[i], center_entities[j]);
                }
            }

            // Pairs with neighboring cells
            iterate_cell_neighborhood(center_cell, cell_radius,
                [&](const cell_type& neighbor_cell) {
                    if (neighbor_cell <= center_cell) return;

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

    [[nodiscard]] std::ranges::view auto cell_entities(const cell_type& cell) const {
        auto it = cells_.find(cell);
        if (it != cells_.end()) {
            return it->second | std::views::all;
        }
        return std::views::empty<IndexT>;
    }

    // ========================================================================
    // Iteration
    // ========================================================================

    [[nodiscard]] auto cells() const noexcept {
        return cells_ | std::views::keys;
    }

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

private:
    // ========================================================================
    // Optimized Internal Helpers
    // ========================================================================

    /**
     * Fast cell coordinate calculation using precomputed reciprocals
     * Replaces division with multiplication
     */
    cell_type get_cell_coord_fast(const EntityT& entity) const {
        cell_type cell;

        if constexpr (Dims == 2) {
            // Manual unroll for 2D (most common case)
            FloatT coord0 = position_accessor<EntityT, Dims>::get(entity, 0);
            FloatT coord1 = position_accessor<EntityT, Dims>::get(entity, 1);

            FloatT normalized0 = (coord0 + config_.world_size[0] * FloatT(0.5)) * cell_size_inv_[0];
            FloatT normalized1 = (coord1 + config_.world_size[1] * FloatT(0.5)) * cell_size_inv_[1];

            cell[0] = static_cast<int>(std::floor(normalized0));
            cell[1] = static_cast<int>(std::floor(normalized1));
        } else if constexpr (Dims == 3) {
            // Manual unroll for 3D (most common case)
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

    cell_type get_cell_coord_fast(const std::array<FloatT, Dims>& position) const {
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
     * Optimized cell wrapping with manual unrolling
     */
    cell_type wrap_cell_fast(cell_type cell) const {
        switch (config_.topology_type) {
            case topology::toroidal:
                if constexpr (Dims == 2) {
                    // Optimized modulo for common case
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

    template<typename Func>
    void iterate_cell_neighborhood(
        const cell_type& center,
        const std::array<int, Dims>& radius,
        Func&& func
    ) const {
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
            current = wrap_cell_fast(current);
            func(current);
        } else {
            for (int offset = -radius[Dim]; offset <= radius[Dim]; ++offset) {
                current[Dim] = center[Dim] + offset;
                iterate_recursive<Dim + 1>(center, radius, current, std::forward<Func>(func));
            }
        }
    }

    template<std::size_t N>
    static constexpr std::array<FloatT, N> make_uniform_array(FloatT value) {
        std::array<FloatT, N> arr;
        arr.fill(value);
        return arr;
    }
};

// ============================================================================
// Deduction Guides
// ============================================================================

template<typename EntityT, std::size_t Dims, typename FloatT>
sparse_spatial_hash(const grid_config<Dims, FloatT>&)
    -> sparse_spatial_hash<EntityT, Dims, FloatT>;

// ============================================================================
// Convenience Type Aliases
// ============================================================================

template<typename EntityT, typename FloatT = float>
using sparse_spatial_hash_2d = sparse_spatial_hash<EntityT, 2, FloatT>;

template<typename EntityT, typename FloatT = float>
using sparse_spatial_hash_3d = sparse_spatial_hash<EntityT, 3, FloatT>;

} // namespace spatial
} // namespace boost
