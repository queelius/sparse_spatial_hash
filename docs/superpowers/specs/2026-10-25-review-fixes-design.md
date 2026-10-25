# Review fixes

The user authorized implementation of the five findings from the review of
`dc16ac1`: transactional single-entity moves, unique bounded neighborhoods,
allocation-free toroidal neighborhoods, linear validation, and overflow-safe
toroidal radius handling.

Keep the library header-only, C++20, generic in dimension, and stdlib-only.
Keep allocation failure visible to callers while preserving grid membership.
Do not broaden this change to the previously identified coordinate conversion
or unsupported array-entity overload issues.

## Approach

- Prepare destination membership before removing source membership. Clean up a
  newly inserted destination if preparation fails, and reacquire map iterators
  after any possible rehash.
- Enumerate the Cartesian product of unique per-axis coordinate intervals.
  Clip bounded intervals; limit toroidal intervals to one period; use wide
  arithmetic before converting valid cell coordinates to `int`.
- Validate in one traversal with one temporary byte per tracked entity, using
  the configured allocator. Reject missing, duplicate, out-of-range, or
  incorrectly bucketed indices. This makes validation O(n + occupied cells)
  with O(n) extra memory. Remove its `noexcept` declaration and document the
  strong guarantee and possible allocation failure.

The alternative of retaining a visited hash set would violate the allocation
contract. Retaining repeated bucket searches would retain quadratic validation;
catching allocation failures and returning false would confuse resource failure
with invalid grid state. These are implementation decisions, not additional
requirements attributed to the user.

## Verification

Add deterministic allocation-failure and allocation-count tests in a separate
test executable; exercise unique query and pair results in 2D, 3D, and 4D across
all topologies. Count index comparisons for dense-cell validation. Include
missing, duplicated, wrong-cell, and out-of-range tracking cases. Run CTest,
all four examples, header coverage, and UBSan on the large-radius regression.
