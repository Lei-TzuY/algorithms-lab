# Scope recovery: cactus-forest block decomposition

## Coverage decision

This recovery slice adds a structural graph class that was absent from live
`main@a81629fd1bf2db50b31c03f569a7ea8bb0baa582`: recognition and deterministic
block decomposition for cactus forests. Fresh code search returned no cactus
implementation and fresh branch lookup returned no `scope-recovery-cactus`
surface. The long-lived `scope-recovery-minimum-cycle-basis` branch is left
untouched.

This is not another low-link or block-cut wrapper. The subject is the
characterization that every vertex-biconnected edge block of a simple undirected
graph is either one bridge edge or one simple cycle exactly when every edge lies
on at most one simple cycle.

## Contract

`analyze_cactus_forest` accepts an undirected simple graph; disconnected inputs
are allowed and interpreted component-wise. Edge weights are ignored. Directed
graphs, self-loops, and parallel edges are rejected explicitly so the API does
not silently choose among incompatible multigraph cactus definitions. Empty and
isolated-vertex inputs are valid.

The result exposes:

- whether every connected component is a cactus;
- the number of connected components, including isolated vertices;
- a deterministic partition of every graph edge into vertex-biconnected blocks;
- each block classified as bridge, simple cycle, or complex/non-cactus;
- for cycle blocks, a replayable cycle vertex order starting at the smallest
  vertex and taking its smaller cycle neighbor first.

Blocks and block edges are normalized lexicographically, so insertion order and
edge weights do not change the reported structure.

## Structural invariant

Production first normalizes the simple undirected edge set, then runs an
iterative Tarjan DFS with an edge stack. `discovery[v]` is the first-visit time
and `low[v]` is the minimum discovery time reachable from the DFS subtree of
`v` using tree edges plus at most one back edge. When a child `v` satisfies
`low[v] >= discovery[parent[v]]`, edges are popped through the parent tree edge;
that maximal suffix is exactly one vertex-biconnected edge block.

A one-edge block is a bridge. A larger block is a simple cycle exactly when the
number of block edges equals the number of incident block vertices and every
block vertex has degree two inside the block. Any other block is complex, making
the graph non-cactus. Every original edge is popped exactly once, so the output
blocks form an exact edge partition.

The DFS is iterative; a long chain does not depend on recursive call-stack
depth. The implementation uses sorted normalized edges for deterministic output.
With the explicit simple-edge normalization used here, the conservative bound is
`O((V + E) log E)` time and `O(V + E)` auxiliary/output storage. The Tarjan
portion itself is linear after normalization.

## Independent verification

The randomized oracle does not reuse Tarjan low-link values, edge stacks, or
block classification. For each undirected edge `(u,v)`, it removes that edge and
independently enumerates simple `u`-to-`v` paths, stopping after two. Simple
cycles containing `(u,v)` are in one-to-one correspondence with those alternate
paths, so the graph is a cactus forest exactly when no edge has two such paths.

Focused candidate verification passes:

- GCC C++20 strict warnings-as-errors: 4/4 tests;
- Clang C++20 strict warnings-as-errors: 4/4 tests;
- GCC ASan+UBSan: 4/4 tests.

Evidence includes empty/disconnected forests, weighted-edge irrelevance, one
cycle, two cycles sharing one articulation vertex plus a bridge, a diamond
non-cactus block, directed/self-loop/parallel rejection, a 20,000-vertex chain,
and 600 fixed-seed random simple graphs with 0..9 vertices. Randomized checks
compare cactus recognition to the independent alternate-path oracle and also
verify exact edge partition, deterministic cycle witnesses, block kind
constraints, and connected-component counts.
