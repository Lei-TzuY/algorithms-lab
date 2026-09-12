# Scope recovery: DSU-on-tree subtree frequency aggregation

## Coverage decision

This recovery slice adds a proof model that was absent from the live repository at
`main@d1347389b2365662c97bb33027d2db8e4ca964b7`: DSU-on-tree ("sack")
small-to-large subtree aggregation. A fresh code search found no DSU-on-tree
implementation and a fresh branch search found no occupied DSU-on-tree surface.
The long-lived `scope-recovery-minimum-cycle-basis` branch is intentionally left
untouched.

This is not another heavy-light, centroid, link-cut, or generic DSU variant. The
subject is the heavy-child retention invariant that makes repeated subtree
frequency aggregation amortized rather than rebuilding every subtree from
scratch.

## Contract

`DsuOnTreeFrequencyIndex` snapshots one signed 64-bit label per vertex of a
non-empty undirected simple connected tree and a chosen root. Edge weights are
ignored deliberately. Construction rejects directed graphs, self-loops, parallel
edges, cycles, disconnected inputs, label-count mismatches, and invalid roots.

For every vertex the immutable index exposes:

- rooted parent and subtree size;
- deterministic heavy child (largest child subtree, smaller vertex id on ties);
- subtree distinct-label count;
- maximum label frequency in the subtree;
- deterministic mode label (smallest signed label among maximum-frequency ties).

The implementation is iterative, including rooting and the DSU-on-tree event
schedule, so a long chain does not rely on recursive call-stack depth.

## DSU-on-tree invariant

Each vertex chooses its largest child subtree as heavy. Light children are solved
with `keep=false` and cleared. The heavy child is then solved with `keep=true`;
its frequency state remains resident while every light subtree is added back,
then the current vertex is added and the answer is recorded. A non-kept subtree
is removed only when the active state is exactly that subtree, restoring an empty
frequency state.

The Euler/preorder interval `[tin[v], tout[v])` is exactly the rooted subtree of
`v`, so adding or clearing one subtree is a linear scan of that interval. The
frequency table is indexed by a deterministic coordinate compression of the
snapshotted signed labels.

## Amortized bound

Whenever a vertex is re-added because it lies in a light child's subtree, its
containing subtree size at least doubles before it can cross another light edge
toward the root. Therefore any vertex crosses at most `floor(log2 n)` light
edges. Including its own retained processing, each vertex is added at most
`floor(log2 n) + 1` times.

The implementation counts every add operation and checks the executable bound

`add_operations <= n * (floor(log2 n) + 1)`.

Sorting labels for coordinate compression costs `O(n log n)`; the DSU-on-tree
aggregation itself performs `O(n log n)` add/remove work with `O(n)` structural
and frequency state. No worst-case `O(1)` subtree-query preprocessing claim or
generic online-update claim is made: this is an immutable offline subtree
aggregation index.

## Verification

Focused candidate verification uses repository-style strict warnings and the
actual sanitizers:

- GCC C++20 strict warnings-as-errors: 4/4 tests pass;
- Clang C++20 strict warnings-as-errors: 4/4 tests pass;
- GCC ASan+UBSan: 4/4 tests pass.

Evidence includes:

- singleton behavior and signed `int64_t` label boundaries;
- deterministic heavy-child and smallest-mode-label ties;
- rejection of directed, self-loop, parallel-edge, cyclic, disconnected,
  wrong-label-count, and invalid-root inputs;
- a 4096-vertex chain proving the implementation does not depend on recursive
  tree depth;
- 500 fixed-seed random trees with 1..128 vertices, arbitrary roots, random
  signed labels, and ignored random edge weights;
- independent naïve rooted-subtree traversal with `std::map` frequency counts
  for every vertex;
- independent checks of parent, subtree size, heavy-child selection, complete
  frequency summaries, and the executable amortized add bound.

The naïve oracle does not reuse the DSU-on-tree event order, heavy-state
retention, coordinate compression, or frequency table.
