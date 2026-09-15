# Scope recovery: min-plus matrix algebra and exact-edge walk powers

## Coverage decision

A fresh live-state, default-branch code, pull-request-history, and branch audit at
`main@08cb60038e3426975a9fadf62de9405b82883216` found no min-plus / tropical
matrix-product surface and no occupied `min-plus` recovery branch. Several
initially attractive candidates were rejected as already present, including
matroid intersection/union, subset convolution, planarity, discrete logarithm,
FKS perfect hashing, Euler-tour dynamic forests, Smith normal form, Cartesian
RMQ, Chinese postman, Berlekamp-Massey, arithmetic coding, palindromic trees,
and Fibonacci heaps.

The immediately preceding checkpoint is prime-field Pfaffian elimination. This
slice deliberately changes proof model from skew-symmetric field elimination to
idempotent min-plus path composition. Historical compiler/backend roadmap state
remains frozen prospectively under `scope_recovery_after_phase69.md`.

## Production contract

`MinPlusMatrix` stores explicit row/column dimensions and `std::optional<int64_t>`
entries, with `nullopt` representing positive infinity. Explicit dimensions keep
zero-row and zero-column matrices unambiguous.

The header exposes:

- `min_plus_identity(n)`;
- `min_plus_product(A, B)` for dimension-compatible signed matrices;
- `min_plus_power_nonnegative(A, exponent)` for square matrices whose finite
  entries are non-negative;
- `exact_walk_distances_nonnegative(graph, edge_count)`, returning the minimum
  weight of a walk using **exactly** `edge_count` edges for every ordered vertex
  pair.

Graph conversion preserves directed/undirected adjacency, self-loops, and
parallel copies; parallel copies collapse only by taking their minimum weight for
this shortest-walk algebra. Any negative stored edge is rejected globally by the
exact-edge walk API. `edge_count == 0` returns the min-plus identity.

## Algebra / correctness obligation

For compatible matrices,

`C[i,j] = min_k (A[i,k] + B[k,j])`,

ignoring positive-infinity operands. Under the path interpretation, one finite
entry in `A` describes the best first segment and one in `B` the best second
segment, so every candidate concatenates two walks and minimization selects the
best concatenation. The identity has zero diagonal and infinity elsewhere.
Binary exponentiation over this associative min-plus product therefore gives
exact fixed-edge-count walk costs.

The public signed product handles bounded arithmetic more carefully than a
blanket checked add: a positive-overflow candidate is ignored when a smaller
representable candidate exists, while a negative-overflow candidate proves that
the mathematical minimum is below `INT64_MIN` and must fail. If every finite
candidate lies above `INT64_MAX`, the result also fails.

For non-negative matrix powers, an internal three-state value
`{infinity, representable, above-INT64_MAX}` delays narrowing across repeated
squaring. This is required because an intermediate matrix entry may exceed the
public range yet never participate in the final optimum. A committed regression
has `A^2[0,3] > INT64_MAX` while `A^3[0,3] == 3` through a different three-edge
walk. Only a genuinely unrepresentable **final** finite entry is rejected.

## Verification

Focused final candidate verification passes:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with fail-fast/leak detection: 4/4.

Deterministic evidence covers rectangular products, infinity, signed values,
zero-dimensional shapes, identity, index/dimension validation, matrix-size
overflow, representable optima beside positive-overflow alternatives, genuine
positive/negative result overflow, exponent zero, negative-power-domain
rejection, parallel edges, self-loops, and the intermediate-overflow regression
found during source review.

Primary randomized verification uses 500 fixed-seed directed/undirected
multigraphs with 0..7 vertices, non-negative weights, loops, parallel copies, and
exact edge counts 0..9. The independent oracle does not multiply matrices: for
each source it performs one plain adjacency relaxation round per required edge
and compares the complete source/target table exactly.

## Complexity and non-claims

For `A` of shape `r x k` and `B` of shape `k x c`, direct min-plus product costs
`O(rkc)` time and `O(rc)` result space. For an `n x n` matrix and positive
exponent `e`, binary min-plus powering costs `O(n^3 log e)` time and `O(n^2)`
working/result scale. Graph conversion adds `O(V+E)` work.

This is a first-principles dense baseline. It does not claim subcubic tropical
matrix multiplication, negative-edge fixed-count powering, arbitrary-precision
path weights, APSP superiority over graph-specialized algorithms, min-plus
convolution, generic semiring templates, or benchmark-backed speedups.

## Scope

Exactly four paths change: one header-only production surface, one dedicated
recovery test header, one `test_main.cpp` include, and this proof document. No
CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, Pfaffian, or temporary-file churn.
