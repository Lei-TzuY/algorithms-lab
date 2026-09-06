# Phase 3 — dynamic programming

Phase 3 begins by making the DP state itself an explicit, testable abstraction. Knapsack is the first slice because 0/1 and unbounded variants differ by one crucial dependency: whether an inclusion transition reads the previous item row or the current row.

## Shared problem model

An item has a non-negative integer `weight` (`std::size_t`) and a signed 64-bit `value`. Capacity is an integer bound. The empty selection is always feasible, so an all-negative instance has optimum value zero.

Both implementations use a full `(n + 1) x (capacity + 1)` table. A one-dimensional optimization exists, but the full table is intentional here: it keeps the recurrence, proof obligation, and exact solution reconstruction visible. Complexity is therefore `O(n * capacity)` time and `O(n * capacity)` storage. This is **pseudo-polynomial** in the numeric capacity.

Value addition is checked. If a mathematically relevant candidate cannot be represented in signed 64-bit storage, the solver throws `std::overflow_error` instead of invoking signed-overflow undefined behavior. DP table dimension arithmetic is also checked before allocation.

## 0/1 knapsack

**Precondition.** Each input item may be used at most once. Zero-weight items are valid because multiplicity is bounded by the one-use rule.

Define:

`dp[i][c] = maximum representable total value achievable using only items [0, i) with total weight <= c`.

The base row is zero. For item `i - 1`:

- exclude: `dp[i - 1][c]`
- include, when the weight fits: `dp[i - 1][c - weight] + value`

The inclusion transition reads the **previous row**, so the current item cannot be reused.

**Invariant.** After row `i` is complete, every feasible subset of the first `i` items is represented by exactly one of the two cases above: it excludes item `i - 1`, or includes it exactly once and leaves a feasible subset of the first `i - 1` items. Taking the better case therefore preserves optimality.

**Reconstruction.** Walk rows backward from `(n, capacity)`. If the value equals the cell directly above, deterministic tie-breaking excludes the current item. Otherwise that item was selected; record its index and subtract its weight. Returned indices are normalized to input order.

## Unbounded knapsack

**Precondition.** Every item type must have strictly positive weight. Zero-weight item types are rejected: a positive-value one makes the mathematical optimum unbounded, while even a non-positive zero-weight type destroys the strictly decreasing-capacity measure used by the recurrence/reconstruction.

The state meaning is similar:

`dp[i][c] = maximum representable total value achievable using the first i item types, with unlimited copies, at total weight <= c`.

For type `i - 1`:

- exclude: `dp[i - 1][c]`
- include, when it fits: `dp[i][c - weight] + value`

The inclusion transition stays on the **current row**. Since weight is strictly positive, `c - weight < c`; cells are evaluated left-to-right, so the dependency is already solved and repeated copies are permitted without a cycle.

**Invariant.** Every feasible unbounded solution either uses zero copies of the current type (exclude case) or uses at least one copy. Removing one copy from the latter leaves an optimal subproblem on the same set of item types at smaller capacity, exactly the current-row transition.

**Reconstruction.** If a cell equals the previous row, exclude that type and move upward. Otherwise consume one copy, decrement capacity by its positive weight, and stay on the same row. The positive-weight precondition guarantees termination.

## Verification evidence

Deterministic tests cover classic instances, empty input, zero capacity, negative values, zero-weight 0/1 items, deterministic ties, unbounded zero-weight rejection, repeated unbounded choices, and value overflow.

Randomized evidence uses two different references:

- 0/1: small fixed-seed instances are checked against exhaustive subset enumeration.
- unbounded: small fixed-seed instances are checked against an independent memoized recursion indexed only by remaining capacity, not the production two-dimensional item-prefix table.

Every reconstructed result is also validated independently: indices/counts are in range, 0/1 indices are unique, total weight respects capacity, and recomputed value/weight match the returned optimum.

## Frontier

This document records an **in-progress** phase, not a completeness claim. The next ordered DP slice is longest increasing subsequence, where the lab can compare the textbook quadratic recurrence against the `O(n log n)` tails formulation while preserving correctness evidence and sequence reconstruction.
