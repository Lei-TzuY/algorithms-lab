# Scope recovery: exact Eulerian trails and circuits

## Capability

`eulerian_trail(const Graph&)` returns one deterministic trail that consumes every
logical edge exactly once, or `std::nullopt` when no such trail exists. The
implementation supports both directed and undirected repository multigraphs,
including parallel edges and self-loops. Edge weights are deliberately ignored:
Eulerian feasibility and traversal depend only on incidence and direction.

The trivial conventions are explicit: an empty graph returns an empty circuit;
a non-empty edgeless graph returns the singleton walk `{0}` and is also a
circuit.

## Logical-edge normalization

For a directed graph, every stored adjacency entry is one logical edge. For an
undirected graph, `Graph::add_edge` stores each non-loop edge in both endpoint
adjacency lists and each self-loop once. Normalization therefore scans only
entries with `from <= to`, assigning one edge identity per logical edge copy.
Parallel copies receive distinct edge identities; an undirected self-loop
contributes degree two but needs only one traversal adjacency entry.

This edge-identity layer is what prevents parallel copies from being collapsed
or one undirected edge from being consumed twice.

## Existence conditions

All vertices incident to at least one logical edge must lie in one weakly
connected component after directions are ignored.

For an undirected graph, exactly zero or two vertices may have odd degree. With
two odd vertices, the smaller vertex id is the deterministic start; otherwise
the smallest non-zero-degree vertex is used.

For a directed graph, either every vertex has equal in/out degree, or exactly
one vertex has `out = in + 1` and exactly one has `in = out + 1`. The positive
imbalance is the start when it exists; otherwise the smallest vertex with
non-zero out-degree is used. Together with weak connectivity of the active
vertices, these are the standard directed Euler-trail conditions.

## Hierholzer invariant

The traversal stack is the current unfinished walk. Each logical edge id is
marked used exactly once when traversed. A per-vertex cursor skips already-used
incident identities, so total adjacency scanning is linear. When the stack top
has no unused outgoing/incident edge, that vertex is final for the reverse
Eulerian trail and is appended to the output. Reversing this postorder after the
stack empties yields the complete trail.

Production additionally checks that exactly all normalized edge identities were
consumed and that the reconstructed vertex sequence has `E + 1` entries. Thus a
missed feasibility condition cannot silently expose a partial witness.

## Verification

Deterministic regressions cover empty/edgeless graphs, paths and circuits,
parallel edges, self-loops, directed imbalance, disconnected active components,
and the fact that edge weights do not affect the result.

The fixed-seed randomized corpus contains 1,200 directed or undirected
multigraphs with 0-6 vertices and up to 9 independently identified edge copies.
The primary oracle is an exhaustive memoized search over `(current_vertex,
used_edge_mask)` from every possible start. It does not use degree conditions,
connectivity conditions, or Hierholzer's recurrence. Every production witness
is replayed edge-by-edge against the original logical edge identities, and a
second production call must reproduce the exact same result.

Focused pre-upload builds passed the repository warning policy under GCC and
Clang, plus an actual GCC AddressSanitizer + UndefinedBehaviorSanitizer build:
4/4 focused test groups in all three configurations.

## Complexity and non-claims

Normalization, feasibility checks, and Hierholzer traversal take `O(V + E)`
time and `O(V + E)` auxiliary storage, counting parallel logical edges
separately. The output itself contains `E + 1` vertices for a non-empty edge set.

The returned witness is deterministic for a fixed repository `Graph` adjacency
order, but it is not claimed to be lexicographically minimum or invariant under
edge insertion order. This is not route optimization, a weighted Chinese
postman solver, or a library Euler implementation. No timing measurement is used
as asymptotic evidence.
