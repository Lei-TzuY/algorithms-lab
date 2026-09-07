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

`MaxFlowResult` returns:

- the maximum flow value,
- every original edge in input order with its realized flow,
- the residual source-side membership vector,
- the capacity of the induced `S -> T` cut.

The implementation recomputes that cut capacity with checked arithmetic and requires it to equal the flow value before returning. This internal consistency check is not used as the primary test oracle.

### Max-flow validation and verification

Out-of-range endpoints throw `std::out_of_range`; negative capacities and `source == sink` throw `std::invalid_argument`. Flow values and cut capacities are `int64_t`. If independent representable edge capacities admit a total s-t flow larger than `INT64_MAX`, checked accumulation throws `std::overflow_error` rather than wrapping.

Deterministic tests cover the classic six-vertex network with max flow 23; parallel, antiparallel, self-loop, zero-capacity, and disconnected behavior; input validation; and total-flow overflow.

Fixed-seed randomized verification generates 300 directed multigraphs with 2–8 vertices. For every graph, production Dinic is compared with a test-only Edmonds-Karp implementation. The same graph is also checked against exhaustive enumeration of every source-containing, sink-excluding cut. Finally, the returned witness is replayed edge by edge to verify capacity bounds, flow conservation at every internal vertex, source/sink net flow, cut membership, saturation of every `S -> T` edge, and `max_flow == cut_capacity`.

The Edmonds-Karp and exhaustive-cut oracles do not reuse Dinic's level graph, current-arc cursor, residual adjacency representation, or blocking-flow recurrence.

## Bipartite matching — Hopcroft-Karp and König certificate

A bipartite instance has explicit left vertices `0..L-1`, right vertices `0..R-1`, and `BipartiteEdge {left,right}` edges. Endpoint validation is strict. Parallel edges are accepted and preserve first-seen adjacency order; they do not create extra matching capacity because each vertex may participate in at most one matched pair.

`hopcroft_karp` maintains reciprocal left/right match arrays. Each phase performs a multi-source BFS from every unmatched left vertex to find the shortest augmenting-path length, then DFS augments only through alternating edges that respect those BFS layers. All augmentations in one phase therefore have minimum current length. Once no augmenting path exists, the matching is maximum.

The implementation claims the standard Hopcroft-Karp bound `O(E sqrt(V))`, where `E` includes parallel input edges and `V = L + R`. The matching state and adjacency storage are `O(V + E)`. The augmenting DFS is recursive and can use `O(V)` call stack on a long alternating path.

### Matching witness invariants

`BipartiteMatchingResult` exposes:

- the maximum cardinality,
- `left_match[left]` and `right_match[right]` as reciprocal optional partners,
- a left/right minimum-vertex-cover membership vector.

Every reported matched pair must correspond to an input edge, no left or right vertex can have two partners, and the number of reciprocal pairs equals `cardinality`.

### König minimum-vertex-cover certificate

After matching is maximum, alternating reachability starts from every unmatched left vertex. Traversal follows unmatched edges from left to right and matched edges from right back to left. Let the reachable sets be `Z_L` and `Z_R`. The returned cover is

`(Left \ Z_L) union Z_R`.

Every input edge is incident to that cover. For a maximum bipartite matching, König's theorem gives a minimum vertex cover of exactly the same cardinality, so the implementation returns a second concrete witness for the optimum rather than only a matching count.

### Independent and cross-layer verification

Deterministic tests cover empty partitions, a graph that requires reassignment along an augmenting path, parallel edges, partial matchings, and invalid endpoints.

Four hundred fixed-seed random bipartite multigraphs use at most six vertices per partition. Every Hopcroft-Karp cardinality is compared with an independent exhaustive recursive matching oracle. The same instance is also reduced to the already-implemented max-flow subsystem using unit-capacity `source -> left -> right -> sink` edges; Dinic's flow value must equal the matching cardinality.

The returned matching is replayed for reciprocal uniqueness and edge membership. The returned König cover is checked against every input edge and its size must equal the matching cardinality. The exhaustive matcher is the primary independent optimum oracle; the Dinic reduction is cross-layer integration evidence, not a circular production dependency.

## Lowest common ancestor — validated binary lifting

`LowestCommonAncestor` builds an immutable rooted-tree index over the existing undirected `Graph`. Construction rejects directed graphs, self-loops, parallel edges, cycles, disconnected input, and an invalid root before any query state is exposed. Edge weights are intentionally ignored because the current query surface is structural and reports distance in edges.

The constructor performs one rooted traversal to establish `parent[v]` and `depth[v]`, then builds a binary-lifting table where `up[k][v]` is the `2^k`-th ancestor of `v`; the root is its own stored ancestor. Preprocessing uses `O(V log V)` time and state.

`lca(u,v)` first lifts the deeper vertex to equal depth, then tests ancestor jumps from the largest power of two downward until both vertices have the same parent. `kth_ancestor(v,k)` decomposes `k` into binary jumps and returns `nullopt` when `k > depth[v]`. `distance_edges(u,v)` uses the rooted depths and their LCA to return the unique tree-path length.

### LCA invariants and complexity

- Parent invariant: every non-root vertex has exactly one parent discovered through the validated tree traversal; the root is its own table parent.
- Depth invariant: `depth[child] = depth[parent] + 1`.
- Jump invariant: after preprocessing, `up[k][v]` is the ancestor reached by exactly `2^k` parent steps unless the root is reached first, in which case it remains the root.
- LCA invariant: after equalizing depths, simultaneous unequal jumps preserve the true LCA strictly above both current vertices; their final parents are therefore the lowest common ancestor.

Each `lca` and `kth_ancestor` query is `O(log V)`; `depth` is `O(1)`; edge distance is `O(log V)` because it performs one LCA query. Construction is iterative, while queries perform no recursion.

### LCA independent verification

Deterministic tests cover sibling, cross-subtree, ancestor/descendant, root, edge-distance, k-th ancestor, and out-of-range query behavior. Separate invalid-input regressions reject directed graphs, self-loops, parallel edges, cycles, disconnected graphs, and an invalid root.

Five hundred fixed-seed random trees contain 1–80 vertices and choose a random root. Each tree executes 100 random query rounds. Production LCA, depths, edge distances, and k-th ancestors are compared with an independent BFS-rooted parent array plus naïve one-step parent climbing. The oracle does not use the binary-lifting table or any production jump recurrence.

## Frontier

Max flow / min cut, bipartite matching, and LCA now cover residual optimization, matching/cover duality, and repeated rooted-tree queries. The remaining ordered Phase-6 frontier is offline algorithms; Phase 6 is not sealed yet.
