# Scope recovery: constructive open ear decomposition

## Why this slice belongs in algorithms-lab

The preceding Robbins strong-orientation slice exercised the bridge-free / two-edge-connected boundary. This slice deliberately moves to the different vertex-connectivity theorem: a simple undirected graph with at least three vertices is two-vertex-connected exactly when it admits an open ear decomposition.

The production API returns a replayable constructive witness rather than only a Boolean recognition result. It reuses the sealed vertex-biconnected decomposition only as the domain gate; the ear construction itself is new.

## Contract

`open_ear_decomposition(graph)` accepts simple undirected graphs. Directed input, self-loops, and parallel edges are rejected with `std::invalid_argument`; edge weights are intentionally ignored. Graphs with fewer than three vertices, disconnected graphs, and graphs with an articulation vertex return `not_two_vertex_connected`. When an articulation vertex is available from the sealed block-cut decomposition it is returned as a blocking witness.

On success, `ears[0]` is a simple cycle represented with its first vertex repeated at the end. Every later ear is an open path whose endpoints already occur in earlier ears and whose internal vertices are new. The complete ear sequence partitions every structural edge exactly once.

## Construction and invariants

1. Canonicalize the simple edge set and build deterministic adjacency ordered by `(neighbor, edge_id)`.
2. Reuse `vertex_biconnected_decomposition` to reject an articulation point after an explicit connectivity and `|V| >= 3` gate.
3. Choose the lexicographically first structural edge. In a two-vertex-connected graph it lies on a cycle, so a BFS with that edge removed yields an alternate path; that path plus the removed edge is the initial cycle.
4. Maintain the vertices already incorporated by the current ear union. For the lexicographically first outside component, collect its edges to the incorporated subgraph. Two-vertex-connectivity guarantees at least two distinct attachment vertices: otherwise the sole attachment would be an articulation point. A deterministic BFS inside the component connects two such attachments and produces the next open ear, whose internal vertices are all new.
5. Repeat until every vertex is incorporated. Any unused structural edge then has both endpoints in the existing ear union and becomes a valid single-edge ear.

The central proof obligation is therefore constructive: every non-initial ear has old, distinct endpoints and new internal vertices; each chosen edge is consumed once; when construction ends, all vertices and all structural edges are represented.

## Complexity

This direct, proof-oriented implementation recomputes outside components and an internal path as ears are added. The conservative bound is `O(V(V + E))` time with `O(V + E)` working storage. No linear-time ear-decomposition claim is made.

## Verification

Deterministic cases cover domain rejection, too-small and articulation-point failures, a triangle, and a graph that requires both a nontrivial later ear and leftover single-edge ears. Repeated execution must return the identical ear sequence.

A fixed-seed randomized suite generates 700 simple undirected graphs with `0..8` vertices. The primary oracle is independent of production: it removes every vertex in turn and runs BFS to decide two-vertex-connectivity. For every successful production result, a separate replay checker verifies the initial cycle, old-endpoint/new-internal-vertex rule, edge existence, no repeated edge, complete vertex coverage, and exact edge partition. If production returns an articulation witness, deleting that vertex must disconnect the graph under the same independent oracle.

Focused GCC strict-warning, Clang strict-warning, and actual ASan+UBSan builds all pass the four repo-native test cases before upload. Full-repository integration remains gated by GitHub Actions on the exact PR head.

## Boundary

This is intentionally the classical simple-graph theorem. Extending the witness format to multigraph ears, closed ears, or a linear-time implementation would be separate work and is not implied by this checkpoint.
