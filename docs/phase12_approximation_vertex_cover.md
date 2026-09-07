# Phase 12 — approximation algorithms and executable quality guarantees

Phase 12 adds algorithms whose contract is not exact optimality but a proved relation between a returned feasible solution and an unknown optimum. The first slice is a deterministic 2-approximation for minimum vertex cover on the repository's undirected multigraph abstraction.

## Contract

`approximate_minimum_vertex_cover(graph)` accepts an undirected `Graph` and returns:

- `vertices`: the selected cover vertices in ascending order;
- `forced_self_loop_vertices`: every vertex carrying at least one self-loop;
- `maximal_matching_edges`: a deterministic vertex-disjoint matching on the residual graph after edges incident to forced vertices are already covered.

Directed input is rejected. Stored edge weights are irrelevant because this slice is the unweighted minimum-cardinality vertex-cover problem. Parallel copies do not change feasibility or the approximation proof. A self-loop `(v,v)` forces `v` into every feasible cover, so loop vertices are handled before the matching approximation rather than being silently excluded by a simple-graph assumption.

## Construction and invariant

Let `F` be the set of vertices with self-loops. Every feasible cover contains all of `F`. Ignore edges incident to `F` because they are already covered. On the remaining graph, scan vertices and insertion-order adjacency deterministically and greedily add an edge whenever both endpoints are still unmatched. The resulting set `M` is maximal and vertex-disjoint.

The returned cover is

`C = F union endpoints(M)`.

Every residual edge has at least one endpoint matched; otherwise that edge could be added to `M`, contradicting maximality. Therefore `C` covers every non-loop edge, while `F` covers all self-loops and all other edges incident to forced vertices.

## Approximation proof obligation

Every optimum cover must include all `|F|` forced loop vertices. The matching edges in `M` avoid `F` and are pairwise vertex-disjoint, so any cover needs at least one additional non-forced endpoint for every edge in `M`. Thus

`OPT >= |F| + |M|`.

The algorithm returns exactly

`|C| = |F| + 2|M| <= 2(|F| + |M|) <= 2 OPT`.

The universal 2-approximation guarantee follows from this argument, not from randomized testing. Tests provide executable evidence that the implementation realizes the proof obligations.

## Complexity

The implementation scans the graph adjacency a constant number of times and stores boolean state plus the witness vectors. With the repository's undirected adjacency representation the direct bound is `O(V + E)` time and `O(V + E)` result/auxiliary storage, where parallel edge instances count toward `E`.

## Verification

Deterministic coverage includes empty input, directed rejection, self-loops, parallel edges with different ignored weights, repeated deterministic execution, cliques, and stars.

An independent exhaustive oracle enumerates every vertex subset for graphs of at most ten vertices and finds the exact minimum cover. Six hundred fixed-seed random undirected multigraphs are checked for:

- replayed cover validity over original edge multiplicity;
- `|C| <= 2 * OPT`;
- witness lower bound `|F| + |M| <= OPT`;
- exact result-size identity `|C| = |F| + 2|M|`;
- self-loop necessity for every member of `F`;
- matching-edge existence, disjointness, and avoidance of forced vertices;
- maximality on the residual graph.

This corpus verifies the implementation against exact small instances. It is not the source of the universal approximation theorem.

## Frontier

This slice establishes the Phase-12 quality-guarantee discipline: the algorithm returns a feasible witness, the approximation factor has an explicit proof obligation, and small exact instances independently validate both feasibility and the claimed bound. Phase 12 remains active until architecture audit determines whether another genuinely different approximation guarantee is worth adding.
