# Scope recovery: exact static 2D KD-tree nearest-neighbor index

## Coverage decision

A fresh live-code and pull-request-history audit after the first-principles B-tree
checkpoint found no KD-tree or spatial partition nearest-neighbor index. This
slice deliberately changes proof model again instead of adding another ordered
set, string index, graph variant, or frozen compiler/backend continuation.

## Capability

`algorithms::geometry::KdTree2D` builds an immutable spatial index over the
existing bounded-integer `Point2i` domain `[-1e9, 1e9]^2`.

- constructor input is deduplicated by exact coordinate equality;
- `nearest(query)` returns the exact nearest stored coordinate by squared
  Euclidean distance;
- equal-distance ties return the lexicographically smallest coordinate;
- empty indexes return `std::nullopt`;
- construction and queries reject coordinates outside the established exact
  geometry domain;
- each result exposes visited-node and pruned-subtree diagnostics;
- `valid_structure()` replays split, bounding-box, reachability, and depth-axis
  invariants.

## Structural proof obligations

Depth `d` splits on axis `d mod 2`. Each node stores one unique point; every
left-subtree point is strictly earlier than the pivot in that axis' total order,
and every right-subtree point is strictly later. The stored rectangle is the
exact coordinate bounding box of the complete subtree.

For a query, squared distance to a subtree bounding box is a lower bound on the
squared distance to every point in that subtree. A subtree is therefore pruned
only when this lower bound is strictly larger than the best exact point distance
already observed. Equality is still explored so the lexicographic tie rule
cannot be pruned away. Searching both non-pruned children and the current pivot
therefore preserves the exact optimum.

The accepted coordinate domain keeps every point distance and bounding-box lower
bound within signed `int64_t`: the largest squared distance is `8e18`.

## Complexity and non-claims

This educational baseline sorts each recursive subrange by the active axis and
chooses its median. The resulting tree height is `O(log n)` and construction is
conservatively `O(n log^2 n)` time with `O(n)` resident storage plus recursive
stack. Branch-and-bound nearest-neighbor queries are data dependent and remain
`O(n)` in the worst case; no expected-logarithmic or dimensional-distribution
claim is made. `valid_structure()` intentionally favors diagnostic clarity and
may do `O(n log n)` work on the balanced tree.

This is a static exact 2D index. It does not claim dynamic insertion/deletion,
k-nearest neighbors, range reporting, higher dimensions, approximate search,
cache-optimal layout, or universal speedup over a direct scan.

## Verification

Deterministic tests cover empty and duplicate input, coordinate validation,
lexicographic equal-distance ties, exact boundary arithmetic, observable subtree
pruning, balanced height, repeated-query determinism, and shuffled-input
semantic invariance.

The primary differential corpus builds 700 fixed-seed point multisets with up to
80 inputs and executes 80 queries per tree. Every answer is compared with an
independent full scan over deduplicated points; structure validity is replayed
for every built index. Focused GCC strict-warning, Clang strict-warning, and
actual GCC ASan+UBSan builds are required before upload.
