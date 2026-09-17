# Scope recovery: divide-and-conquer optimized partition DP

## Coverage decision

A fresh live-state audit at `main@0a9300e66cca7ffa670629992228d381771d6938`
found zero open pull requests and zero open issues, with exact main CI run
`35175292655` completed successfully. The post-Phase-69 authority remains
`docs/scope_recovery_after_phase69.md`; the stale historical backend ROADMAP is
not an active frontier.

The immediately preceding recovered slice added reusable functional-graph
successor decomposition/indexing. Recovery deliberately changes proof model
again rather than extending that surface with jump aggregates or dynamic
successor updates.

Default-branch code search found no divide-and-conquer DP optimization / monotone
opt implementation. A Knuth-optimization surface is currently occupied by the
separate `scope-recovery-optimal-bst-knuth` branch and was therefore left alone.
A min-cut cactus surface is likewise occupied. This slice instead adds the
independent divide-and-conquer optimization pattern on a concrete Monge-cost
partition problem.

## Production contract

`minimum_squared_sum_partition(weights, groups)` receives non-negative integer
weights and partitions them into exactly `groups` non-empty contiguous segments.
It returns:

- the exact minimum of the sum of squared segment sums; and
- deterministic half-open segment boundaries `[0 = b0 < b1 < ... < bg = n]`.

Tie-breaking chooses the lexicographically smallest boundary sequence through
the leftmost optimal split at every DP state.

The empty sequence is valid only with zero groups and returns cost zero with the
single boundary `{0}`. For non-empty input, `groups` must lie in `[1,n]`.

The exact arithmetic domain is explicit: `sum(weights) <= 2^32-1`. Inputs beyond
that bound throw `std::overflow_error`. Under this precondition every segment
sum is at most `2^32-1`, and every feasible partition cost is at most the square
of the total sum, so all production arithmetic is exactly representable in
`uint64_t`.

## DP recurrence and optimization invariant

Let `S[i]` be the prefix sum through the first `i` weights and define

`C(j,i) = (S[i] - S[j])^2`.

For exactly `g` groups covering the first `i` items,

`DP[g][i] = min_{g-1 <= j < i} DP[g-1][j] + C(j,i)`.

For `a <= b <= c <= d`, non-negative weights make prefix sums monotone and

`C(a,c) + C(b,d) <= C(a,d) + C(b,c)`, because the right-minus-left difference is

`2 * (S[b]-S[a]) * (S[d]-S[c]) >= 0`.

Thus this interval cost is Monge. The classical monotone-minima consequence for
this partition recurrence gives nondecreasing leftmost optimal split positions:

`opt[g][i] <= opt[g][i+1]`.

Production uses that invariant to solve each DP layer recursively: compute the
middle endpoint, search only the inherited optimal-split interval, then recurse
left/right with the winning split as the corresponding bound. Parent splits are
stored for exact reconstruction.

The Monge / monotone-opt theorem is a proof obligation. Finite testing validates
the implementation but does not prove that theorem universally.

## Independent verification

Focused final candidate bytes passed before upload under:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with leak detection and fail-fast behavior: 4/4.

Deterministic tests cover empty/singleton semantics, invalid group counts,
all-zero tie-breaking, all-singleton groups, a hand-checked nontrivial optimum,
the exact arithmetic upper boundary, and rejection immediately above it.

The primary small-instance oracle does **not** use the DP recurrence. For every
weight sequence of length `1..7` over `{0,1,2}` and every legal group count, it
enumerates every legal set of cut positions, evaluates the complete partition,
and chooses the exact lexicographically earliest optimum. Production must match
both cost and reconstructed boundaries.

A second larger oracle implements the full `O(k n^2)` DP transition without
divide-and-conquer pruning. Across 400 fixed-seed random instances with up to 48
items, production must match its exact cost and boundaries. The quadratic oracle
also explicitly checks that its leftmost optimum positions are nondecreasing in
each DP layer, independently exercising the optimization precondition on the
sampled inputs.

## Complexity and non-claims

With `n` items and `k` groups, each divide-and-conquer DP layer has a conservative
`O(n log n)` transition-search bound, giving `O(k n log n)` time. Production
stores two cost rows plus the `k x n` parent table needed for reconstruction, for
`O(k n)` storage; recursive search depth is `O(log n)` per layer.

This slice does **not** claim Knuth optimization, generic Monge-matrix search,
SMAWK, arbitrary signed weights, arbitrary cost callbacks, automatic proof of
quadrangle inequalities, or support beyond the documented exact arithmetic
domain. The separate occupied Knuth branch is intentionally not modified.

## Scope

Exactly three new paths are added:

- `include/algorithms/dynamic_programming/divide_conquer_partition.hpp`;
- `tests/test_divide_conquer_dp_cases.hpp`;
- `docs/scope_recovery_divide_conquer_dp.md`.

The merged verification architecture auto-enrolls `tests/test_*_cases.hpp` in
isolated translation units, so no CMake or `tests/test_main.cpp` edit is required.
No README, historical ROADMAP, recovery-authority, workflow, benchmark, frozen
compiler/backend, occupied recovery surface, or temporary-file churn is included.
