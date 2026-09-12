# Scope recovery — exact bounded treewidth by elimination subset DP

## Coverage decision

A fresh post-Phase-69 live-state audit at `main@feb1ff52df8c43b2403478c516b81ebca076aea4` found no treewidth API, implementation, branch, pull request, or historical commit. The most recent recovery work covered Freivalds verification, rollback-parity bipartiteness, Stable Roommates rotations, CDQ dominance, subset convolution, deterministic streaming quantiles, linear recurrences, perfect hashing, finite-field square roots, SMAWK, discrete logarithms, Segment Tree Beats, Hamiltonian subset DP, and chordal recognition. Exact treewidth adds a different proof model: fill-in under vertex elimination plus a min-max subset dynamic program.

This slice follows `docs/scope_recovery_after_phase69.md`. The frozen Phase-45–69 compiler/backend sequence and stale historical ROADMAP frontier remain untouched.

## Public contract

`exact_treewidth(graph)` accepts only an undirected graph with at most 20 vertices. It projects the repository multigraph to the underlying simple graph: self-loops are ignored, parallel copies collapse, and edge weights do not affect treewidth. Directed input is rejected and larger instances fail explicitly rather than pretending the exponential baseline scales indefinitely.

The result contains:

- the exact treewidth;
- the lexicographically smallest optimal elimination order under ascending vertex ids;
- a replayable bag for every elimination step, consisting of the eliminated vertex plus its then-current filled neighbors.

## Torso invariant and recurrence

For an eliminated set `S`, two remaining vertices are adjacent in the filled elimination graph exactly when the original simple graph contains a path between them whose internal vertices all lie in `S`. This is the torso of `S`; crucially, it depends only on the set `S`, not on the order used to eliminate vertices of `S`.

Let `deg_S(v)` be the degree of a remaining vertex `v` in that torso. The exact suffix recurrence is

`TW(S) = min_{v notin S} max(deg_S(v), TW(S union {v}))`,

with `TW(V)=0`. After the optimum width `W=TW(empty)` is known, reconstruction scans candidates in ascending vertex order and chooses the first `v` with both `deg_S(v) <= W` and `TW(S union {v}) <= W`. This preserves the global optimum while producing the lexicographically smallest optimal elimination order even after an earlier prefix has already attained width `W`. The returned bag at a state is `{v}` plus the torso neighbors used by the same recurrence.

The direct bounded baseline computes a torso neighborhood by traversing only eliminated vertices reachable from the candidate and collecting their remaining boundary neighbors. With `n <= 20`, the conservative bound is `O(n^2 2^n)` time and `O(2^n + n)` auxiliary DP state, excluding the returned bags. This is an exact exponential algorithm, not an FPT, heuristic, or large-instance treewidth claim.

## Independent verification

Production is not compared against another subset-DP recurrence. Test code keeps an explicit mutable simple graph, replays an elimination order, fills every pair of current neighbors into a clique, deletes the chosen vertex, and records the resulting width and bags. For graphs up to seven vertices it enumerates every vertex permutation to obtain the exact optimum and lexicographically first optimal order.

Deterministic cases cover empty/singleton graphs, trees, cliques, a cycle, fill-producing shapes, self-loops, parallel weighted copies, directed rejection, and the explicit vertex bound. A fixed-seed randomized corpus of 160 multigraphs independently compares production against exhaustive permutation search and replays every returned bag.

No test timing is used as a performance claim, and exhaustive small-instance agreement is evidence rather than a substitute for the elimination/torso proof obligation.

## Scope

Exactly four files differ from main:

- `include/algorithms/graphs/treewidth.hpp`;
- `tests/test_treewidth.cpp`;
- one test registration line in `CMakeLists.txt`;
- this recovery proof document.

No README, ROADMAP, workflow, benchmark, frozen compiler/backend, or occupied recovery-surface file changes.
