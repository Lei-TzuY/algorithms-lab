# Scope recovery: exact offline 3D dominance counting

## Coverage decision

The post-Phase-69 recovery stream deliberately changes proof model after the
ranked-zeta subset-convolution checkpoint. Live code and branch searches found no
offline multidimensional dominance counter or CDQ divide-and-conquer capability;
existing compiler dominance-frontier code is a different control-flow concept.
The occupied minimum-cycle-basis branch is left untouched.

## Contract

For each signed-64-bit point `p_i = (x_i,y_i,z_i)`, production returns the exact
number of input copies `p_j` satisfying all three coordinate-wise inequalities
`x_j <= x_i`, `y_j <= y_i`, and `z_j <= z_i`. Counts include the queried point
itself and every exact duplicate. Input order is semantically irrelevant, and no
coordinate arithmetic is required, so `INT64_MIN` / `INT64_MAX` coordinates are
valid.

## Algorithm and proof obligations

1. Sort by `(x,y,z)` and collapse exact duplicates into weighted groups. This is
   essential: all copies of one point must receive the same answer.
2. In lexicographic x-order, any distinct equal-x point that can dominate a target
   appears no later than that target because its `(y,z)` pair is also no greater.
   Thus the ordinary left-to-right CDQ partial order remains valid even when x
   values tie.
3. CDQ recursively solves both halves. During each merge, left-half groups are
   streamed by nondecreasing y into the sealed Phase-4 `FenwickTree`, keyed by
   compressed z. An inclusive prefix query then adds exactly the left-half groups
   satisfying both remaining inequalities to each right-half target.
4. Every temporary Fenwick update is rolled back before the recursion returns.
   The merge leaves the range ordered by `(y,z,x)` for its parent without changing
   membership of the already-fixed CDQ halves.
5. Each group starts with its own multiplicity, so after all cross-half
   contributions its stored count is exactly the weighted size of its closed
   dominance orthant. That count is copied back to every original duplicate.

The result is exact; there is no probabilistic or floating-point claim.

## Complexity

Sorting/grouping/compression costs `O(n log n)`. There are `O(log n)` CDQ levels;
each level performs `O(n)` Fenwick updates/queries at `O(log n)` each plus linear
merge work, for `O(n log^2 n)` time and `O(n)` auxiliary/result storage.

## Verification

Deterministic tests cover empty/singleton inputs, heavy duplicates, incomparable
points, equal-x boundaries, monotone chains, input permutation, and full signed
coordinate extremes. A fixed-seed randomized corpus of 900 instances (0–90
points, duplicate-heavy coordinates) is compared point-for-point against an
independent quadratic coordinate-wise scan.

This slice does not claim dynamic dominance queries, reporting all dominated
points, higher-dimensional CDQ, range trees, fractional cascading, or compiler
control-flow dominance functionality.
