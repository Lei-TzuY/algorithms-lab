# Phase 41 — Gomory-Hu all-pairs minimum-cut tree

## Scope and domain

This phase adds an exact Gomory-Hu cut-equivalent tree for an **undirected,
non-negative-capacity multigraph**. Parallel edges preserve multiplicity,
zero-capacity edges are valid, and self-loops are accepted but cut-neutral.
Invalid endpoints and negative capacities are rejected before any flow work.

`GomoryHuTree` exposes the rooted tree directly as `parent[v]` and
`cut_to_parent[v]`. Vertex `0` is the deterministic root with
`parent[0] == 0` and `cut_to_parent[0] == 0`. For every `v > 0`, the edge
`(v, parent[v])` has weight `cut_to_parent[v]`.

## Construction invariant and proof obligation

The implementation uses the standard Gomory-Hu construction. Initially every
non-root vertex has parent `0`. For `source = 1..V-1`, it computes an exact
minimum cut between `source` and its current parent with the sealed Dinic
implementation. Vertices that currently share that parent and lie on the
source side of the cut are reparented to `source`; the standard parent-swap
case is applied when the parent's own parent lies on that source side.

The **Gomory-Hu cut-equivalence theorem** is the proof obligation: after these
`V-1` minimum-cut computations, the minimum edge weight on the unique tree
path between any distinct pair `(u,v)` equals the exact `u-v` minimum-cut
value in the original graph. The randomized/exhaustive tests below are
implementation evidence; they do not replace that theorem.

Each undirected input edge `{u,v,c}` is represented for Dinic as two directed
capacity arcs `u->v` and `v->u`, each of capacity `c`. For an undirected cut,
exactly one of those two directed arcs crosses from the chosen source side to
the sink side, so Dinic's directed cut capacity equals the intended undirected
cut capacity. Self-loops are omitted from this reduction because they never
cross a cut.

## Query semantics

`min_cut(u,v)` validates both vertices and requires `u != v`. The baseline
query walks the cut-equivalent tree and returns the minimum edge value on the
unique path. This implementation deliberately uses an `O(V)` traversal with
`O(V)` temporary workspace; it does **not** claim a binary-lifted or other
accelerated query index.

Disconnected components and zero-capacity separations are represented by
zero-weight tree edges, so cross-component queries return `0` exactly.

## Representability

Capacities use the sealed `Capacity = int64_t` contract. The Gomory-Hu layer
does not wrap or saturate a cut value. If any required Dinic `s-t` cut is not
representable, the existing `std::overflow_error` propagates to the caller.

## Complexity

Construction performs exactly `V-1` calls to the sealed Dinic max-flow
routine, plus `O(V^2)` parent/cut bookkeeping in this direct implementation.
No asymptotic improvement over those max-flow calls is claimed. The stored
cut-equivalent tree is `O(V)` words beyond the input/repeated flow workspace.
The current pair query costs `O(V)` time and `O(V)` temporary workspace.

## Verification

Deterministic tests cover empty/singleton inputs, invalid endpoints, negative
capacities, self-loops, parallel and zero-capacity edges, disconnected graphs,
repeated deterministic construction, and an unrepresentable parallel-edge cut.

A fixed-seed differential suite generates 400 undirected multigraphs with
2–8 vertices. For **every distinct vertex pair** it compares the tree query
against an independent oracle that enumerates every source-containing,
sink-excluding bipartition and sums crossing input capacities directly. It
also verifies each represented tree edge through the public query API.

The oracle never calls Dinic and does not reproduce the Gomory-Hu parent
recurrence, so it is independent of both production layers being exercised.
