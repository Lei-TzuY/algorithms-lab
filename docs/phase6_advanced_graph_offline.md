# Phase 6 — advanced graph and offline algorithms

Phase 6 begins with max flow / min cut because it adds a residual-network state model and a theorem-level executable certificate rather than another ordinary path traversal.

## Max flow / min cut — Dinic residual network

The public input is a directed multigraph described by `CapacityEdge {from, to, capacity}` over dense vertices `0..V-1`. Capacities are signed `int64_t` only so invalid negative input can be rejected explicitly; every accepted capacity is non-negative. Parallel and antiparallel edges are represented independently. Self-loops are accepted but receive canonical zero flow because a circulation around a self-loop cannot improve an s-t objective or cross an s-t cut. Zero-capacity edges remain visible in the returned edge list.

Each materialized original edge owns one forward residual arc and one paired reverse residual arc. If its current flow is `f` and capacity is `c`, the forward residual capacity is `c-f` and the paired reverse residual capacity is `f`. Every augmentation decreases one side and increases the other by the same amount, preserving `0 <= f <= c`.

Dinic repeatedly builds a BFS level graph using only positive-residual arcs and then sends blocking flow through edges whose levels advance by exactly one. A current-arc cursor prevents rescanning dead outgoing arcs within a blocking-flow phase. The implementation uses recursive blocking-flow DFS, so the call stack can be `O(V)` on a deep level graph.

### Dinic invariants and complexity

- Level invariant: every traversed blocking-flow arc satisfies `level[to] = level[from] + 1`.
- Residual-pair invariant: a forward/reverse pair encodes exactly the remaining forward capacity and cancellable flow.
- Capacity invariant: reconstructed original-edge flow always lies in `[0, capacity]`.
- Conservation invariant: augmenting an s-t path changes intermediate inflow and outflow equally.
- Phase progress: after a blocking flow, no s-t path remains in the current level graph; rebuilding levels therefore strictly increases the shortest residual s-t distance until the sink becomes unreachable.

For a general directed network the implementation claims the standard Dinic bound `O(V^2 E)` and `O(V + E)` residual state. It does not claim specialized unit-capacity or bipartite bounds.

## Executable min-cut certificate

When no residual s-t path remains, the implementation performs one final residual reachability scan from the source. Let `S` be those reachable vertices and `T` the remainder. The sink must be in `T`. Any original edge from `S` to `T` has zero forward residual capacity; otherwise its endpoint would also be reachable. Therefore every such edge is saturated.

`MaxFlowResult` returns the maximum flow value, every original edge in input order with its realized flow, the residual source-side membership vector, and the capacity of the induced `S -> T` cut. The implementation recomputes that cut capacity with checked arithmetic and requires it to equal the flow value before returning. This internal consistency check is not used as the primary test oracle.

### Max-flow validation and verification

Out-of-range endpoints throw `std::out_of_range`; negative capacities and `source == sink` throw `std::invalid_argument`. Flow values and cut capacities are `int64_t`. If independent representable edge capacities admit a total s-t flow larger than `INT64_MAX`, checked accumulation throws `std::overflow_error` rather than wrapping.

Deterministic tests cover the classic six-vertex network with max flow 23; parallel, antiparallel, self-loop, zero-capacity, and disconnected behavior; input validation; and total-flow overflow. Fixed-seed randomized verification generates 300 directed multigraphs with 2–8 vertices. Production Dinic is compared with a test-only Edmonds-Karp implementation and exhaustive enumeration of every source-containing, sink-excluding cut. The returned witness is replayed for capacity bounds, conservation, source/sink net flow, cut membership, saturation, and `max_flow == cut_capacity`.

## Bipartite matching — Hopcroft-Karp and König certificate

A bipartite instance has explicit left vertices `0..L-1`, right vertices `0..R-1`, and `BipartiteEdge {left,right}` edges. Endpoint validation is strict. Parallel edges are accepted and preserve first-seen adjacency order; they do not create extra matching capacity because each vertex may participate in at most one matched pair.

`hopcroft_karp` maintains reciprocal left/right match arrays. Each phase performs a multi-source BFS from every unmatched left vertex to find the shortest augmenting-path length, then DFS augments only through alternating edges that respect those BFS layers. Once no augmenting path exists, the matching is maximum. The implementation claims the standard Hopcroft-Karp bound `O(E sqrt(V))`, where `E` includes parallel input edges and `V = L + R`; the augmenting DFS can use `O(V)` call stack.

