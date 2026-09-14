# Scope recovery: separation pairs and structural triconnectivity

## Coverage decision

A fresh live default-branch, pull-request-history, branch, and recovery-document
audit at `main@07f7eb60505cd3404d8bd404cb858033027d0925` found no separation-pair
analysis, triconnected-component enumeration, or SPQR implementation. The sealed
block-cut decomposition stops at articulation vertices, while the sealed exact
vertex-connectivity slice explicitly lists separation pairs, triconnected
components, and SPQR trees outside its contract.

This recovery slice fills the exact **separation-pair / block triconnectivity**
boundary without pretending to implement a canonical SPQR tree. It deliberately
changes proof model again after Suurballe shortest paths: the subject is
structural two-vertex failure inside already vertex-biconnected blocks.

The historical compiler/backend ROADMAP remains prospectively frozen under
`docs/scope_recovery_after_phase69.md` and is not modified by this recovery.

## Production contract

`analyze_triconnectivity(const Graph&)` accepts the repository's undirected
multigraph abstraction and first reuses the sealed vertex-biconnected block-cut
decomposition. It returns:

- the deterministic articulation-vertex list inherited from block-cut;
- every deterministic vertex-biconnected block;
- a block kind: singleton, bridge, triangle, triconnected, or
  split-pair-decomposable;
- for every block with at least four vertices, every unordered separation pair
  `(u,v)` whose deletion disconnects the remaining block;
- for every returned pair, the sorted connected components of the remaining
  vertices as a replayable split witness.

Structural semantics match the sealed block-cut layer:

- directed input is rejected;
- self-loops do not affect structural connectivity;
- parallel edge copies collapse to one structural adjacency;
- edge weights are ignored;
- singleton and bridge blocks remain explicit small atoms;
- three-vertex blocks are classified as `triangle`, not called 3-connected;
- a block is called `triconnected` only when it has at least four vertices and
  has no separation pair.

## Separation-pair obligation

For one vertex-biconnected block `B`, an unordered pair `{u,v}` is returned if
and only if the structural graph induced by `B - {u,v}` has more than one
connected component. Production constructs deterministic block-local structural
adjacency, enumerates pairs in lexicographic vertex order, and replays each
pair deletion by an independent graph traversal that emits sorted component
witnesses.

The preceding block-cut step is important: articulation structure is not mixed
with pair separation. The output therefore gives an exact two-level structural
view—articulation-separated blocks first, separation pairs inside those blocks—
without claiming the virtual-edge normalization or S/P/Q/R node semantics of
SPQR decomposition.

## Verification

Focused pre-upload verification against the exact live `Graph` and sealed
block-cut implementation passed:

- GCC C++20 strict warnings-as-errors: 5/5;
- Clang C++20 strict warnings-as-errors: 5/5;
- actual GCC ASan+UBSan: 5/5.

Deterministic regressions cover directed rejection, a mixed disconnected graph
with articulation/bridge/triangle/triconnected blocks, `C4`'s two opposite
separation pairs, two `K4` blocks glued along one edge, and a `K4` augmented with
parallel copies, self-loops, and ignored signed weights.

The primary randomized corpus generates 1,200 fixed-seed vertex-biconnected
multigraphs by starting from a cycle and adding arbitrary chords, parallel
copies, self-loops, and signed weights. An independently implemented test oracle
rebuilds a structural adjacency matrix and, for every unordered pair, performs a
separate breadth-first component search after deleting that pair. Production
must match the complete lexicographically ordered pair-and-component witness set,
not only a boolean classification.

## Complexity and non-claims

For a block with `b` vertices and `e_b` structural edges, production enumerates
`O(b^2)` candidate pairs and performs an `O(b + e_b)` traversal for each. Across
all block-cut blocks, the direct educational baseline therefore costs
`O(sum b^2 (b + e_b))` time after block-cut construction. Auxiliary storage is
`O(V + max(b + e_b))` excluding the returned witness, whose size is itself
output-sensitive because every reported pair contains its component partition.

This slice does **not** claim canonical SPQR-tree construction, Hopcroft-Tarjan
linear-time triconnected decomposition, virtual-edge S/P/Q/R normalization,
dynamic triconnectivity, preservation of loop/parallel multiplicity inside the
structural relation, or optimized large-graph scalability. Those remain distinct
future frontiers rather than hidden claims of this bounded recovery slice.
