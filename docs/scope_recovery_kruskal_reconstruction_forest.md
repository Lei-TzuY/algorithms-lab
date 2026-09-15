# Scope recovery — Kruskal reconstruction forest threshold connectivity

## Coverage decision

A fresh live audit after the merged quotient-filter checkpoint found no Kruskal
reconstruction tree/forest, minimax bottleneck-threshold index, or component-at-
weight-threshold query surface in default-branch code, pull-request history, or
branch names. The sealed minimum-spanning-forest implementations compute one
optimum forest, but they do not materialize the monotone merge hierarchy needed
for repeated threshold-connectivity queries.

Nearby live recovery branches were deliberately left alone: HyperLogLog,
partially-persistent DSU, and order-maintenance already occupy their respective
surfaces. Full SPQR remains a real but substantially larger gap and is not
collapsed into this bounded recovery slice. Prospective work follows
`docs/scope_recovery_after_phase69.md`; the historical compiler/backend ROADMAP
remains untouched.

## Production contract

`KruskalReconstructionForest` accepts the repository's undirected weighted
multigraph and builds one immutable binary merge forest.

- original graph vertices are leaves `0..V-1`;
- self-loops are ignored because they never change connectivity;
- parallel edge copies are preserved during the weight-ordered scan;
- every successful sealed-DSU union creates one internal merge node carrying
  that edge weight and the merged component size;
- signed weights use the complete `int64_t` domain, including `INT64_MIN` and
  `INT64_MAX`;
- disconnected input remains a reconstruction forest rather than being forced
  into one artificial root;
- equal-weight unions may create several binary nodes at the same threshold,
  but public connectivity/component semantics do not depend on their internal
  refinement order.

The public query surface is:

- `minimax_connectivity(u,v)`: whether the vertices are eventually connected
  and, for distinct connected vertices, the minimum edge-weight threshold at
  which they become connected;
- `connected_at_most(u,v,t)`: connectivity in the subgraph containing exactly
  edge copies of weight at most `t`;
- `component_size_at_most(v,t)`: size of `v`'s component in that thresholded
  subgraph;
- `reconstruction_node_count()` as a small structural diagnostic.

For `u == v`, the zero-edge path is connected under every threshold, so the
result marks `connected=true` with no finite bottleneck value. `connected`
distinguishes this from a genuinely disconnected pair.

## Invariant / proof obligation

Process non-loop edges in nondecreasing weight order. After all edges of weight
at most `t` have been processed, DSU classes are exactly the connected
components of the original graph restricted to edges of weight at most `t`:
every processed edge performs its required union, while DSU never joins
components without such an edge.

A successful union creates a parent whose leaf set is exactly the unioned DSU
class and whose merge weight is the current edge weight. Because edges are
processed monotonically, merge weights are nondecreasing on every leaf-to-root
route. Consequently:

1. the highest ancestor of leaf `v` whose merge weight is at most `t` contains
   exactly `v`'s threshold-`t` component;
2. for two distinct leaves in the same final tree, their LCA is the first merge
   node whose leaf set contains both; its weight is therefore the least `t` for
   which the vertices are connected;
3. that least connectivity threshold is equivalently the minimum possible
   maximum edge weight over all paths between the two vertices.

Binary lifting over the merge forest makes both ancestor threshold climbing and
LCA logarithmic. Equal-weight tie order can alter only the binary refinement
inside one weight level; all nodes in that refinement carry the same threshold,
so the three public statements above remain unchanged.

The classical Kruskal/DSU connectivity argument is the proof obligation. Tests
are implementation evidence rather than a proof of the theorem.

## Independent verification

Focused final repo-native bytes pass:

- GCC C++20 repository strict warnings-as-errors;
- Clang C++20 repository strict warnings-as-errors;
- actual GCC ASan+UBSan with fail-fast/leak detection.

Deterministic evidence covers empty/isolated graphs, directed rejection,
invalid vertices, self-loops, dominated parallel copies, equal-weight insertion
order, disconnected components, signed thresholds, and exact `INT64_MIN` /
`INT64_MAX` boundaries.

Primary randomized evidence uses 500 fixed-seed undirected multigraphs with
0..8 vertices, self-loops, parallel copies, and weights in `[-20,20]`. The
oracle never builds an MST, DSU, or reconstruction tree. For every tested
threshold it performs plain BFS in the original graph using only edges with
weight at most that threshold, then checks every production connectivity answer
and every component size. For every vertex pair it scans the distinct input
weights in ascending order and takes the first threshold whose BFS connects the
pair; production minimax thresholds must match exactly.

## Complexity / non-claims

Let `E` count logical non-loop undirected edge copies. Construction sorts those
copies and builds at most `2V-1` reconstruction nodes, for
`O(E log(E+1) + V log(V+1))` direct work plus sealed DSU operations. Temporary
construction storage is `O(E + V log(V+1))`; resident forest/jump-table storage
is `O(V log(V+1))`. Each public threshold/LCA query is `O(log(V+1))`.

No minimum-spanning-tree edge witness, explicit minimax path reconstruction,
dynamic edge update, directed threshold-connectivity semantics, canonical
isomorphism-independent equal-weight merge topology, sublogarithmic query
bound, or benchmark-speedup claim is implied.
