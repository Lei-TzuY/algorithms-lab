# Scope recovery — exact bounded convex Minkowski sum

## Coverage decision

After the Dulmage-Mendelsohn bipartite decomposition reached merged `main`, a fresh
live code, pull-request, and branch audit found no Minkowski-sum production API or
prior recovery slice. The choice deliberately leaves the recent dynamic-graph and
matching streak: this slice returns to exact computational geometry with a different
proof model, namely angular merging of convex-polygon edge sequences.

The Phase-45–69 compiler/backend surface remains frozen under
`docs/scope_recovery_after_phase69.md`. Historical ROADMAP phases are not extended.

## Production contract

`convex_minkowski_sum(left, right)` accepts two finite sets of integer points and
returns the canonical convex hull of `conv(left) + conv(right)`:

- either empty input gives the empty sum;
- duplicate and interior input points are accepted and canonicalized through the
  sealed `convex_hull` implementation;
- output is unique hull vertices in counterclockwise order, beginning at the
  lexicographically smallest vertex, with interior collinear boundary points removed;
- point and segment degeneracies are supported explicitly;
- the operation is deterministic and commutative at the canonical output level.

This API computes the convex Minkowski sum of the input point sets. It does not
represent non-convex polygon interiors, holes, or polygon Boolean operations.

## Exactness / representability boundary

Each input coordinate must lie in `[-500,000,000, 500,000,000]`. This is stricter
than the repository's general `Point2i` predicate domain on purpose: every output
coordinate is the sum of one left and one right coordinate and therefore lies in
`[-1,000,000,000, 1,000,000,000]`, the sealed exact geometry domain.

For the proper-polygon merge, each input edge component has magnitude at most
`1,000,000,000`. Comparing two edge directions uses the exact signed 64-bit cross
product `ax*by - ay*bx`, whose magnitude is at most `2e18`. Equal-angle edge vectors
may be added componentwise to at most `2e9`. The running output vertex is checked
against the sealed `+-1e9` domain before conversion back to `Point2i`.

Inputs outside the composable domain fail with `std::out_of_range`; impossible
representability violations during the edge walk fail rather than wrap.

## Edge-angle merge obligation

Both point sets are first reduced to canonical convex hulls. For two proper polygons
(three or more hull vertices), each hull is rotated to its lowest-`y`, then lowest-`x`
vertex. Its counterclockwise boundary edges then form a cyclic polar-angle sequence
with the wrap placed between the final and first edge.

Production merges those two edge sequences. The comparator uses an explicit
upper/lower polar half-plane plus cross product; testing `cross == 0` alone would be
incorrect because opposite directions are also collinear. When two current edges have
the same polar direction, production advances both polygons and emits their vector
sum. Otherwise it advances the smaller polar direction.

Invariant: after every emitted edge, the running vertex equals the sum of the current
left and right hull vertices, and the emitted boundary is exactly the merged prefix of
the two nondecreasing polar edge sequences. Thus the walk traces the boundary of the
Minkowski sum and returns to its starting vertex after all edges are consumed.

If either hull is a point or segment, production uses the identity
`conv(A)+conv(B) = conv({a+b})` on the bounded hull vertices and delegates only that
degenerate canonicalization to sealed `convex_hull`. Because one hull has at most two
vertices, this does not introduce a quadratic general-case implementation.

## Verification

Focused repo-style execution passed:

- GCC C++20 with repository strict warnings-as-errors;
- Clang C++20 with the same strict warnings;
- actual GCC ASan+UBSan with halt-on-error.

Deterministic evidence covers empty sums, point translation, segment+segment,
proper polygons, duplicate/interior input points, commutativity, exact positive and
negative coordinate boundaries, domain rejection, and a polygon pair with parallel
and opposite-direction boundary edges.

The primary randomized oracle is structurally independent of the angular merge. For
2,000 fixed-seed pairs of point multisets, each side has `0..14` points with integer
coordinates in `[-70,70]`. The oracle enumerates every pairwise point sum and then
calls the sealed convex hull. Production output must equal that canonical oracle
exactly; every nondegenerate production hull additionally replays strict CCW turns
and lexicographic-start invariants.

The oracle is intentionally `O(nm)` and test-only. It is not production reuse of the
edge-merge recurrence.

## Complexity / non-claims

Let `n,m` be input point counts and `h1,h2` their hull sizes. Input canonicalization
costs `O(n log n + m log m)`. For two proper polygons the Minkowski boundary merge is
`O(h1+h2)`. Degenerate point/segment fallback contains at most `2*max(h1,h2)` pair
sums before hull canonicalization. Overall this direct API is therefore
`O(n log n + m log m)` time and `O(n+m)` auxiliary/output scale.

No floating-point geometry, arbitrary-precision coordinates, non-convex polygon sum,
holes, support-function query index, 3D Minkowski operation, or process-scale geometry
claim is made.
