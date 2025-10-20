# CppCon Proposal Guide

Guide for proposing a talk about `sparse_spatial_hash` at CppCon.

## Talk Proposal

### Title Options

1. **"Sparse Spatial Hashing: 60,000x Memory Reduction for Game Physics"**
   - Focus: Performance and real-world application
   - Audience: Game developers, performance engineers

2. **"Generic N-Dimensional Spatial Indexing with Modern C++20"**
   - Focus: Generic programming, C++20 features
   - Audience: Library authors, C++ enthusiasts

3. **"From Game Engine to Boost Library: Extracting Production Code"**
   - Focus: Software engineering, library design
   - Audience: Architects, library designers

4. **"Spatial Data Structures Shootout: When to Use What"**
   - Focus: Comparison and trade-offs
   - Audience: Anyone working with spatial data

### Recommended: Option 1

**Title**: "Sparse Spatial Hashing: 60,000x Memory Reduction for Game Physics"

**Session Type**: Lecture (60 minutes)

**Audience Level**: Intermediate

**Tags**: Performance, Games, Data Structures, C++20

## Abstract

### Short Abstract (300 characters)

```
Spatial indexing is critical for games, physics, and robotics. We'll explore sparse spatial hashing - a data structure that achieves 60,000x memory reduction vs dense grids and 40x faster updates than R-trees, while maintaining O(1) insertion and O(k) queries. Includes real-world benchmarks from a 10M particle physics engine.
```

### Full Abstract (1500 characters)

```
Spatial indexing is fundamental to game engines, physics simulators, and robotics systems. Traditional data structures like R-trees and octrees excel at static data but struggle with highly dynamic scenes where entities move frequently.

This talk introduces sparse spatial hashing - a practical data structure that combines the best characteristics of hash tables and spatial grids. Extracted from the DigiStar physics engine handling 10M+ particles, it demonstrates:

- 60,000x memory reduction compared to dense grids (6TB → 100MB for 10M particles)
- 40x faster incremental updates compared to R-trees (2ms vs 80ms per frame)
- O(1) insertion complexity
- O(k) query complexity where k is entities in nearby cells (typically 1-10)

We'll cover:

1. **Theory**: Why sparse hashing works for uniform spatial distributions
2. **Implementation**: Generic N-dimensional design using C++20 concepts and ranges
3. **Optimizations**: Morton encoding, division reciprocals, loop unrolling (10-48% gains)
4. **Comparisons**: When to use sparse hash vs R-tree, octree, kd-tree, or dense grid
5. **Real-world performance**: Production benchmarks from game physics engine
6. **API design**: Making it Boost-quality and STL-compatible

Attendees will learn practical spatial indexing techniques applicable to games, simulations, GIS, and robotics. All code is open-source and ready for production use.

Target audience: Game developers, simulation engineers, library authors interested in generic programming and performance optimization.
```

## Presentation Outline

### Section 1: The Problem (5 minutes)

**Problem Statement**
```
Game loop: 16.67ms budget for 60 FPS
10,000 entities need collision detection
Naive O(n²): 100 million checks → 500ms → 30 FPS
Need: Fast spatial queries for dynamic scenes
```

**Demo**: Live animation showing naive vs spatial indexing

### Section 2: Spatial Data Structure Comparison (10 minutes)

**R-tree**
- Hierarchical bounding volumes
- O(log n) insertion
- Great for static data
- Slow rebalancing for dynamic scenes

**Octree**
- Hierarchical space subdivision
- Good for LOD
- Memory overhead for tree nodes
- Rebalancing complexity

**Dense Grid**
- O(1) operations
- Simple to implement
- Memory explosion: 6TB for 10,000³ cells
- Not practical for large worlds

**Sparse Hash Grid**
- O(1) insertion
- O(k) queries, k = nearby entities
- Sparse storage: only occupied cells
- Perfect for dynamic uniform distributions

**Demo**: Visual comparison with animations

### Section 3: Theory and Design (10 minutes)

**Core Concept**
```cpp
// Divide space into grid cells
Cell cell = position_to_cell(entity.position);

// Hash map: only occupied cells stored
hash_map<Cell, vector<EntityIndex>> cells;

// Insert: O(1)
cells[cell].push_back(entity_index);

// Query: O(k), k = entities in nearby cells
for (Cell c : nearby_cells(position, radius)) {
    for (EntityIndex idx : cells[c]) {
        // Process entity
    }
}
```

**Key Insights**
- Entities typically clustered (not scattered)
- Uniform distribution (no massive clusters)
- Most entities don't change cells per frame (~99% stay put)
- Incremental updates exploit this

### Section 4: Implementation Deep Dive (15 minutes)

**Generic N-Dimensional Design**
```cpp
template<typename Entity, std::size_t Dims,
         typename FloatT = float,
         typename IndexT = std::size_t>
class sparse_spatial_hash;

// Works for 2D, 3D, 4D+
sparse_spatial_hash<Entity, 2> grid_2d;
sparse_spatial_hash<Entity, 3> grid_3d;
sparse_spatial_hash<Entity, 4> grid_4d;  // Space-time!
```

**C++20 Features**
- Concepts for compile-time constraints
- Ranges for STL compatibility
- Designated initializers for clean config

**Customization Point**
```cpp
template<>
struct position_accessor<Entity, 3> {
    static float get(const Entity& e, std::size_t dim);
};
```

