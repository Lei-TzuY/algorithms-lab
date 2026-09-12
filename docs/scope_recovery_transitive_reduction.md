# Scope recovery: DAG transitive reduction

## Coverage decision

A fresh live coverage audit after the merged Dilworth checkpoint found no DAG
transitive-reduction capability in code, documentation, PR history, or branch
state. Several superficially similar candidates were rejected because they are
already present (directed arborescence, minimum-mean cycle, Huffman coding,
maximum clique, Chinese postman) or because they would expand into a second
subproblem before delivering the target capability (exact Delaunay predicates).

This slice therefore adds the canonical reachability-preserving minimal
representation of a directed acyclic graph. Although the previous recovery also
used DAG reachability, the proof obligation is different: Dilworth is a
matching/duality theorem, while transitive reduction is a global
reachability-equivalence and edge-essentiality problem.

## Contract

`transitive_reduction(const Graph&)` accepts directed acyclic `Graph` input and
returns:

- one canonical `(from,to)` arc for every relation in the unique transitive
  reduction;
- a deterministic topological order that can be replayed independently.

The operation is structural. Edge weights are intentionally ignored. Parallel
arcs represent the same reachability relation and collapse to one canonical arc.
Undirected input, self-loops, and any directed cycle are rejected.

## Invariant and proof obligation

Production first canonicalizes each adjacency list and validates acyclicity with
Kahn's algorithm. It then materializes reachability from every source.

For an original structural arc `u -> v`, the arc is redundant exactly when some
other immediate successor `w != v` of `u` can reach `v`. Any alternate path from
`u` to `v` must begin with such an immediate successor; conversely, if such a
`w` reaches `v`, removing `u -> v` preserves that relation. Retaining precisely
the non-redundant arcs therefore preserves the complete reachability relation.
For a DAG the transitive reduction is unique, so the result is also the unique
minimum-edge reachability-equivalent subgraph.

## Independent verification

The primary randomized oracle does not reuse the production redundancy test.
For each bounded DAG it:

1. canonicalizes the input's unique structural arcs;
2. enumerates every subset of those arcs;
3. independently recomputes reachability for each subset;
4. chooses the smallest subset whose closure equals the original closure;
5. compares that exact subset with production output.

The fixed-seed suite covers 350 random DAG multigraphs with 0-6 vertices and at
most 11 unique arcs. Labels are randomly permuted, weights vary (including
negative values), and parallel copies are injected independently. Every returned
arc is also removed one at a time to verify that its deletion changes the
reachability relation.

Deterministic tests cover empty/singleton input, disconnected DAGs, diamonds and
long shortcuts, parallel weighted arcs, undirected rejection, self-loop
rejection, and longer directed cycles. Focused strict GCC, strict Clang, and
actual GCC ASan+UBSan execution passed before upload.

## Complexity and non-claims

The direct educational baseline runs graph reachability from every source and
then checks alternate first hops. With `V` vertices and `E` canonical arcs, it
uses `O(V(V+E))` time in the stated implementation bound and `O(V^2+E)` storage.

This slice does not claim cyclic-graph transitive reduction, weighted shortest-
path preservation, minimum equivalent digraph for general directed graphs,
bitset-accelerated closure, dynamic updates, or poset-specific Hasse-diagram
formatting beyond the structural DAG result.
