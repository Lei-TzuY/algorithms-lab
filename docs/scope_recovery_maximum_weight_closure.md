# Scope recovery: exact maximum-weight closure / project selection

## Coverage decision

This post-Phase-69 recovery slice adds a graph-optimization capability that was
absent from the live implementation and pull-request history after exact
bipartite multigraph edge coloring reached merged-main green. It deliberately
changes proof model instead of extending the recent matching/edge-coloring
surface.

The problem is maximum-weight closure on a directed implication graph. Every
vertex has a signed weight. Selecting vertex `u` requires selecting every
out-neighbor `v`; stored graph-edge weights, self-loops, and parallel copies do
not change that structural implication relation.

The stale historical compiler/backend roadmap is not prospective authority.
`docs/scope_recovery_after_phase69.md` plus the fresh live coverage audit remain
the scope boundary.

## Production contract

`maximum_weight_closure(graph, weights)` requires a directed `Graph` and exactly
one signed `int64_t` weight per vertex. It returns:

- the exact maximum closure weight;
- the corresponding deterministic selected-vertex witness in ascending order;
- the reduction's positive-weight sum and minimum-cut value as replayable
  diagnostics.

The empty set is always feasible. When the mathematical optimum is zero, the API
returns the empty set canonically rather than choosing an arbitrary zero-weight
closure.

The implementation reuses the sealed Dinic max-flow/min-cut subsystem. It does
not duplicate a flow engine.

## Reduction and proof obligation

Let `P` be the sum of all positive vertex weights. For every positive vertex
`v`, add `source -> v` with capacity `w(v)`. For a negative vertex, add
`v -> sink` with penalty `min(|w(v)|, P)`. For every distinct non-self
implication `u -> v`, add capacity `P`.

The empty closure induces a cut of capacity `P`, so every minimum cut has value
at most `P`.

If a cut has value strictly below `P`, it cannot cross an implication edge and
cannot select a negative vertex whose true magnitude is at least `P`. Therefore
its source-side original vertices form a genuine closed set and every selected
negative penalty is untruncated. For a closed selected set `S` the cut identity
is then

`cut = P - weight(S)`.

Thus every cut below `P` maps exactly to a positive-weight closure, and every
positive-weight closure maps to a cut below `P`. If the minimum cut equals `P`,
no positive-weight closure exists and the canonical empty closure is optimal.

This argument is also why exact `INT64_MIN` vertex weights are supported without
calling `abs(INT64_MIN)`: penalties beyond `P` can be capped safely.

The reduction uses the repository's signed-64 flow-capacity surface, so the sum
of all positive vertex weights must itself fit in `int64_t`. Inputs exceeding
that reduction representability bound throw `std::overflow_error`, even if a
large negative dependency could make one particular closure's final weight fit.
No arbitrary-precision closure claim is made.

## Verification

Focused candidate verification passed before upload under:

- GCC C++20 strict warnings-as-errors;
- Clang C++20 strict warnings-as-errors;
- actual GCC ASan+UBSan with leak detection.

Deterministic evidence covers:

- empty input and all-non-positive weights;
- undirected and weight-count rejection;
- forced negative dependencies, cycles, self-loops, and parallel implication
  copies with ignored stored weights;
- canonical empty result for a zero optimum;
- exact `INT64_MAX` positive weight;
- exact `INT64_MIN` negative penalty;
- a representable optimum next to the full signed-64 boundary;
- fail-closed positive-sum overflow.

Primary randomized verification uses 700 fixed-seed directed multigraphs with up
to ten vertices. The oracle enumerates every vertex subset, directly checks the
closure implication predicate, and computes the exact best weight. It does not
use flow, cuts, residual reachability, or the production reduction. Production
must match the exact optimum weight, return a replayable closed witness, and be
bit-for-bit deterministic on repeated execution.

## Complexity and non-claims

Let `V` be the number of vertices and `E_s` the number of distinct non-self
implications after structural canonicalization. Building the reduction costs
`O(E log E + V)` with the direct ordered-set canonicalization. The dominant solve
cost is the sealed Dinic implementation on `V+2` vertices and `O(V+E_s)` arcs;
this slice inherits that implementation's documented flow complexity rather than
inventing a stronger bound. Auxiliary closure-specific storage is `O(V+E_s)`.

This slice does not claim arbitrary-precision aggregate profits, a parametric
closure solver, dynamic updates, submodular minimization, precedence scheduling,
or a faster specialized closure algorithm.

## Scope

The intended recovery diff is exactly four paths:

- `include/algorithms/graphs/maximum_weight_closure.hpp`;
- `tests/test_maximum_weight_closure_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this proof/coverage document.

No CMake, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or unrelated recovery surface is modified.
