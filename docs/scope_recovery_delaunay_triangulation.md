# Scope recovery — exact bounded-integer Delaunay triangulation

## Coverage decision

A fresh live-state and coverage audit at `main@07435583eb8e2151e63f52c61afd96f65271f886` found no Delaunay / Voronoi / Bowyer-Watson / empty-circumcircle triangulation implementation in the default branch, pull-request history, or branch namespace. The same audit deliberately avoids the occupied `scope-recovery-minimum-cycle-basis` and `scope-recovery-exact-treewidth` surfaces. The post-Phase-69 recovery authority remains `docs/scope_recovery_after_phase69.md`; the stale historical compiler/backend roadmap is not resumed.

This slice adds a new geometric proof model rather than extending the recent dynamic-connectivity, matroid, flow, string, or dynamic-tree streak: exact empty-circle Delaunay faces with an independent paraboloid-lifting oracle.

## Production contract

`exact_delaunay_triangulation(points)` accepts a fixed point snapshot and returns:

- counterclockwise Delaunay triangles as original input indices;
- canonical sorted unique undirected triangulation edges;
- empty / singleton inputs with no edges or triangles;
- a two-point input with its one undirected edge.

The exact arithmetic domain is deliberately bounded to integer coordinates in `[-10'000, 10'000]`. Duplicate points, any collinear triple, and any cocircular quadruple are rejected. These general-position restrictions avoid pretending that symbolic perturbation / degenerate Delaunay-cell semantics are implemented when they are not.

## Exact predicate and proof boundary

For counterclockwise `a,b,c`, production evaluates the translated in-circle determinant

`|a-d|^2 cross(b-d,c-d) - |b-d|^2 cross(a-d,c-d) + |c-d|^2 cross(a-d,b-d)`.

A positive value means `d` lies strictly inside the circumcircle. A triangle is emitted exactly when every other point is strictly outside its circumcircle. Under the general-position precondition, the classical empty-circle characterization says those triangles are exactly the unique Delaunay triangulation.

The numeric bound is executable and code-local. Coordinate differences are at most `20,000`; squared norms and two-dimensional cross products are at most `8e8`. Each norm-times-cross term is therefore at most `6.4e17`, and the sum of three signed terms is bounded by `1.92e18 < INT64_MAX`. No floating-point epsilon, hidden wide-integer extension, or overflow-dependent behavior is used.

This direct educational baseline enumerates all triples and tests every other point, so it intentionally claims only `O(n^4)` time. The temporary/output edge representation is conservatively bounded by `O(n^2)` storage. No Bowyer-Watson, divide-and-conquer, randomized incremental, or `O(n log n)` claim is made.

## Independent verification

Focused final candidate passed:

- GCC C++20 with repository strict warnings-as-errors: 4/4;
- Clang C++20 with repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan: 4/4.

Deterministic evidence covers empty/singleton/two-point semantics, the full accepted coordinate boundary, duplicate rejection, collinear rejection, cocircular rejection, a nontrivial interior-point triangulation, and repeated deterministic replay.

The primary randomized oracle is structurally independent from the production in-circle formula. It lifts every point `(x,y)` to `(x,y,x^2+y^2)` and enumerates exact lower convex-hull facets with a direct three-dimensional orientation determinant. For 500 fixed-seed general-position point sets with 3..8 vertices, production triangles must match the lower-hull facets exactly; edges are reconstructed independently from the oracle facets. Tests also replay counterclockwise triangle orientation and require every input point to appear in the triangulation.

The lifting theorem / empty-circle equivalence is a mathematical proof obligation. The finite randomized corpus is implementation evidence, not a claim that testing proves Delaunay theory.

## Non-claims

This slice does not claim degenerate/cocircular symbolic perturbation, arbitrary full `int32_t` coordinates, Voronoi-cell construction, constrained Delaunay triangulation, dynamic point insertion/removal, robust floating-point predicates, minimum-angle mesh generation, or optimal asymptotic complexity.
