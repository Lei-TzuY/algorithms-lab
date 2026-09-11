# Scope recovery: exact planarity by rotation-system enumeration

## Coverage decision

A fresh live-code, PR-history, and branch audit after the merged order-statistic
treap checkpoint found no planarity / planar-embedding implementation. Recent
recovery PRs explicitly still list planarity as uncovered. A separate
`scope-recovery-minimum-cycle-basis` branch already exists, so this slice avoids
that occupied surface and instead recovers the distinct topological-embedding
proof model.

This work follows `docs/scope_recovery_after_phase69.md`. It does not resume the
historically frozen compiler/backend ROADMAP frontier.

## Production contract

`exact_planar_rotation_embedding(const Graph&)` accepts the repository's
undirected multigraph abstraction and returns either:

- `std::nullopt` when the graph is non-planar; or
- a replayable orientable genus-zero rotation system for the simplified
  underlying graph.

The public witness contains the cyclic order of the unique non-self-loop
neighbors around every vertex, a face count for every connected component, and
the exact number of complete rotation systems evaluated before the accepted
witness.

Directed input is rejected. Edge weights do not affect planarity. Self-loops are
removed and parallel copies are collapsed while searching because an undirected
multigraph is planar exactly when its underlying simple graph is planar: loop
arcs can be embedded in a sufficiently small vertex neighborhood and parallel
copies can be routed in a sufficiently small neighborhood of an existing edge.
The returned rotation therefore certifies the simple core; it is not a
per-original-edge drawing of duplicate copies.

## Exactness / proof obligation

For one connected simple graph, a cyclic order of incident darts at every
vertex is an orientable rotation system. The permutation obtained by reversing
a dart across its edge and then advancing to the next dart around the new
vertex partitions all directed darts into facial boundary walks. This defines a
cellular embedding on an orientable closed surface.

For that embedding,

`V - E + F = 2 - 2g`.

The implementation enumerates every cyclic order, modulo the irrelevant cyclic
shift at each vertex by fixing its smallest neighbor first. It accepts exactly
when a complete rotation system has `V - E + F = 2`, i.e. genus `g = 0`.
Consequently a connected component is accepted exactly when it admits a planar
rotation system; disconnected graphs are planar exactly when every component is
planar. A simple planar density bound `E <= 3V-6` is used only as a necessary
early rejection, never as a sufficiency test.

Correctness therefore relies on the classical rotation-system / combinatorial-
embedding correspondence and Euler characteristic theorem. Tests are
implementation evidence, not a substitute for those theorems.

## Determinism

The simple core is rebuilt in vertex-id order. Components are processed by
smallest contained vertex. Within a component, vertices are assigned in
non-increasing simple degree with vertex-id tie breaking; each cyclic order fixes
the smallest neighbor first and lexicographically permutes the remaining
neighbors. The first genus-zero assignment is therefore deterministic and
independent of original edge insertion order, duplicate multiplicity, and
weights.

## Verification

Focused candidate verification passed under:

- GCC C++20 strict warnings-as-errors: 5/5;
- Clang C++20 strict warnings-as-errors: 5/5;
- actual GCC ASan+UBSan with leak detection: 5/5.

Deterministic evidence covers empty graphs, trees, cycles, dense planar `K5-e`,
`K5`, `K3,3`, a subdivided `K3,3`, disconnected components, directed rejection,
self-loops, parallel copies, arbitrary signed weights, repeated execution, and
edge-insertion / weight invariance. Every returned witness is independently
replayed: each rotation must contain exactly the unique simple neighbors, face
walks are recounted, and Euler's equation is checked component by component.

The randomized primary oracle does not enumerate rotations. For 220 fixed-seed
simple cores with 0--6 vertices (with multigraph noise added in the public
`Graph` input), it applies Wagner's theorem independently: it enumerates all
connected branch-set models of `K5` and `K3,3`. Production must report planar
exactly when neither forbidden minor model exists. The explicit subdivided
`K3,3` regression additionally covers a seven-vertex non-planar subdivision.

## Complexity and non-claims

Let `d(v)` be simple degree, `Delta` maximum degree, and

`R = product_v max(1, (d(v)-1)!)`.

The first-principles baseline may examine `R` complete rotation systems. A full
face replay uses direct neighbor lookup and costs `O(E * Delta)`, while simple-
core normalization uses `O(V^2 + E)` time and `O(V^2 + E)` temporary storage.
Thus a conservative public bound is `O(V^2 + E + R * E * Delta)` time with
`O(V^2 + E)` auxiliary state, excluding the returned witness. Search recursion
uses `O(V)` stack.

This is deliberately **not** a Hopcroft-Tarjan, Boyer-Myrvold, linear-time, or
polynomial-time planarity implementation. It does not construct straight-line
coordinates, optimize embeddings, enumerate all embeddings, return a Kuratowski
subdivision certificate on failure, or claim practical scalability on
high-degree graphs. Its purpose is an exact first-principles planarity baseline
with a replayable combinatorial embedding and an honest complexity boundary.