After matching is maximum, alternating reachability from unmatched left vertices yields the König cover `(Left \ Z_L) union Z_R`. Every input edge must be incident to the returned cover, and cover size equals matching cardinality.

Four hundred fixed-seed random bipartite multigraphs use at most six vertices per partition. Every Hopcroft-Karp cardinality is compared with an independent exhaustive recursive matching oracle. The same instance is also reduced to the already-implemented max-flow subsystem using unit-capacity `source -> left -> right -> sink` edges; Dinic's flow value must equal the matching cardinality. The exhaustive matcher remains the primary independent optimum oracle; Dinic is cross-layer integration evidence.

## Lowest common ancestor — validated binary lifting

`LowestCommonAncestor` builds an immutable rooted-tree index over the existing undirected `Graph`. Construction rejects directed graphs, self-loops, parallel edges, cycles, disconnected input, and an invalid root before any query state is exposed. Edge weights are intentionally ignored because the query surface is structural and reports distance in edges.

The constructor establishes `parent[v]` and `depth[v]`, then builds `up[k][v]`, the `2^k`-th ancestor of `v`, with the root as its own stored ancestor. Preprocessing uses `O(V log V)` time and state. `lca` and `kth_ancestor` are `O(log V)`, `depth` is `O(1)`, and edge distance is `O(log V)` through one LCA query. Construction is iterative and queries perform no recursion.

Five hundred fixed-seed random trees contain 1–80 vertices and choose a random root. Each tree executes 100 random query rounds. Production LCA, depths, edge distances, and k-th ancestors are compared with an independent BFS-rooted parent array plus naïve one-step parent climbing.

## Offline dynamic connectivity — temporal rollback integration

The final ordered implementation slice interprets an offline sequence of undirected `AddEdge`, `RemoveEdge`, and `QueryConnected` operations. It reuses the Phase-4 `RollbackDisjointSetUnion` instead of introducing another mutable connectivity engine.

Every undirected edge is canonicalized. Each add opens one active copy and each remove closes one active copy; duplicate active copies therefore form a multiset. Removing an inactive edge is rejected. Self-loops are legal and obey the same temporal balancing rules but never change connectivity.

A preprocessing scan converts each active copy into a half-open interval `[add_time, remove_time)`, with unremoved copies extending to the end of the timeline. Each interval is stored in the `O(log T)` nodes of a segment tree over time that exactly cover it. During a depth-first segment traversal, node edges are united in the rollback DSU, leaf queries are answered, and the DSU is restored to the entry snapshot before the sibling range is visited.

**Temporal partition invariant.** At a query leaf for time `t`, the DSU contains exactly the connectivity effect of edge copies whose active intervals contain `t`. Segment range decomposition ensures every active interval is applied on the leaf's root-to-leaf path, while snapshot/rollback prevents edges from leaking to times outside their intervals.

The rollback DSU intentionally has no path compression. With union by size, its `find`/`connected`/`unite` operations are `O(log V)` worst-case, so no inverse-Ackermann claim is made. Let `A` be the number of adds, `Q` the number of queries, `T` the operation count, and `D` the number of distinct canonical edges. Ordered-map interval pairing is `O(T log D)`; temporal DSU work is `O((A log T + Q) log V)`; auxiliary state is `O(A log T + V + T)`.

Detailed temporal semantics and verification obligations live in [`phase6_offline_dynamic_connectivity.md`](phase6_offline_dynamic_connectivity.md). Deterministic regressions cover add/remove timelines, duplicate copies, self-loops, invalid endpoints/kinds, and inactive removal. Five hundred fixed-seed traces of 120 operations over 1–10 vertices are compared with an independent active-edge multiset that rebuilds an ordinary adjacency list and runs BFS for every query.

## Sealed checkpoint

All four ordered Phase-6 implementation slices are integrated on main and passed the three-job CI matrix at checkpoint `b0c5659f08f0938e986ddf3a907ff17fcfad05a8`. The architecture/integration review is recorded in [`phase6_sealing_audit.md`](phase6_sealing_audit.md).

The audit found no correctness, architecture, cross-layer-integration, oracle-independence, validation, or complexity-claim blocker. Phase 6 is therefore sealed. Phase 7 is the active frontier; computational-geometry foundations are first.