**Demo**: Live coding of simple 2D particle system

### Section 5: Performance Optimizations (10 minutes)

**Optimization 1: Morton Encoding**
- Z-order curve for spatial locality
- Adjacent cells → adjacent hash values
- Better cache utilization

**Optimization 2: Division Reciprocals**
```cpp
// Slow (10-15 cycles)
float cell = position / cell_size;

// Fast (3-5 cycles)
float cell = position * cell_size_inv;  // Precomputed
```

**Optimization 3: Loop Unrolling**
```cpp
// Generic (compiler can't optimize)
for (size_t d = 0; d < Dims; ++d) { ... }

// Specialized (optimal code)
if constexpr (Dims == 3) {
    // Manually unrolled
}
```

**Results**: 10-48% performance improvement

**Demo**: Benchmark comparisons before/after

### Section 6: Real-World Performance (5 minutes)

**DigiStar Physics Engine**
- 10M particles in 10,000³ world
- 20-unit interaction radius

**Results**:
- Memory: 100MB (vs 6TB for dense grid)
- Build: 80ms (initial construction)
- Incremental update: 2ms (1% of particles move)
- Collision detection: 150ms (all pairs within radius)
- **Total**: 232ms for 10M particles (60 FPS achievable with optimizations)

**Comparison**:
| Operation | Dense Grid | R-tree | Sparse Hash |
|-----------|-----------|--------|-------------|
| Memory | 6 TB | 400 MB | 100 MB |
| Build | O(n) | O(n log n) | O(n) |
| Update | O(n) | O(m log n) | O(m), m ≈ 0.01n |

### Section 7: When to Use What (5 minutes)

**Use Sparse Hash When**:
- Large sparse worlds
- Dynamic scenes (frequent movement)
- Uniform entity distribution
- Real-time requirements

**Use R-tree When**:
- Static/semi-static data
- Non-uniform clustering
- Hierarchical bounding volumes needed

**Use Octree When**:
- LOD hierarchy needed
- Spatial clustering significant

**Use Dense Grid When**:
- Small dense worlds
- All cells likely occupied

**Use kd-tree When**:
- Static dataset
- kNN queries more important than radius

**Decision flowchart** (diagram)

### Section 8: Library Design (5 minutes)

**Boost-Quality Standards**
- Header-only
- Zero-overhead abstractions
- STL-compatible
- Exception safety guarantees
- Comprehensive testing (31 tests, 100% pass)
- Full documentation

**API Design Principles**
- Simple common cases
- Customizable for advanced uses
- Hard to misuse
- Clear error messages (concepts)

**Future**: Boost submission planned

### Section 9: Q&A (5 minutes)

Questions and discussion

## Demo Ideas

### Demo 1: Live Particle System
- Animated 2D particles
- Toggle spatial indexing on/off
- Show FPS difference
- Visualize cell grid

### Demo 2: Performance Comparison
- Benchmark runner
- Compare naive, R-tree, sparse hash
- Real-time graphs
- Memory usage visualization

### Demo 3: N-Dimensional Support
- 2D example (screen)
- 3D example (3D visualization)
- 4D example (conceptual: space + time)

### Demo 4: Incremental Updates
- Visualize which entities change cells
- Show timing: rebuild vs update
- Highlight moved entities

## Slides Outline

1. Title slide
2. Problem statement (collision detection)
3. Naive solution (animation)
4. Data structure comparison
5. Sparse hash concept
6. Theory (diagrams)
7. Implementation (code)
8. Generic design (C++20)
9. Morton encoding (visualization)
10. Performance optimizations
11. Benchmark results
12. DigiStar case study
13. When to use what (flowchart)
14. API design
15. Boost submission plans
16. Live demo
17. Q&A
18. Thank you (links)

## Submission Timeline

### For CppCon 2026

- **Call for Submissions Opens**: March 2026
- **Submission Deadline**: May 2026
- **Acceptance Notification**: June 2026
- **Conference**: September 2026

### Preparation Schedule

- **January 2026**: Finalize library, complete documentation
- **February 2026**: Prepare demos and slides
- **March 2026**: Submit proposal
- **April-May 2026**: Refine based on feedback
- **June 2026**: Notification
- **July-August 2026**: Prepare final presentation

## Resources

### CppCon Submission

- **Website**: https://cppcon.org/
- **Submission Portal**: TBA in March
- **Speaker Guidelines**: https://cppcon.org/speakerguidelines/

### Tips for Acceptance

1. **Clear problem statement**: What problem does this solve?
2. **Concrete results**: Show real benchmarks and use cases
3. **Practical value**: Attendees can use this tomorrow
4. **Technical depth**: Not just "what" but "how" and "why"
5. **Engaging demo**: Live code or animations
6. **Production-tested**: Not just theory

### Previous Similar Talks

Study these for inspiration:

- "Optimizing Hash Tables for Mobile" - Matt Kulukundis
- "Data Structures in Modern C++" - Various speakers
- "Game Engine Architecture" - Various speakers
- "Boost Library Design" - Various speakers

## Contact

Questions about CppCon submission?

- CppCon: program@cppcon.org
- Library author: [contact info]

---

**Timeline**: Submit March 2026 for CppCon 2026
**Expected Audience**: 100-300 attendees
**Session Length**: 60 minutes (50 min talk + 10 min Q&A)
