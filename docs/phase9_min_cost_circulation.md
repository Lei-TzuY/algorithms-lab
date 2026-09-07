# Phase 9 — lower-bounded demand min-cost circulation

This slice closes the ordered Phase-9 implementation roadmap with a bounded circulation problem rather than another source-to-sink flow variant.

## Contract

Each directed edge carries `(lower, upper, cost)` with `0 <= lower <= upper`. `demand[v]` means required net inflow minus net outflow at vertex `v`; all demands must sum to zero. A feasible witness must satisfy every edge bound and every vertex demand exactly. Infeasibility is represented by `std::nullopt`, not by weakening a balance equation.

`INT64_MIN` edge cost is rejected because a reverse residual arc would require the unrepresentable negation `-INT64_MIN`. Other capacity, balance, Bellman-Ford, reduced-cost, and total-cost arithmetic is checked and fails closed with `std::overflow_error` when an intermediate is not representable.

## Feasibility transformation

The solver initially assigns every edge its lower bound. For non-self-loop edge `u -> v` with lower bound `l`, that contributes `-l` net inflow at `u` and `+l` at `v`; self-loops do not change a vertex balance.

The remaining balance is routed through capacity `upper-lower` on every original edge. A super-source connects vertices that need net outflow, and vertices that need net inflow connect to a super-sink. Phase-6 `dinic_max_flow` must saturate the entire required auxiliary flow. The first `E` flow witnesses returned by Dinic map directly back to residual flow above each lower bound.

This transformation proves feasibility only; it does not claim minimum cost.

## Residual cycle optimality

For a current feasible circulation, every original edge produces up to two residual arcs:

- forward capacity `upper-flow` with cost `c`;
- reverse capacity `flow-lower` with cost `-c`.

Augmenting any residual cycle preserves every vertex demand and all edge bounds. If the cycle has negative total cost, sending its bottleneck residual capacity strictly improves the feasible circulation. The implementation repeatedly uses all-vertex Bellman-Ford to find such a cycle, reconstructs it through predecessor arcs, and applies the bottleneck augmentation.

When Bellman-Ford can no longer relax an arc, its distance labels form a residual potential `p` satisfying

`c(u,v) + p(u) - p(v) >= 0`

for every positive-residual arc. Thus no negative residual cycle remains, which is the standard optimality condition for a feasible minimum-cost circulation.

## Complexity boundary

Feasibility uses the existing Dinic implementation. If `K` negative cycles are cancelled, the optimization phase costs `O(KVE)` time and `O(V+E)` working residual state, aside from feasibility storage. Because `K` depends on integral capacities/cost improvements, this cycle-cancelling formulation is pseudo-polynomial; the repository deliberately does not relabel it as strongly polynomial.

## Verification

Deterministic tests cover lower-bound balance adjustment, non-zero demands, infeasible demand routing, negative two-edge cycles, negative self-loops, malformed bounds/endpoints, demand-shape validation, exact total cost `INT64_MIN`, and overflow rejection.

The randomized differential suite runs 300 fixed-seed instances with 1–4 vertices and at most six edges. Bounds are tiny enough that the independent oracle can enumerate every edge-flow assignment, check the demand equations directly, and select the exact minimum cost. Approximately half the random demands are derived from a sampled feasible bounded flow; the rest are arbitrary zero-sum demands so infeasibility is exercised naturally.

Test-side certificate replay independently checks edge bounds, demand balances, total cost, and the forward/reverse reduced-cost inequalities implied by the returned residual potential. The oracle shares none of the production feasibility or cycle-cancelling machinery.
