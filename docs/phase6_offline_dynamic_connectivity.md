# Phase 6 — offline dynamic connectivity

This slice turns the Phase-4 rollback DSU into a temporal graph engine. The input is known offline as a sequence of undirected `AddEdge`, `RemoveEdge`, and `QueryConnected` operations. The result contains one boolean answer for every query, in query order.

## Timeline and multiedge semantics

Every edge is canonicalized as `(min(u,v), max(u,v))`. Edge multiplicity is intentional: each `AddEdge` opens one active copy, and each `RemoveEdge` closes one currently active copy of the same canonical edge. Identical active copies therefore behave as a multiset. Removing one of two copies leaves the edge logically active; removing an edge with no active copy throws `std::invalid_argument`. Self-loops are legal and follow the same add/remove balancing rules, but they never change connectivity. Every operation validates both endpoints before temporal preprocessing begins.

A preprocessing scan pairs removals with active additions and turns every edge copy into one half-open active interval `[add_time, remove_time)`. Copies never removed remain active through `[add_time, T)`. A deterministic `std::map` stores the stacks of unmatched additions for distinct canonical edges.

## Segment tree over time

Each active interval is inserted into the `O(log T)` segment-tree nodes whose time ranges partition that interval. A depth-first traversal then represents exactly one path of nested time ranges. Entering a node applies all edges assigned to that range to `RollbackDisjointSetUnion`; reaching a leaf answers the query at that time; leaving the node rolls the DSU back to the snapshot taken on entry.

The central invariant is temporal: immediately before evaluating a leaf at time `t`, the rollback DSU contains every edge copy whose active interval covers `t`, and no edge copy whose interval excludes `t`. Segment-tree range decomposition supplies the first half of that invariant; snapshot/rollback supplies the second by preventing state from leaking between sibling time ranges.

Repeated copies of an already connected edge may make `unite` a no-op. This is compatible with rollback because the Phase-4 DSU records history only for successful root links; rolling back to the entry snapshot still restores the exact partition that existed before the node.

## Complexity boundary

Let `T` be the number of operations, `A` the number of `AddEdge` operations, `Q` the number of connectivity queries, and `D` the number of distinct canonical edges. Pairing the timeline with the ordered map is `O(T log D)` in the worst case. Each active edge interval is stored in `O(log T)` segment nodes.

The reused rollback DSU deliberately has no path compression. With union by size, `find`, `connected`, and `unite` take `O(log V)` worst-case time; each successful rollback record is undone in `O(1)`. The segment traversal therefore costs `O((A log T + Q) log V)` for DSU work, with `O(A log T + V + T)` auxiliary state. No inverse-Ackermann claim is made.

## Verification

Deterministic regressions cover:

- an add/query/remove/query timeline,
- path connectivity appearing and disappearing across multiple edges,
- duplicate active copies where one removal must not disconnect the pair,
- balanced self-loop add/remove behavior,
- empty timelines,
- invalid endpoints,
- unknown operation kinds,
- removal of an inactive edge.

Fixed-seed randomized verification uses seed `0x0FF11E` for 500 traces. Each trace has 1–10 vertices and 120 operations, including duplicate edges, self-loops, adds, valid removals, and connectivity queries. The oracle maintains an independent active-edge multiset; for every query it rebuilds an ordinary adjacency list from currently positive multiplicities and runs BFS. The entire production answer vector must equal the oracle answer vector.

The oracle does not use rollback DSU, segment-tree time decomposition, interval pairing, or production connectivity state. This makes the test evidence independent from the temporal recurrence while still exercising the Phase-4 rollback DSU through production.

## Phase boundary

With max flow/min cut, bipartite matching, LCA, and offline dynamic connectivity all represented, the ordered Phase-6 implementation list is complete. That does not seal Phase 6 by itself. The next required action after exact candidate and merged-main CI are green is an architecture/integration sealing audit covering theorem witnesses, cross-phase reuse, complexity claims, validation semantics, and oracle independence before Phase 7 is promoted.
