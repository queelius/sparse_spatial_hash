# Review Fixes Implementation Plan

> **For agentic workers:** Use superpowers:executing-plans to implement this plan. The user explicitly requested implementation by the current agent.

**Goal:** Resolve all five verified findings from the review of dc16ac1.

**Architecture:** Transactional rebucketing, allocation-free unique axis intervals,
and a linear seen-index validation pass. Keep test allocation instrumentation in
a separate executable so it cannot affect the normal test runner.

**Tech Stack:** Header-only C++20, Catch2 v3, CMake, gcov/lcov, GCC UBSan.

**Spec:** `docs/superpowers/specs/2026-10-25-review-fixes-design.md`

## Global Constraints

- Keep the library header-only, C++20, generic in dimension, and stdlib-only.
- Keep allocation failure visible to callers while preserving grid membership.
- Leave the user's original checkout untouched until the fixes are verified;
  then apply the complete patch there if it still has no independent changes.
- Leave the resulting changes uncommitted for user inspection.

## Review Focus

- Destination insertion can rehash; old-cell iterators must remain safe.
- Heap growth of an existing destination cell can fail independently of map allocation.
- Wrapped neighborhoods can cover one axis completely and another only partially.
- Negative and extreme representable cell coordinates must not overflow interval arithmetic.
- Diagnostic allocation failures must propagate while leaving grid state unchanged.

### Task 1: Transactional moves and allocation instrumentation

**Files:** `test/test_allocation_safety.cpp`, `test/CMakeLists.txt`,
`include/spatial/sparse_spatial_hash.hpp`.

**Interfaces:** Preserve `update_one(IndexT, const EntityT&)` and `update()`.

- [x] Add a separate allocation-instrumented Catch2 executable. For every
  allocation failure while moving to a new destination, and while overflowing
  an existing destination's inline storage, require identical membership,
  entity/cell counts, and successful validation. Cover both update entrypoints.
- [x] Run allocation regression tests; expect failures on the original helper.
- [x] Prepare destination insertion and growth before committing removal; erase
  a newly created destination on preparation failure.
- [x] Run allocation tests and the complete suite; expect all to pass.

### Task 2: Unique allocation-free neighborhoods

**Files:** `test/test_neighborhoods.cpp`, `test/test_allocation_safety.cpp`,
`test/CMakeLists.txt`, `include/spatial/sparse_spatial_hash.hpp`.

**Interfaces:** Preserve radius query/callback/pair signatures; private iteration
may consume computed per-axis intervals instead of offsets.

- [x] Add literal query and pair expectations at bounded corners, toroidal
  wraps, and partial-axis wraps in 2D/3D/4D. Require zero callback-query/pair
  allocations, including empty grids. Add a one-cell torus with radius 1.1e9f.
- [x] Run bounded and allocation tests and the large-radius test under UBSan;
  expect duplicates, allocation counts, and signed overflow respectively.
- [x] Enumerate distinct axis intervals with wide arithmetic; remove the
  visited set and clip infinite intervals to representable cell coordinates.
- [x] Run neighborhood regressions and the complete suite; expect all to pass.

### Task 3: Linear diagnostics and final verification

**Files:** `test/test_validation.cpp`, `test/test_allocation_safety.cpp`,
`test/CMakeLists.txt`, `include/spatial/sparse_spatial_hash.hpp`,
`docs/api-reference/index.md`.

**Interfaces:** `bool validate() const` may allocate and throw; all successful
validity results and the grid's strong guarantee are preserved.

- [x] Count comparisons for 2,048 entities in one cell; require at most 4n.
  Add missing, duplicate, wrong-cell, and out-of-range index diagnostics;
  assert diagnostic allocation failures propagate without changing membership.
- [x] Run new regressions; expect dense-cell and diagnostic-allocation failures.
- [x] Traverse cells once with allocator-backed seen-index bytes, checking
  bounds, duplicates, reverse mapping, and complete coverage. Document costs.
- [x] Run all tests, examples, coverage, and targeted UBSan regressions.
- [x] Obtain one independent review, address substantive findings with tests,
  and apply the verified patch to the unchanged original checkout.
