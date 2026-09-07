# Phase 7 — selected advanced foundations

Phase 7 opens with computational geometry because it introduces exact geometric predicates and output canonicalization rather than another graph or sequence recurrence.

## Exact bounded-integer 2D predicates

`Point2i` stores signed 32-bit coordinates, but public geometry operations deliberately accept only the closed coordinate domain `[-1,000,000,000, 1,000,000,000]`. Any point outside that domain is rejected with `std::out_of_range`.

For accepted points, a coordinate difference has magnitude at most `2,000,000,000`. Each product in the 2D determinant therefore has magnitude at most `4e18`, and their difference has magnitude at most `8e18`, which is representable by signed `int64_t`. Orientation is consequently exact for the documented domain without floating-point epsilon, unchecked overflow, non-standard integer extensions, or an external arbitrary-precision dependency.

`orientation(a,b,c)` classifies the sign of that determinant. `on_segment(a,b,p)` requires exact collinearity plus inclusive coordinate bounds. `segments_intersect(a,b,c,d)` treats proper crossings, shared endpoints, degenerate point segments, and collinear overlap as intersections.

## Deterministic convex hull

`convex_hull` uses Andrew's monotone-chain construction. Input points are validated, lexicographically sorted, and deduplicated. While constructing lower and upper chains, a non-left turn removes the middle candidate; this excludes interior collinear boundary points and preserves only extreme vertices.

The returned hull contract is deterministic:

- no duplicate vertices,
- counterclockwise order when at least three vertices exist,
- the lexicographically smallest hull point is first,
- interior collinear boundary points are excluded,
- all-collinear input returns the two extreme unique points,
- empty/singleton input is returned canonically.

Sorting dominates the runtime, so hull construction is `O(n log n)` time and `O(n)` auxiliary/output storage. Orientation, on-segment, and segment intersection are `O(1)`.

## Independent verification

Deterministic tests cover clockwise/counterclockwise/collinear orientation, proper crossing, shared endpoints, collinear overlap/disjoint segments, degenerate point segments, duplicate points, an interior point, all-collinear input, the exact coordinate boundary, and rejection outside the exact domain.

Fixed-seed randomized verification generates 500 point multisets with up to 25 points. Production monotone-chain hulls are compared exactly with a test-only Jarvis-march/gift-wrapping implementation whose control flow does not reuse the production lower/upper-chain recurrence. The tests additionally verify strict counterclockwise turns and that every generated point lies on or to the left of every oriented hull edge.

A separate 5,000-round fixed-seed segment sample checks endpoint reversal and segment-order symmetry. These are property checks, not a claim that symmetry alone is an independent segment-intersection optimum oracle.

## Frontier

This is the first executable Phase-7 slice. Computational-geometry foundations on the roadmap are complete at this bounded exact-predicate/hull scope; Phase 7 remains active. Number theory is the next ordered frontier.
