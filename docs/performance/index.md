# Performance Guide

Everything you need to know about optimizing spatial queries and updates.

## Contents

- **[Benchmarks](benchmarks.md)** - Performance measurements and comparisons
- **[Optimizations](optimizations.md)** - Technical optimization details
- **[Best Practices](best-practices.md)** - How to get maximum performance
- **[Comparison](comparison.md)** - vs R-tree, octree, dense grid, kd-tree

## Performance Highlights

### Build Performance
- **14.7M entities/second** build throughput
- O(n) complexity
- Linear scaling with entity count

### Update Performance
- **29.2M entities/second** incremental update throughput
- **40x faster** than full rebuild (when 1% of entities move)
- O(k) complexity where k = entities that changed cells

### Memory Efficiency
- **60,000x** less memory than dense grid
- Sparse storage: only occupied cells use memory
- 100MB for 10M particles in 10000³ world

[View Detailed Benchmarks](benchmarks.md){ .md-button .md-button--primary }
