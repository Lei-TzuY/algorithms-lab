# Scope recovery — centroid decomposition with dynamic nearest-active queries

## Scope

This recovered slice adds a centroid-decomposition index for a strict undirected
tree and a dynamic set of active vertices. Queries return the active vertex with
minimum **edge-count distance** from a requested vertex; ties choose the smallest
vertex id. Edge weights in `Graph` are intentionally ignored.

The input contract is deliberately strict: an empty graph is accepted, while a
non-empty input must be undirected, connected, acyclic, loop-free, and free of
parallel edges. Invalid vertices and non-tree graphs are rejected rather than
silently converted to a spanning tree.

## Decomposition invariant

For a current tree component of size `s`, the chosen centroid is the smallest
vertex id whose removal leaves every remaining connected component with size at
most `floor(s / 2)`. The centroid becomes the parent of recursively chosen
centroids from those remaining components.

Every original vertex belongs to exactly one active component at each
centroid-decomposition level. Component collection, subtree-size computation,
and distance enumeration therefore touch each vertex only `O(log V)` times over
the whole construction. Large component traversals are iterative; only the
balanced decomposition itself recurses, so its recursion depth is `O(log V)`.
The implementation reuses scratch arrays and resets only vertices in the current
component, avoiding a hidden `O(V)` reinitialization at every recursive node.

For every original vertex `x`, the index stores one entry `(c, dist(x,c))` for
each centroid ancestor `c` of `x`. This is the structural basis for both the
space bound and dynamic queries.

## Active-state invariant

For each centroid `c`, its active bucket contains exactly one pair
`(dist(c,x), x)` for every currently active vertex `x` whose centroid path
contains `c`. Pairs are ordered first by distance and then by vertex id, so the
bucket minimum is deterministic. Activation inserts one pair along every
centroid ancestor; deactivation removes those exact pairs. Repeated activation
or deactivation is idempotent and reports that no state change occurred.

`std::multiset` is used as the supporting ordered container. The educational
subject of this slice is centroid decomposition and its dynamic overlay, not a
second implementation of a balanced search tree.

## Query correctness obligation

Consider query vertex `q` and any active vertex `x`. Their centroid-ancestor
chains share at least the decomposition root. Let `c` be their lowest common
centroid ancestor. At the decomposition step that selected `c`, either one of
`q` or `x` is `c`, or they lie in different components after removing `c`.
Consequently the unique original-tree path from `q` to `x` passes through `c`,
and

`dist(q,x) = dist(q,c) + dist(c,x)`.

The query scans every centroid ancestor of `q`. At each such centroid it combines
`dist(q,c)` with that centroid bucket's minimum active distance. Triangle
inequality means any such sum is never smaller than the true distance to the
chosen active vertex; the shared separator `c` above supplies an exact sum for
the true nearest candidate. Taking the minimum over all scanned ancestors is
therefore exact. Equal distances are resolved by smallest active vertex id both
inside buckets and in the final comparison.

## Complexity

For `V > 0`:

- construction: `O(V log V)` time and `O(V log V)` stored centroid-path state;
- `activate` / `deactivate`: `O(log^2 V)` worst-case with `O(log V)` centroid
  ancestors and logarithmic multiset updates;
- `nearest_active`: `O(log V)`, because each centroid bucket exposes its minimum
  in constant time;
- `is_active`: `O(1)` after bounds validation.

The copied original-tree adjacency uses `O(V)` additional storage. These bounds
do not claim weighted-tree distance support or a dynamic topology; the original
tree is immutable after construction.

## Verification

Deterministic tests cover empty input, path/tie semantics, idempotent toggles,
ignored edge weights, a 4096-vertex chain, the centroid-parent witness, and
rejection of directed, self-loop, parallel-edge, cyclic, and disconnected
inputs.

A fixed-seed randomized differential suite builds 300 trees with 1–60 vertices.
For each tree it executes 180 mixed activate/deactivate/query operations and then
queries every vertex. Every query is compared with an independent oracle that
runs breadth-first search on the original tree and scans the active set. The
oracle does not use centroid decomposition, centroid ancestors, or centroid
buckets.

Focused builds pass GCC and Clang with the repository strict-warning set, plus a
real GCC AddressSanitizer + UndefinedBehaviorSanitizer build. Full-repository CI
remains the integration gate before this slice can be merged.
