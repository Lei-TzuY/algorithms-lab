# Scope recovery: replayable minimum enclosing circle

## Coverage decision

A fresh live default-branch, PR-history, and branch audit found no minimum / smallest enclosing-circle capability. This slice deliberately leaves the immediately preceding dense conjugate-gradient and Householder-QR numerical-linear-algebra work, and does not touch the occupied minimum-cycle-basis, BFPRT-selection, or subset-convolution branches.

The existing geometry foundation supplies bounded exact integer predicates and many exact point/convex operations, but no enclosing-disk optimization. Half-plane intersection was also considered; the repository already contains exact rational linear programming, and a complete unbounded half-plane polygon representation would make a single recovery PR materially broader. Minimum enclosing circle gives a tighter independent proof model and reviewable boundary.

## Production contract

`minimum_enclosing_circle(points, seed)` accepts `Point2i` inputs in the existing exact geometry coordinate domain `[-1e9,1e9]` and returns `std::nullopt` for an empty set. A non-empty result contains a numerical `long double` center, squared radius, and a canonical lexicographically sorted support witness of one to three input points.

The algorithm copies and replayably shuffles input points using the standard `mt19937_64` engine plus repository-defined rejection sampling rather than `std::shuffle` / `uniform_int_distribution` mapping. The incremental support-circle construction then maintains the minimum disk of the processed prefix; when a new point lies outside, one- and two-boundary subproblems are rebuilt and the two-point boundary scan chooses the extremal circumcircle on each side of the boundary chord.

Same input order + same seed therefore replays the same shuffled order and support decisions. Randomization is used for expected running time, not for correctness probability.

## Exactness and proof boundary

Integer-domain validation and collinearity tests are exact signed-64-bit calculations under the existing `[-1e9,1e9]` geometry bound. Circle centers, radii, containment, and circumcenters are `long double` numerical calculations. The implementation uses scale-aware machine-epsilon containment slack and fails closed if a required circumcircle becomes non-finite.

The API therefore does **not** claim an exact rational circle, platform-independent bitwise floating output, a backward-error theorem, or exact boundary classification beyond the integer collinearity predicate. Tests provide bounded numerical evidence.

The mathematical correctness obligation is the standard minimum-enclosing-disk support theorem: every finite planar point set has a unique minimum disk supported by at most three input points; if two support points determine the solution they form a diameter, otherwise three non-collinear support points determine the circumcircle. The randomized incremental algorithm preserves the processed-prefix optimum by rebuilding only when the next point violates the current disk.

## Verification

Focused pre-upload evidence uses repository strict-warning flags with GCC and Clang plus a real GCC ASan+UBSan build. Deterministic cases cover empty/singleton, obtuse and acute triangles, collinear inputs, duplicates, exact coordinate boundaries, domain rejection, seed replay, alternate seeds, and reversed input order.

The primary small-instance oracle is structurally independent of the incremental recurrence: it enumerates every candidate disk supported by one point, every pair diameter, and every non-collinear triple circumcircle, keeps candidates containing all input points, and chooses the minimum radius. Fixed-seed randomized point multisets are compared against that exhaustive support oracle, while every production support point is replayed as an input boundary point.

## Complexity / non-claims

For a uniformly replay-shuffled order, the classical randomized incremental algorithm has expected `O(n)` time. This direct implementation retains the standard nested fallback structure and honestly allows `O(n^3)` worst-case work. It stores an `O(n)` shuffled copy and `O(1)` circle/support state beyond the input copy.

No deterministic linear-time bound, exact rational geometry, minimum enclosing ellipse/ball, weighted disk, streaming update, or benchmark speedup is claimed.
