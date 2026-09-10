# Scope recovery: undirected low-link structure

## Capability

This slice adds Tarjan-style low-link analysis to the existing graph-traversal
surface for the repository's undirected multigraph model. It returns:

- every bridge as an exact edge witness `(first, second, weight)`;
- every articulation vertex in increasing vertex order; and
- deterministic bridge-component labels obtained after all bridges are removed.

Edge weights do not participate in connectivity, but a bridge witness preserves
the stored weight. Parallel edge copies are distinguished internally, so one
copy cannot be reported as a bridge while another parallel copy still connects
the same endpoints. Self-loops are connectivity-neutral for bridge/articulation
classification.

Directed input is rejected rather than silently projected to an undirected
view.

## First-principles representation

`Graph` stores an undirected non-loop edge twice, once in each endpoint's
adjacency list, and a self-loop once. The analysis first reconstructs one
canonical internal edge identity by scanning vertices in increasing order and
retaining only adjacency entries with `from <= to`. Each non-loop identity is
then placed into an internal edge-id adjacency at both endpoints; a self-loop is
placed once.

The edge id is essential. Merely remembering the DFS parent vertex would be
incorrect for a multigraph because a second parallel edge to the parent is a
back edge, not the tree edge that must be skipped.

## Low-link invariant

For every discovered vertex `u`:

- `discovery[u]` is its DFS discovery time;
- `low[u]` is the minimum discovery time reachable from `u` by descending zero
  or more DFS-tree edges and then using at most one non-tree edge.

When child `v` finishes:

- tree edge `(u,v)` is a bridge iff `low[v] > discovery[u]`;
- non-root `u` is an articulation vertex iff some child satisfies
  `low[v] >= discovery[u]`;
- a DFS root is an articulation vertex iff it has more than one DFS-tree child.

The implementation uses an explicit DFS frame stack rather than recursive
low-link calls. This keeps auxiliary stack storage `O(V)` without relying on the
process call-stack depth.

After bridge discovery, a second iterative traversal skips every bridge edge and
labels the connected components. These labels are assigned by scanning seed
vertices from `0` upward, so the returned component ids are deterministic.

## Multigraph corner cases

A parallel edge has a distinct edge id. If one copy becomes the DFS-tree edge,
the other copy remains visible as a non-tree edge to the parent and lowers the
child's low-link value to the parent's discovery time. Consequently neither
copy is falsely reported as a bridge.

A self-loop only compares a vertex's low-link value with its own discovery time;
it cannot make an edge a bridge or create an articulation point.

## Complexity

Let `V` be the number of vertices and `E` the number of undirected edge copies
as added through `Graph::add_edge` (a parallel copy counts separately, a
self-loop counts once). Canonical edge extraction, low-link DFS, bridge output,
and bridge-component labeling are all linear:

- time: `O(V + E)`;
- auxiliary storage: `O(V + E)`.

No library bridge/articulation implementation is used.

## Verification

Focused exact-interface builds pass the repository warning policy under GCC and
Clang and also pass a real GCC ASan+UBSan build.

Deterministic cases cover:

- directed-input rejection;
- empty and singleton graphs;
- self-loops;
- paths, where every edge is a bridge and only internal vertices articulate;
- cycles and disconnected components; and
- parallel edges, including different ignored weights, preventing false
  bridges.

The randomized differential corpus contains 700 fixed-seed undirected
multigraphs with `0..9` vertices and up to 18 edge copies. The primary oracle is
structurally independent of low-link recurrence:

1. remove each edge copy in turn and rebuild connectivity to classify bridges;
2. remove each vertex in turn and rebuild connectivity to classify articulation
   vertices; and
3. remove the oracle bridges, rebuild connectivity, and compare the complete
   bridge-component partition.

The oracle therefore exercises deletion/reachability semantics rather than a
second Tarjan implementation.
