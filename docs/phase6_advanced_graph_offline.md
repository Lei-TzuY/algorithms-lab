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

## Validation and representability

Out-of-range endpoints throw `std::out_of_range`; negative capacities and `source == sink` throw `std::invalid_argument`. Flow values and cut capacities are `int64_t`. If independent representable edge capacities admit a total s-t flow larger than `INT64_MAX`, checked accumulation throws `std::overflow_error` rather than wrapping.

## Independent verification

Deterministic tests cover the classic six-vertex network with max flow 23; parallel, antiparallel, self-loop, zero-capacity, and disconnected behavior; input validation; and total-flow overflow.

Fixed-seed randomized verification generates 300 directed multigraphs with 2–8 vertices. For every graph, production Dinic is compared with a test-only Edmonds-Karp implementation. The same graph is also checked against exhaustive enumeration of every source-containing, sink-excluding cut. Finally, the returned witness is replayed edge by edge to verify capacity bounds, flow conservation at every internal vertex, source/sink net flow, cut membership, saturation of every `S -> T` edge, and `max_flow == cut_capacity`.

The Edmonds-Karp and exhaustive-cut oracles do not reuse Dinic's level graph, current-arc cursor, residual adjacency representation, or blocking-flow recurrence.

## Frontier

This checkpoint establishes max-flow/min-cut residual semantics and theorem-level witnesses. The next ordered Phase-6 slice is bipartite matching, which should integrate with this capability rather than merely add an unrelated matcher implementation.
