# Exact closest pair of planar points

## Contract

`closest_pair` operates on the existing `Point2i` domain
`[-1'000'000'000, 1'000'000'000]^2`. Inputs with fewer than two points return
`std::nullopt`. Duplicate points are valid and yield squared distance zero.
Endpoints are canonicalized lexicographically; ties between different closest
pairs are resolved by the lexicographic pair key, so input order does not affect
the witness.

Squared Euclidean distance is exact in `std::int64_t`: each coordinate
difference has absolute value at most `2e9`, so `dx^2 + dy^2 <= 8e18 < INT64_MAX`.
No floating-point square root, epsilon comparison, or non-standard wide integer
is required.

## Divide-and-conquer invariant

The top-level input is sorted by `(x,y)`. A recursion over `[begin,end)` starts
with that interval in x-order. Before returning it has:

1. found the best canonical pair entirely inside the interval; and
2. reordered the interval by `(y,x)` for its parent's linear merge/strip step.

For a non-base interval, the x-coordinate of the split is saved before either
child mutates its order. Both children are solved recursively and then merged by
y. Only points whose squared horizontal distance from the split can still match
or improve the current best enter the strip. Because the strip is y-sorted, the
inner scan stops as soon as squared vertical separation is larger than the
current best distance.

After duplicate points are handled up front, the current best distance is
strictly positive. Standard closest-pair packing then bounds the number of strip
neighbors that can survive the y-distance test by a constant, so each recursion
level performs linear merge/strip work.

## Complexity

- initial x-sort: `O(n log n)`;
- recursive merge/strip work: `O(n log n)` total;
- auxiliary vectors: `O(n)`;
- recursion stack: `O(log n)`.

The implementation does not claim linear time and does not hide a quadratic
strip scan behind a benchmark claim.

## Verification

Deterministic tests cover empty/singleton input, duplicate points, canonical tie
breaking, reversed input order, collinear points, a cross-split nearest pair,
regular grids, exact coordinate extremes, and out-of-domain rejection.

A fixed-seed randomized suite generates 1,500 point multisets of up to 64 points
and compares the full returned result—not only the distance—against an
independent `O(n^2)` pair enumeration oracle. Shuffled copies are also checked to
prove input-order-independent witness semantics.
