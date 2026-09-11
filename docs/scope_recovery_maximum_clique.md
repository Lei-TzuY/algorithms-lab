# Scope recovery: exact maximum clique

## Coverage gap

A fresh live code search on the post-Phase-69 recovery main found no Bron-Kerbosch or maximum-clique capability. This slice therefore changes proof model again after Mo's moving window, the suffix automaton's end-position equivalence, and matroid intersection's exchange graph: it adds exact exponential combinatorial search with a maximal-clique invariant.

The frozen Phase-45-69 compiler/backend surface is untouched. Prospective authority remains `docs/scope_recovery_after_phase69.md` plus fresh live coverage audits; the historically presentation-lagged `ROADMAP.md` is intentionally not changed by this recovery slice.

## Production contract

`maximum_clique_bron_kerbosch(const Graph&)` accepts the repository's undirected multigraph abstraction and returns:

- the lexicographically smallest sorted maximum-cardinality clique;
- the number of recursive Bron-Kerbosch calls;
- the number of maximal cliques examined.

Directed graphs are rejected. Self-loops do not make a vertex adjacent to itself for clique semantics. Parallel copies collapse to one adjacency relation, and edge weights are ignored because the objective is cardinality.

## Bron-Kerbosch invariant and pivot pruning

For each recursive state `(R, P, X)`:

- `R` is a clique;
- every vertex in `P` is adjacent to every vertex in `R` and remains eligible for extension;
- every vertex in `X` is adjacent to every vertex in `R` but was already considered on an earlier sibling branch;
- when both `P` and `X` are empty, `R` is maximal.

A pivot is chosen deterministically from `P union X` to maximize its number of neighbors in `P`, breaking ties by vertex id. Only vertices in `P \ N(pivot)` are branched on. Any maximal clique extending `R` must contain either the pivot (when eligible) or a vertex outside the pivot's neighborhood; otherwise the pivot could be added and the clique would not be maximal. Therefore pivot pruning preserves every maximal-clique witness.

Every maximum clique is maximal, so enumerating all maximal cliques and retaining the largest yields an exact maximum clique. Sorting each terminal witness and breaking equal-cardinality ties lexicographically makes the public result deterministic.

## Verification

Focused repository-style verification passed before upload:

- GCC strict warnings-as-errors: 4/4;
- Clang strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan: 4/4.

Deterministic cases cover empty/singleton input, directed rejection, two tied triangles with canonical witness selection, ignored self-loops, parallel copies with differing weights, a five-cycle, and a complete seven-vertex graph.

Randomized differential evidence uses 1,000 fixed-seed undirected multigraphs with 0-11 vertices. An independent oracle enumerates every vertex subset, checks clique feasibility directly from a simple adjacency relation, and returns the lexicographically smallest exact optimum. The production witness matches that oracle exactly on every trial.

The exhaustive oracle is implementation evidence, not a replacement for the Bron-Kerbosch correctness argument.

## Complexity and non-claims

Building the simple adjacency matrix costs `O(V^2 + E)` time and `O(V^2)` storage. Bron-Kerbosch with pivoting has an exponential worst case; using the classical `O(3^(V/3))` maximal-clique bound together with this direct vector/matrix implementation gives a conservative `O(V^2 * 3^(V/3) + E)` time characterization. The adjacency matrix dominates storage at `O(V^2)`, plus `O(V)` recursion/set state per active level.

This slice does not claim a polynomial-time maximum-clique algorithm, weighted maximum clique, all-maximal-clique public enumeration, bit-parallel acceleration, degeneracy ordering, or an optimal implementation constant. It also does not infer theorem correctness from randomized testing.
