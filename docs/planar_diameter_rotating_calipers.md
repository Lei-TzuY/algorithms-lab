# Exact planar diameter with rotating calipers

## Contract

`planar_diameter` accepts the repository's existing bounded-integer `Point2i`
domain `[-1'000'000'000, 1'000'000'000]^2` and returns an exact farthest pair
under squared Euclidean distance. Fewer than two input points returns
`std::nullopt`. Duplicate coordinates are valid; if every input point is the
same coordinate, the returned pair contains that coordinate twice and has
squared distance zero.

Endpoints are canonicalized lexicographically. If multiple pairs realize the
same diameter, the lexicographically smallest canonical pair is returned, so
input order cannot change the witness.

The existing sealed `convex_hull` implementation performs coordinate validation
and removes duplicate/interior/collinear non-extreme points before the calipers
scan.

## Exact arithmetic

For the accepted coordinate domain, every coordinate difference has magnitude
at most `2e9`. Therefore both squared Euclidean distance and the orientation /
twice-triangle-area determinant are bounded by `8e18`, strictly below
`INT64_MAX`. The production path uses `std::int64_t` throughout and needs no
floating-point comparison, epsilon, square root, or non-standard wide integer.

## Hull reduction

A Euclidean farthest pair can be chosen from convex-hull vertices. Any point
strictly inside the convex hull cannot be a unique maximizer of distance from
another point; moving along a supporting direction reaches a boundary extreme
without decreasing that linear projection. The all-collinear case is reduced by
the sealed hull implementation to its two extremes.

This is the first cross-check in the implementation: the algorithm does not scan
all original points after the hull is built.

## Rotating-calipers invariant

Let the hull contain `h >= 3` vertices in counterclockwise order. For each
directed hull edge `(i, i+1)`, maintain an `opposite` vertex whose signed twice
triangle area with that edge is locally maximal. Convexity makes these heights
unimodal around the polygon and the maximizing antipodal index advances
monotonically as the edge rotates. Thus the opposite pointer performs only a
linear number of advances across the full scan.

At each edge, both edge endpoints are paired with the current antipodal vertex.
When the next antipodal vertex has equal area, both plateau endpoints are also
considered. This explicitly preserves deterministic tie coverage rather than
assuming the diameter is unique.

The rotating-calipers theorem states that a convex polygon's diameter is
realized by an antipodal pair enumerated by this process. That theorem is a
proof obligation; randomized tests are evidence for the implementation, not the
source of the theorem.

## Complexity

For `n` input points and `h` hull vertices:

- sealed convex-hull construction: `O(n log n)` time;
- rotating-calipers scan: `O(h)` time;
- total: `O(n log n)` time;
- hull/result auxiliary storage: `O(n)`;
- no benchmark timing is used as asymptotic evidence.

The implementation deliberately does not claim linear time for arbitrary
unsorted input.

## Verification

Deterministic coverage includes empty/singleton input, all-identical duplicates,
all-collinear points, interior-point elimination, a square with multiple diameter
pairs, reversed input order, exact coordinate extremes, and out-of-domain
rejection.

Every nontrivial subset of a `3 x 3` integer grid is compared against an
independent exhaustive all-pairs oracle. A fixed-seed randomized corpus adds
1,500 point multisets with 2-80 points; the full canonical witness and squared
distance are compared against the same `O(n^2)` oracle, then the input is
shuffled and replayed to verify input-order independence.

Focused pre-upload builds pass repository-equivalent GCC strict warnings, Clang
strict warnings, and actual GCC ASan+UBSan. The exact PR-head full repository CI
matrix remains the integration gate.
