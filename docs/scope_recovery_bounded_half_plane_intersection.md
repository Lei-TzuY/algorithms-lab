# Scope recovery: bounded-exact half-plane intersection

## Coverage decision

A fresh live audit at `main@1466a377a9d924268cbeb82f2fbdc81e287aee4f`
found no half-plane-intersection / convex-clipping production surface in the
default branch, pull-request history, or branch namespace. The current main CI
run `35047389187` is completed/success, and there were no open PRs or issues
before promotion.

This follows `docs/scope_recovery_after_phase69.md`. The historical compiler /
backend ROADMAP remains prospectively frozen. Several initially plausible
recovery candidates were explicitly rejected because they already exist:
Delaunay triangulation, suffix automata, exact planarity, general CNF SAT,
Dreyfus-Wagner Steiner tree, bitwise Walsh-Hadamard convolution, Held-Karp
Hamiltonian cycles, treewidth/pathwidth, and Yen k-shortest paths.

## Production contract

`bounded_half_plane_intersection(half_planes, box_bound)` intersects closed
integer half-planes

`a*x + b*y <= c`

with the explicit square `[-box_bound, box_bound]^2`.

- every public coefficient and `box_bound` is bounded in magnitude by 500;
- a half-plane normal must be nonzero;
- at most 128 caller half-planes are accepted by this direct baseline;
- results are exact rational extreme points represented by one positive common
  denominator `(x_numerator, y_numerator, denominator)`;
- points are normalized, unique, and sorted lexicographically by exact rational
  `(x,y)` value;
- empty intersections return no vertices;
- one-dimensional / zero-dimensional intersections return the exact segment
  endpoints or singleton point;
- duplicate, redundant, parallel, and positively scaled inequalities are valid;
- this API intentionally returns the canonical extreme-point set, not a cyclic
  polygon boundary, area, an unbounded-region representation, or floating-point
  approximations.

The mandatory box is part of the semantic contract: this slice converts one
bounded H-representation to its exact finite V-representation rather than
pretending to solve arbitrary unbounded polyhedra.

## Exact arithmetic boundary

Let `M=500`. For two accepted boundary equations, Cramer's rule gives

- `|det| <= 2 M^2 = 500,000`;
- `|x_num|, |y_num| <= 2 M^2 = 500,000`.

The implementation makes denominators positive and divides the homogeneous
triple by its common gcd. Feasibility replay computes

`a*x_num + b*y_num <= c*den`,

whose two left products and right product remain far inside signed-64 range.
Exact lexicographic comparison cross-multiplies normalized numerators and
positive denominators; each such product is at most `(2M^2)^2 = 2.5e11`.
No unchecked wider intermediate, floating epsilon, non-standard `__int128`, or
arbitrary-precision dependency is required by the public domain.

## Vertex-enumeration proof obligation

After adding the four box inequalities, the feasible set is a bounded convex
polyhedron in two dimensions. Every extreme point of a non-empty bounded
2D polyhedron is the unique intersection of two linearly independent active
boundary equations. Conversely, every pairwise boundary intersection that
satisfies all inequalities has two independent active constraints and is an
extreme point (with coincident/parallel lines excluded by zero determinant).

Production therefore:

1. appends the four exact box half-planes;
2. enumerates every pair of boundary lines;
3. computes the normalized exact rational intersection for nonparallel pairs;
4. replays every inequality at that point;
5. inserts every feasible point into an exact rational ordered set.

The same argument covers lower-dimensional results: a feasible segment has its
endpoints among boundary-line intersections, and a singleton feasible region is
an intersection point. If no feasible pairwise intersection exists inside the
bounded box, the feasible set is empty.

Correctness relies on this standard bounded-polyhedron vertex characterization;
finite tests are implementation evidence rather than a theorem proof.

## Independent verification

The focused final candidate passed:

- GCC C++20 with repository strict warnings-as-errors: 4/4;
- Clang C++20 with the same strict warnings: 4/4;
- actual GCC ASan+UBSan with fail-fast/leak detection: 4/4.

Deterministic evidence covers the full box, a rational triangle, exact line and
point degeneracies, an empty intersection, redundant/scaled inequalities,
parameter validation, and the public coefficient boundary.

The primary randomized oracle deliberately does **not** enumerate line-pair
intersections. It uses 700 fixed-seed bounded instances whose normals are drawn
from the axis/diagonal primitive family. In that domain every true vertex lies on
the half-integer grid. The oracle enumerates every half-grid point in the box,
keeps exactly the feasible points, and independently takes their convex hull.
Its extreme-point set must equal production exactly. Every production point is
also replayed directly against every original inequality and the box; periodic
constraint-order shuffles require byte-for-byte deterministic results.

## Complexity and non-claims

For `H = caller_half_planes + 4`, production examines `O(H^2)` boundary pairs
and replays `O(H)` inequalities per nonparallel pair, for `O(H^3)` direct time.
The exact candidate set is `O(H^2)` and ordered-set insertion contributes an
additional `O(H^2 log H)` comparison term. Resident/output state is `O(H^2)`.

This is deliberately **not** the classical deque-based `O(H log H)` half-plane
intersection algorithm. It makes no unbounded-region, arbitrary-precision,
large-coordinate, floating-point robustness, polygon-clipping throughput,
linear-programming, 3D half-space, or benchmark-performance claim.
