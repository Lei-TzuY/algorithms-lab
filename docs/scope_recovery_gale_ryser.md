# Scope recovery: Gale-Ryser bipartite degree sequences

## Coverage decision

A fresh live-code, pull-request-history, and branch audit at
`main@1c2bc1456fdb0335385df6737f424bc581a0140c` found no Gale-Ryser,
bipartite graphical-degree-sequence decision procedure, or equivalent
constructive realization surface. The prospective authority remains
`docs/scope_recovery_after_phase69.md`; the frozen Phase-45--69 compiler/backend
history and stale historical ROADMAP frontier are untouched.

The immediately preceding recovery checkpoints are bounded-exact Pell equations
and dense unrestarted GMRES. This slice deliberately changes proof model again:
majorization of two partition degree sequences plus a constructive bipartite
Havel-Hakimi reduction, rather than another numerical or continued-fraction
variant.

## Production contract

`gale_ryser_bipartite_graphical(left, right)` decides whether two labelled
non-negative degree vectors are realized by a **simple bipartite graph** with the
specified left/right partition sizes.

- every left degree must be at most `|right|` and every right degree at most
  `|left|`; malformed domain values throw `std::invalid_argument`;
- empty partitions and zero-degree isolated vertices are valid;
- feasibility is invariant under permutations within either partition;
- arithmetic sums are checked in `uint64_t` and fail closed on an unrepresentable
  host-size accumulation.

`bipartite_havel_hakimi_realization(left, right)` returns either `nullopt` or a
replayable simple-edge witness `(left_id,right_id)` over the original labelled
vertices. The constructive order is deterministic: left vertices by requested
degree descending then id ascending; right residuals by residual degree
descending then id ascending. Returned edges are strictly sorted and duplicate
free.

`valid_bipartite_degree_realization` independently replays endpoint bounds,
strict edge uniqueness/order, and both requested degree vectors.

## Correctness boundary

Let sorted left degrees be `r_1 >= ... >= r_m` and sorted right degrees be
`c_1 >= ... >= c_n` with equal total sum. Gale-Ryser states that a simple
bipartite realization exists exactly when, for every `1 <= k <= m`,

`sum_{i=1..k} r_i <= sum_{j=1..n} min(k,c_j)`.

The decision implementation evaluates those inequalities directly.

Construction uses the bipartite Havel-Hakimi reduction theorem: choosing a left
vertex of maximum residual degree `d` and joining it to the `d` right vertices
of largest residual degree preserves realizability. Repeating the reduction
therefore yields a witness exactly when the residual sequence remains feasible.
These majorization/reduction theorems are mathematical proof obligations;
finite testing is implementation evidence, not their proof.

## Verification

Primary verification is independent of both production formulations. For every
partition shape through `3 x 3`, tests enumerate **every labelled simple
bipartite graph**, collect its exact pair of degree vectors, then enumerate every
well-formed degree-vector pair and require:

- Gale-Ryser decision equals exhaustive realizability;
- the constructive routine has a value exactly for realizable pairs;
- every returned edge witness replays both degree vectors exactly.

Additional deterministic evidence covers empty-side semantics, known feasible
and infeasible majorization examples, unequal totals, malformed degree bounds,
strict witness validation, deterministic replay, and duplicate-edge rejection.
A 1,200-case fixed-seed corpus derives degree sequences from independent random
bipartite graphs through 20-by-20 vertices, permutes labels within both sides,
and replays every returned construction. Perturbing one legal degree without
balancing the opposite side provides an additional infeasibility check.

## Complexity / non-claims

The direct Gale-Ryser baseline sorts both sides and scans the full right vector
for every left prefix: `O(L log L + R log R + L*R)` time and `O(L+R)` working
storage. The direct constructive baseline re-sorts right residuals for each left
vertex: `O(L*R log R + L log L + E)` time and `O(L+R+E)` result/working storage.

No linear-time majorization optimization, bipartite multigraph realization,
connected realization, forbidden-edge / prescribed-edge constraints, counting
or sampling of realizations, canonical graph up to isomorphism, or benchmark
speedup is claimed.
