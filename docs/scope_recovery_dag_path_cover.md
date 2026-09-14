# Scope recovery — exact original-arc DAG path cover

## Fresh coverage decision

This recovery slice starts only after exact maximum-weight closure reached merged `main@28c271c89ed717b02fb4410c925f88b62c5d0391` and push CI run `34791412934` completed successfully on GCC release, Clang release, and GCC ASan+UBSan.

A fresh live audit found no `minimum_dag_path_cover` / original-edge DAG path-cover implementation, no open implementation PR, and no `dag` / `path-cover` recovery branch. The closest sealed result is Dilworth decomposition (PR #213), whose proof record explicitly distinguishes reachability-poset chain cover from original-edge-only DAG path cover. Its dedicated regression `0->2, 1->2, 2->3, 2->4` has Dilworth width two but original-arc path-cover size three. This slice closes that documented non-claim instead of extending the recent maximum-weight-closure or matching/coloring streak.

Prospective scope remains governed by `docs/scope_recovery_after_phase69.md`; the frozen Phase-45–69 compiler/backend frontier and historical ROADMAP are untouched.

## Production contract

`minimum_dag_path_cover(graph)` computes an exact minimum-cardinality vertex-disjoint path cover of a directed acyclic `Graph` using **only original directed arcs**.

- undirected input is rejected;
- cyclic input, including a self-loop, is rejected;
- every vertex appears in exactly one returned path;
- stored edge weights do not affect the cardinality objective;
- parallel `u->v` copies collapse to one matching relation, while the first insertion-order copy is retained as the replay witness;
- returned paths are ordered by ascending start vertex and are deterministic for fixed adjacency order;
- empty input returns zero paths; isolated vertices return singleton paths;
- result exposes path count, matching cardinality, vertex paths, and exact `(from, adjacency_index, to, weight)` edge witnesses.

No transitive/reachability edge may be introduced merely because one vertex can reach another.

## Reduction and proof obligation

Create left and right copies of every DAG vertex and one bipartite edge `u_L -> v_R` for each distinct original arc `u -> v`. Reuse the sealed Hopcroft-Karp implementation for a maximum matching `M`.

Any vertex-disjoint path cover with `p` paths uses exactly `V-p` path arcs. Their tails and heads are unique, so those original arcs form a bipartite matching. Therefore `|M| >= V-p`, or `p >= V-|M|`.

Conversely, a matching gives every vertex at most one selected successor and at most one selected predecessor. Because every selected pair is an original DAG arc, those links cannot contain a directed cycle. They therefore decompose all vertices into vertex-disjoint directed paths with exactly `V-|M|` starts. Hence the returned cover is optimal and the exact optimum is `V-|M|`.

The wrapper fail-closes if the sealed matching result has a malformed shape, non-reciprocal left/right mates, a cardinality inconsistent with its mate arrays, or a matched endpoint pair absent from the original DAG.

## Independent verification

Focused pre-upload checks passed under strict GCC C++20, strict Clang C++20, and actual GCC ASan+UBSan. The focused harness used API-compatible test doubles for the sealed matching/topological dependencies; the repository's exact-head CI is the authoritative integration gate against the real implementations.

Committed deterministic evidence covers:
- the PR #213 gap graph where original-arc path cover is three while Dilworth width is two;
- a three-vertex chain;
- parallel arcs with differing signed weights, including exact first-insertion witness replay;
- empty and isolated graphs;
- directed-cycle, self-loop, and undirected rejection;
- repeated deterministic execution.

The primary randomized optimum oracle does **not** call matching. For 700 fixed-seed DAG multigraphs with 0..9 vertices and at most 12 distinct original endpoint pairs, it enumerates every subset of unique original arcs and keeps exactly those subsets with at most one chosen outgoing and one chosen incoming arc per vertex. Since all generated inputs are DAGs, the maximum such link count is exact; the minimum path count is `V-max_links`. Production must match both values exactly. Every returned vertex partition and original adjacency witness is replayed.

## Complexity and non-claims

Let `R <= E` be the number of distinct original ordered endpoint pairs. Canonicalizing parallel arcs through the direct ordered-map baseline costs `O(E log(E+1))`. DAG validation is `O(V+E)`. Sealed Hopcroft-Karp contributes `O(R sqrt(V+1))`, and reconstruction is `O(V+R)`. The direct combined bound is therefore

`O(V + E log(E+1) + R sqrt(V+1))`

with `O(V+R)` additional relation/matching/result state beyond the caller-owned graph (plus the sealed matching implementation's documented state). Recursive DFS inside the sealed Hopcroft-Karp may use `O(V)` process stack.

This slice does not claim weighted/min-cost path cover, transitive-closure chain cover, cyclic-graph path cover, dynamic updates, canonical choice among all optimum covers, or a specialized asymptotically faster DAG-matching engine.

## Scope

Exactly four paths change:
- `include/algorithms/graphs/dag_path_cover.hpp`
- `tests/test_dag_path_cover_cases.hpp`
- one include line in `tests/test_main.cpp`
- `docs/scope_recovery_dag_path_cover.md`

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark, frozen compiler/backend, or temporary-file surface is changed.
