/**
 * Shared test fixtures for sparse_spatial_hash test suite.
 *
 * Defines the canonical Particle test entity and its 2D/3D position_accessor
 * specializations. Including this header in multiple translation units is
 * safe: full class-template specializations follow normal class ODR (one
 * identical definition per TU), and the static get() member is implicitly
 * inline because it is defined inside the class body.
 */
#pragma once

#include <spatial/sparse_spatial_hash.hpp>
#include <cstddef>

struct Particle {
    float x, y, z;
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
