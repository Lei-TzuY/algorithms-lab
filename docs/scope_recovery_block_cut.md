# Scope recovery: vertex-biconnected blocks and block-cut forest

## Coverage decision

The live repository already contains undirected low-link analysis for bridges,
articulation vertices, and bridge-connected components. A fresh coverage audit
after the transitive-reduction checkpoint found no vertex-biconnected block or
block-cut-forest capability in code, documentation, or branch state.

This slice fills that distinct decomposition gap. Bridge components answer
2-edge-connectivity after deleting bridges; vertex-biconnected blocks instead
factor an undirected graph around articulation vertices and expose the
block/articulation incidence forest.

## Contract

`vertex_biconnected_decomposition(const Graph&)` accepts undirected `Graph`
input and returns:

- deterministic maximal vertex-biconnected blocks as sorted vertex sets;
- articulation vertices in increasing vertex order; and
- deterministic `(articulation_vertex, block_index)` incidence records that
  represent the block-cut forest.

The decomposition is structural. Edge weights are ignored. Parallel copies
collapse to one structural undirected adjacency relation because multiplicity
does not change vertex connectivity. Self-loops are connectivity-neutral and
are ignored for block structure. A bridge forms a two-vertex block; a vertex
with no non-loop structural neighbor forms a singleton block. Directed input is
rejected.

## Tarjan edge-stack invariant

Production first canonicalizes non-loop structural edges and builds an
edge-identified undirected adjacency. An explicit DFS frame stack maintains:

- `discovery[u]`: DFS discovery time;
- `low[u]`: the earliest discovery time reachable from the DFS subtree of `u`
  using zero or more tree edges followed by at most one back edge; and
- an edge stack containing exactly the discovered structural edges that have
  not yet been assigned to a completed block.

A tree edge `(u,v)` is pushed when `v` is first discovered. A back edge is
pushed only in the descendant-to-ancestor direction, so every structural edge
appears on the stack once. When child `v` finishes and
`low[v] >= discovery[u]`, no vertex below `v` can reach a strict ancestor of
`u` without passing through `u`; popping through tree edge `(u,v)` therefore
produces exactly one maximal vertex-biconnected block.

The implementation uses explicit DFS frames rather than recursive calls, so its
algorithmic stack storage is explicit and independent of process recursion
depth.

After all blocks are canonicalized, a vertex belongs to more than one block
exactly when it is an articulation vertex. Those repeated memberships are
returned as the block-cut incidence forest.

## Independent exhaustive oracle

The primary randomized oracle does not reuse discovery/low-link state. For each
bounded multigraph it first projects to the same documented structural simple
undirected relation, then enumerates every non-empty vertex subset.

A candidate subset is accepted independently when:

- a singleton is structurally isolated;
- a two-vertex subset contains an edge; or
- a larger subset is connected and remains connected after deleting any one of
  its vertices.

The oracle retains only inclusion-maximal accepted subsets. These exact maximal
subsets are the expected blocks. Articulation vertices and block-cut incidences
are then derived independently from repeated block membership.

The fixed-seed corpus covers 500 multigraphs with `0..7` vertices and up to 18
input edge copies, including random self-loops, parallel copies, and ignored
signed weights. Deterministic cases cover paths, bow-tie graphs, isolated
vertices, loops, parallel edges, repeated deterministic execution, and directed
input rejection.

The repo-native test additionally cross-checks the returned articulation set
against the already sealed `analyze_undirected_low_link()` capability. That
cross-layer assertion is integration evidence only; the exhaustive vertex-subset
oracle remains the primary correctness oracle.

Focused strict GCC, strict Clang, and actual GCC ASan+UBSan execution pass the
repo-native four-test candidate before upload.

## Complexity and non-claims

Let `E` be the number of stored non-loop edge copies and `E_s` the number of
unique structural undirected edges. Canonicalization sorts edge pairs, the
Tarjan core visits every structural vertex/edge once, and result
canonicalization sorts bounded output sets. The direct educational baseline is
therefore conservatively bounded by `O((V+E) log(V+E))` time and `O(V+E_s)`
auxiliary storage plus returned output.

This slice does not claim edge-biconnected decomposition (already covered by the
sealed bridge-component capability), triconnected/SPQR decomposition, dynamic
updates, weighted block semantics, or preservation of self-loop/parallel-edge
multiplicity inside blocks.
