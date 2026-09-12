# Scope recovery: offline dynamic bipartiteness

## Coverage decision

A fresh live audit at `main@91bbcd8a57c1b2c8e53ed22e9fbe87002a55cd06`
found no parity-DSU, rollback-parity, or offline dynamic-bipartiteness
implementation in live code, PR history, or branches. The sealed Phase-6 offline
dynamic-connectivity solver answers connectivity only; it does not maintain XOR
color constraints or detect odd-cycle contradictions. Occupied recovery branches
such as minimum cycle basis and general graph isomorphism are intentionally left
untouched.

This slice changes proof model after exact Stable Roommates: it combines offline
time decomposition with a rollback system of parity equations instead of another
matching/preference reduction.

## Production contract

`offline_dynamic_bipartiteness(vertex_count, operations)` accepts an offline
undirected edge timeline with `AddEdge`, `RemoveEdge`, and global
`QueryBipartite` operations.

- Each add opens one active copy of the canonical unordered edge.
- Each remove closes one currently active copy (LIFO among identical copies).
- Removing an inactive edge is rejected.
- Parallel copies preserve multiplicity.
- An active self-loop is an immediate parity contradiction.
- A query returns whether the complete active multigraph is bipartite.
- Global queries do not require vertex operands, so the empty graph can be
  queried and is bipartite.

## Parity invariant and rollback proof obligation

Every active non-loop edge imposes the equation

`color(u) xor color(v) = 1`.

The rollback DSU stores no path compression. For each vertex, XORing
`parity_to_parent` along the parent chain yields
`color(vertex) xor color(representative)`. When two components are joined, the
attached root receives exactly the parity that makes the new edge equation true.
Union by size keeps parent depth `O(log V)`.

For an edge whose endpoints already share a representative, equal endpoint
parities contradict the required XOR value one. The implementation records that
contradiction; unequal parities are a consistent redundant constraint. Every
applied constraint, including consistent no-ops, pushes one history record, so a
snapshot is exactly the history length and rollback restores both partition
state and contradiction state.

The active intervals are inserted into a segment tree over time. A DFS applies
only the constraints whose intervals cover the current node, answers leaf
queries, then rolls back to the entry snapshot. Thus every query sees exactly
its active edge multiset.

`conflicts == 0` is equivalent to satisfiability of all currently applied XOR
constraints: a consistent two-coloring satisfies every edge, while any failed
same-component parity equation closes an odd parity cycle (a self-loop is the
length-one case).

## Verification

Focused pre-upload verification passed under:

- GCC C++20 strict warnings-as-errors;
- Clang C++20 strict warnings-as-errors;
- actual GCC ASan+UBSan with leak detection.

The committed suite covers an odd triangle becoming non-bipartite and recovering
after removal, even cycles, disconnected odd components, self-loops, duplicate
edge copies, inactive removal, invalid endpoints, unknown operation kinds, empty
graph queries, and deterministic replay.

The primary randomized oracle is independent of rollback parity DSU. For 400
fixed-seed traces of 120 operations over small multigraphs, tests maintain the
active edge multiset directly; at every query they rebuild an adjacency list from
scratch and run ordinary BFS two-coloring. Production answers must match that
rebuilt oracle exactly.

## Complexity and non-claims

Let `T` be timeline length, `A` the number of adds, `Q` the number of queries,
and `D` the number of distinct canonical edges seen by the pairing map.

- Pairing add/remove events with ordered maps is `O(T log(D+1))`.
- Each active interval is stored in `O(log T)` segment-tree nodes.
- Each parity-DSU find/union costs `O(log V)` with union by size and no path
  compression; rollback of one recorded constraint is `O(1)`.
- The direct bound is therefore
  `O(T log(D+1) + (A log T + Q) log(V+1))` time and
  `O(V + A log T + T)` auxiliary/history storage.

This is an offline solver, not fully dynamic online bipartiteness. It does not
return a canonical coloring, an odd-cycle witness, dynamic matching, or a
persistent version API, and it makes no inverse-Ackermann claim because rollback
forbids path compression.
