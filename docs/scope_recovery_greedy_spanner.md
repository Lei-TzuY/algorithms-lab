# Scope recovery: deterministic greedy weighted graph spanner

## Coverage decision

A fresh live audit after the one-sided Jacobi SVD recovery found no graph-spanner,
stretch-sparsifier, or distance-spanner production surface in default-branch code,
pull-request history, or the branch namespace. An apparently attractive interval-
stabbing frontier was explicitly rejected because `scope-recovery-interval-tree`
already contains an augmented AVL interval-tree implementation. This slice also
leaves the recent dense numerical-factorization streak rather than extending SVD,
LU, Cholesky, QR, or bidiagonalization again.

The historical compiler/backend ROADMAP remains non-authoritative prospectively;
`docs/scope_recovery_after_phase69.md` plus this fresh coverage audit governs the
selection.

## Production contract

`greedy_weighted_spanner(graph, stretch)` accepts an undirected multigraph with
non-negative signed-64 edge weights and integer stretch `stretch >= 1`.

- logical non-loop edges are processed deterministically by
  `(weight, min_endpoint, max_endpoint, canonical_adjacency_index)`;
- an edge `(u,v,w)` is inserted exactly when the currently selected subgraph has
  no `u-v` path of length at most `stretch*w`;
- self-loops are omitted because with non-negative weights they cannot improve a
  shortest-path distance;
- parallel copies remain distinct input edges and naturally compete by weight;
- the returned witness records the exact original adjacency entry used for every
  selected logical edge;
- directed input, zero stretch, and any negative edge are rejected;
- every required non-loop `stretch*w` must be representable in public
  `Weight=int64_t`; otherwise construction fails closed rather than saturating.

The selected graph uses the repository `Graph` abstraction. Bounded shortest-path
checks use the first-principles `BinaryHeap`; candidates above the current
threshold are pruned before addition, so the implementation never requires a
transient signed-distance overflow merely to decide the insertion predicate.

## Stretch proof obligation

When production skips an input edge `(u,v,w)`, the current spanner already
contains a `u-v` path of weight at most `t*w`, where `t=stretch`. Later steps only
add edges, so that replacement path remains available in the final spanner.

Take any shortest path in the original graph. Replacing each of its edges by the
corresponding selected edge or certified replacement path yields a spanner walk
of total weight at most `t` times the original path weight. Therefore for every
pair connected in the input,

`d_G(u,v) <= d_H(u,v) <= t * d_G(u,v)`.

The left inequality holds because `H` is a subgraph of `G`. Every original
non-loop edge is either selected or has a replacement path, so connected
components are preserved. Non-negative self-loops are irrelevant to these
shortest-path statements.

This proof supplies the universal stretch guarantee. Randomized tests are
implementation evidence; they are not used to infer the theorem.

## Independent verification

Focused candidate verification passed under:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with fail-fast/leak detection: 4/4.

Deterministic evidence covers empty/singleton graphs, directed/negative/zero-
stretch rejection, self-loops, parallel copies, exact deterministic selection,
full signed-64 edge-weight acceptance at stretch one, stretch-threshold overflow,
and zero-weight edges under the maximum `uint64_t` stretch value.

Primary randomized evidence uses 600 fixed-seed undirected multigraphs with
0..8 vertices, self-loops, parallel copies, zero weights, and integer stretches
1..5. It is deliberately two-layered:

1. an independent oracle replays the same mathematically specified greedy edge
   order but recomputes every current all-pairs distance from scratch with
   Floyd-Warshall; the complete selected original-edge witness must match
   production exactly;
2. a separate final Floyd-Warshall replay checks component preservation and every
   all-pairs stretch inequality, plus the replacement-path inequality for every
   logical input edge.

The oracle contains neither heap-Dijkstra nor the production bounded-relaxation
recurrence.

## Complexity and non-claims

Let `E` be the number of logical input edges and `V` the number of vertices. The
direct educational baseline sorts the edges in `O(E log E)` and may run one
binary-heap shortest-path search for each non-loop edge. A conservative direct
bound is

`O(E * (E+V) log(E+V))`

time and `O(V+E)` resident/result working scale, excluding allocator overhead.

No optimal/minimum-edge spanner, textbook `(2k-1)` sparsity-size theorem,
Euclidean/geometric spanner, directed spanner, negative-weight support, dynamic
updates, benchmark speedup, or asymptotically faster greedy-spanner construction
is claimed. In particular, this slice guarantees stretch, not a nontrivial edge-
count bound beyond being a subgraph of the input.

## Scope

The intended recovery slice changes exactly four paths:

- `include/algorithms/graphs/greedy_spanner.hpp`;
- `tests/test_greedy_spanner_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- `docs/scope_recovery_greedy_spanner.md`.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, occupied interval-tree/minimum-cycle-basis/Hirschberg
surface, or temporary-file churn is required.
