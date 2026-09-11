# Scope recovery: exact graph coloring with DSATUR search

## Coverage decision

After exact strict-tree isomorphism reached merged-main green, a fresh live-code,
PR-history, and branch audit found no exact graph-coloring / chromatic-number /
DSATUR capability. The old frozen compiler register-allocation phase contains a
bounded greedy interference coloring, but explicitly makes no optimal-coloring or
chromatic-number claim and is not an active recovery surface.

Planarity and minimum cycle basis also remain uncovered, but both would continue
the recent graph-cycle / embedding streak already noted by the tree-isomorphism
checkpoint. Exact coloring changes proof model to feasibility search with color
symmetry breaking while still enabling a useful cross-check against the sealed
maximum-clique capability.

## Production contract

`exact_graph_coloring_dsatur(const Graph&)` accepts the repository's undirected
multigraph abstraction and returns either an exact coloring result or
`std::nullopt` when a self-loop makes proper coloring impossible.

- directed input is rejected;
- parallel edge copies collapse to one adjacency relation;
- edge weights are ignored;
- empty graphs have chromatic number zero and an empty witness;
- non-empty proper colorings use dense labels `0..chi-1`;
- deterministic DSATUR tie breaking is saturation degree, then simple degree,
  then smallest vertex id;
- the result exposes a maximum-clique lower-bound witness, the initial greedy
  DSATUR upper bound, and exact-search node diagnostics.

## Exactness and invariants

Production first simplifies the multigraph to a simple adjacency matrix while
preserving self-loop infeasibility. The sealed Bron-Kerbosch implementation
supplies an exact clique witness, hence the lower bound `omega(G) <= chi(G)`.
A deterministic greedy DSATUR pass supplies a replayable proper coloring and an
upper bound.

For every integer `k` from the clique lower bound through one less than the greedy
upper bound, production runs an exact DSATUR-ordered feasibility search. At a
partial state:

- every colored edge has differently colored endpoints;
- saturation degree is the number of distinct neighbor colors already present;
- the next vertex maximizes saturation, then static degree, with vertex id as the
  deterministic final tie break;
- existing colors are tried in increasing order;
- because color names are interchangeable, a branch may introduce only the next
  unused color label. Every feasible coloring can be relabeled into this
  first-introduction normal form, so this symmetry break removes duplicates
  without losing a feasibility class.

The first feasible `k` is therefore the exact chromatic number. If no smaller `k`
is feasible, the greedy witness proves the upper bound is exact. The maximum
clique accelerates the lower boundary but is not used as an equality assumption:
an explicit five-cycle regression has `omega=2` and `chi=3`.

## Verification

Focused repo-style verification before upload passed:

- GCC C++20 strict warnings-as-errors: 8/8;
- Clang C++20 strict warnings-as-errors: 8/8;
- actual GCC ASan+UBSan with leak detection: 8/8.

Deterministic cases cover empty input, directed rejection, self-loop
infeasibility, complete and complete-bipartite graphs, odd cycles, repeated
execution, parallel copies with different weights, and edge-insertion-order /
weight invariance.

The primary randomized oracle is independent of DSATUR and Bron-Kerbosch: 600
fixed-seed undirected multigraphs with 0-9 vertices are simplified only for
adjacency, then a plain vertex-id-order backtracking oracle tests `k=1..n` and
returns the first feasible `k`. Production must match that exact value and its
returned coloring is replayed edge by edge. Separately, the sealed maximum-clique
solver is replayed only as secondary integration evidence that
`|clique_witness| <= chi <= greedy_upper_bound`.

## Complexity and non-claims

Adjacency simplification and resident state use `O(V^2 + E)` time / `O(V^2)`
storage. Bron-Kerbosch contributes its existing exponential lower-bound search.
The direct exact coloring feasibility search is also exponential in the worst
case; a conservative bound is `O(V^2 * sum_{k=omega}^{U-1} k^V)` for greedy upper
bound `U`, plus the clique search. Search recursion uses `O(V)` depth.

This slice makes no polynomial-time, weighted coloring, list coloring,
precoloring-extension, coloring-count, canonical graph-labeling, planar-coloring,
or register-allocation optimality claim. Tests are implementation evidence; they
do not replace the graph-coloring and color-symmetry arguments above.
