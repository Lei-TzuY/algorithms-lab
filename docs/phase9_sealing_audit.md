# Phase 9 sealing audit

Phase 9 is sealed only after all three ordered weighted-optimization slices passed exact candidate CI, clean merge gates, and merged-main GCC, Clang, and ASan+UBSan runs.

## Integrated capability

The sealed phase contains three distinct optimization models rather than repeated flow variants:

1. `min_cost_max_flow` maximizes source-to-sink flow first and minimizes signed cost at that fixed maximum value.
2. `hungarian_min_assignment` solves complete rectangular injective assignment directly and cross-checks its objective against a flow reduction.
3. `min_cost_circulation` handles lower/upper bounds plus per-vertex demands, separates feasibility from cost optimization, and permits negative residual cycles during improvement.

The first and third slices deliberately have different supported domains. Minimum-cost maximum flow uses feasible potentials for successive shortest augmenting paths and rejects positive-capacity negative-cost input cycles. General circulation cannot use that restriction because a negative residual cycle may be the optimization mechanism itself; it therefore uses cycle cancellation and certifies termination with a feasible residual potential. The audit treats this difference as an explicit semantic boundary, not an inconsistency.

## Cross-phase and cross-slice integration

- Minimum-cost maximum flow reuses the Phase-1 first-principles binary heap for reduced-cost Dijkstra.
- Hungarian assignment is independently implemented, while randomized tests reduce the same instances to minimum-cost max flow for cross-implementation objective equality.
- Lower-bounded circulation reuses Phase-6 Dinic for the feasibility transformation instead of embedding a second private max-flow solver.
- The circulation optimizer is separate from the MCMF implementation so the stronger general negative-cycle domain does not silently weaken the earlier solver's invariant.

## Correctness evidence

Each production recurrence has a structurally independent primary oracle:

- 300 small min-cost-max-flow networks use exhaustive edge-flow assignment enumeration.
- 500 rectangular assignment matrices use exhaustive injective assignment enumeration.
- 300 bounded-demand circulation instances enumerate every bounded edge-flow assignment directly.

Returned witnesses are replayed independently for capacity/bound constraints, conservation or demand equations, objective totals, and the relevant residual dual/potential inequalities. Deterministic tests cover rerouting, negative cycles, infeasibility, malformed inputs, signed-cost extremes, exact `INT64_MIN` total-cost boundaries, and overflow rejection.

## Complexity and claim audit

- Min-cost maximum flow claims `O(VE + A E log V)` for `A` augmentations under its binary-heap implementation.
- Hungarian assignment claims `O(R^2 C)` for `R <= C`.
- Cycle-cancelling circulation explicitly claims `O(KVE)` after feasibility for `K` cancelled cycles and is labeled pseudo-polynomial.

No CI timing is presented as a benchmark, no stronger polynomial claim is made for cycle cancellation, and no test corpus is presented as proof of the underlying optimization theorems.

## Bounded debt

The phase intentionally does not add cost-scaling flow, auction assignment, weighted blossom, or additional flow formulations merely to increase algorithm count. Shared checked-arithmetic helpers remain local to each optimization implementation; deduplicating them would be a maintainability refactor without new executable capability and is not a seal blocker.

## Promotion decision

A substantial uncovered frontier remains: maximum-cardinality matching in **general undirected graphs**. The repository already has sealed Hopcroft-Karp for bipartite graphs, but odd cycles require a qualitatively different contraction/augmentation model. Phase 10 is therefore promoted narrowly to general graph matching, beginning with Edmonds blossom.

Acceptance for that first slice must include odd-cycle contraction regressions, matching-witness validation, exhaustive small-graph optimum comparison, and equality with the sealed Hopcroft-Karp solver on bipartite instances. Weighted blossom or unrelated matching variants are not precommitted by this promotion.
