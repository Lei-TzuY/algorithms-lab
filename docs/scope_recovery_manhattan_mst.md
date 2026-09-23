# Scope recovery: exact 2D Manhattan minimum spanning tree

## Coverage decision

A fresh live audit at `main@60a2cb81f254141771b045a3752e732b53675463`
found zero open pull requests and zero open issues.

The repository already has general weighted-graph minimum spanning forests via
Kruskal and Prim, plus an online dynamic minimum-spanning-forest surface. It did
not contain a Manhattan / rectilinear / L1 geometric MST implementation,
occupied branch, or prior implementation PR.

This slice adds a different capability layer: reducing the complete geometric
L1 graph on a point set from Theta(n^2) edges to O(n) exact candidate edges,
then integrating that sparse graph with the existing first-principles Kruskal
implementation.

## Public contract

`ManhattanPoint2D` stores signed 32-bit coordinates.

`manhattan_minimum_spanning_tree(points)` treats every input position as a
distinct graph vertex, even when multiple vertices have identical coordinates.

The returned `MinimumSpanningForest` is in fact:

- empty with zero components for an empty point set;
- one component and zero edges for a singleton;
- one connected spanning tree with exactly `n-1` edges for every non-empty
  point set;
- exact under Manhattan distance
  `|x1-x2| + |y1-y2|`.

The function returns the existing graph layer's `WeightedEdge` and
`int64_t Weight` types. Final total-weight overflow therefore follows the
existing Kruskal checked-add contract.

## Coordinate bound

Coordinates deliberately use `int32_t`.

For any pair of 32-bit coordinates, one-axis absolute difference is at most
`2^32-1`; two-axis Manhattan distance is therefore at most
`2*(2^32-1) = 8,589,934,590`, which fits exactly in signed 64-bit weight.

Production widens coordinates to `int64_t` before subtraction, transformation,
or summation, so:

- subtraction cannot overflow;
- absolute pairwise coordinate differences cannot overflow;
- the four sweep transforms can negate safely;
- transformed `x+y` fits safely in signed 64-bit.

The overall tree total can still overflow `int64_t` for an unrealistically
large point set; that is intentionally detected by the existing MST layer
rather than silently wrapped.

## Duplicate-coordinate reduction

Coincident input points are distinct vertices but have zero pairwise L1
distance.

Production sorts vertices lexicographically by `(x,y,id)`, chooses one
representative for every coordinate class, and connects every other member of
the class directly to that representative with a zero-weight edge.

The octant sweep then operates only on coordinate representatives.

This is exact because any MST can connect all vertices of one coincident class
with zero total cost, contract that class to one representative, solve the MST
between distinct coordinate classes, then expand the zero-cost class tree.

It also avoids relying on ordered-map overwrite behavior to discover
zero-distance duplicate edges.

## Octant candidate theorem

For distinct coordinates, the complete L1 graph is not materialized.

For one transformed orientation, vertices are processed by increasing
`x+y`. A sweep map is keyed by `-y`.

When current vertex `p` encounters a stored vertex `q` satisfying the
orientation inequality

`x_p - x_q >= y_p - y_q`,

the edge `(p,q)` is emitted and `q` leaves that sweep frontier.

The sweep invariant is the standard Manhattan-MST octant argument: among
points lying in one directional octant of a point, only the nearest admissible
point is needed for some MST. If two candidates lie in the same octant, their
mutual Manhattan distance is no larger than the farther candidate's distance
to the anchor, so the cut/cycle exchange property permits discarding the
dominated complete-graph edge.

Swapping axes and alternating one-axis sign negation across four passes covers
the eight Manhattan octants. Every sweep vertex leaves the ordered map at most
once per pass, so each pass emits O(n) candidate edges.

The union of the four sweep edge sets therefore contains a Manhattan MST.

Production does not reimplement MST selection: it feeds those candidate edges,
plus duplicate-class zero edges, into
`kruskal_minimum_spanning_forest()`.

A postcondition requires one connected component and exactly `n-1` accepted
edges for every non-empty point set. Violating the candidate theorem therefore
fails loudly instead of returning a disconnected forest.

## Complexity boundary

Let `n` be the number of input vertices and `u <= n` the number of distinct
coordinate pairs.

- coordinate-class grouping: `O(n log n)`;
- four directional sorts: `O(u log u)`;
- ordered sweep-map work: `O(u log u)` per pass;
- candidate edges: `O(n + u)`, hence `O(n)`;
- sparse Kruskal: `O(n log n)`.

Overall time is `O(n log n)`; explicit storage is `O(n)` apart from the
existing sparse graph/MST structures.

These are asymptotic algorithmic bounds, not benchmark claims.

## Independent verification

The committed oracle deliberately does not use the geometric candidate
reduction.

For each small test point set it constructs the full complete graph containing
every pairwise Manhattan edge, then runs the repository's independent Prim MST
implementation.

Production uses:

- geometric octant reduction;
- sparse candidate graph;
- Kruskal.

The oracle uses:

- no octant reduction;
- all `n(n-1)/2` geometric edges;
- Prim.

Thus equality of total weights exercises both the geometric theorem and a
different MST selection algorithm.

Committed coverage includes:

- empty and singleton sets;
- several coincident coordinate classes;
- a square;
- collinear points in unsorted input order;
- all four `int32_t` corner extremes;
- 1,600 fixed-seed random point sets of size 0..10 with coordinates in
  `[-15,15]`, intentionally producing duplicates;
- independent tree-shape replay checking edge endpoint bounds, exact geometric
  edge weights, acyclicity, connectivity, edge count, and recomputed total.

A supplementary model-level differential check compared the same four-sweep
candidate rule with complete-graph MSTs on 45,000 additional random point sets
and found zero mismatch. That is supporting evidence only; repository
GCC/Clang/sanitizer CI remains the integration gate.

## Non-claims

This slice does not claim:

- Euclidean MST;
- dimensions above two;
- dynamic point insertion/deletion;
- nearest-neighbor queries;
- Delaunay or Voronoi construction;
- deterministic uniqueness of the returned MST when multiple MSTs exist;
- benchmark-backed speedup over complete-graph construction for tiny inputs;
- support for coordinate types wider than the stated signed 32-bit contract.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/geometry/manhattan_mst.hpp`;
- `tests/test_manhattan_mst_cases.hpp`;
- `docs/scope_recovery_manhattan_mst.md`.

No CMake update is required because the repository auto-discovers
`tests/test_*_cases.hpp`.

No README, historical ROADMAP, workflow, benchmark, frozen compiler/backend, or
temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `60a2cb81f254141771b045a3752e732b53675463`.
