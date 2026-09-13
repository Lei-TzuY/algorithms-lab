# Scope recovery: dense Householder QR factorization

## Coverage decision

A fresh live audit at `main@db8fabc6f51abac9fe2b4aaa5beed1290c67d81e`
found no Householder / QR-decomposition implementation in default-branch code,
pull-request history, or recovery branches. The recent dense conjugate-gradient
checkpoint is an iterative SPD linear solver; it does not provide an orthogonal-
triangular factorization. Occupied minimum-cycle-basis, BFPRT-selection, and
subset-convolution surfaces remain untouched.

This slice deliberately changes proof model from Krylov recurrences to products
of orthogonal reflections.

## Production contract

`algorithms::numerical::householder_qr(A)` accepts a finite dense rectangular
`double` matrix and returns a full `m x m` matrix `Q`, an `m x n` upper-
trapezoidal matrix `R`, and the number of nontrivial reflectors applied.

- empty input is accepted;
- non-empty ragged or non-finite input is rejected;
- matrices with zero columns and rank-deficient matrices are supported;
- no numerical-rank decision or pivoting policy is claimed;
- non-finite required intermediate arithmetic fails closed with
  `std::overflow_error`.

The mathematical contract is `A = Q R` and `Q^T Q = I`. In finite precision the
returned factors satisfy these identities up to ordinary floating-point error;
this API does not claim bit-exact reconstruction.

## Algorithm and proof obligation

At column `k`, production forms the active tail `x = R[k:m,k]` and constructs a
normalized Householder vector `v` so that

`H = I - 2 v v^T`

maps `x` to a signed multiple of the first coordinate. It applies `H` from the
left to the remaining block of `R` and from the right to the accumulated `Q`.
Every Householder matrix is symmetric and orthogonal (`H^T H = I`), so the
product of reflectors remains orthogonal and the invariant `A = Q R` is
preserved in exact arithmetic. Successive reflectors leave previous leading
columns unchanged and zero the current subdiagonal, yielding upper-trapezoidal
`R`.

Those reflection identities are proof obligations. Floating-point tests are
implementation evidence, not a proof of backward stability or conditioning.

## Independent verification

Focused candidate verification passes strict GCC, strict Clang, and actual GCC
ASan+UBSan.

Evidence covers empty/zero-column/ragged/non-finite input, the classical 3x3 QR
example, tall and wide matrices, rank deficiency, and an explicit intermediate-
overflow rejection. Across 700 fixed-seed random tall/square matrices up to
10x10, tests replay `Q*R`, `Q^T Q`, and exact triangular zeros. For full-column-
rank cases, an independent modified Gram-Schmidt implementation builds a basis
and the two column-space projectors must agree numerically. The MGS oracle does
not reuse Householder reflectors.

## Complexity and non-claims

For an `m x n` matrix with `k=min(m,n)`, this direct full-Q implementation uses
`O(m n k + m^2 k)` floating-point work and `O(m^2 + mn)` returned/resident
storage, plus `O(m)` reflector scratch. It intentionally materializes full `Q`
rather than claiming economy-storage optimality.

No column pivoting, rank-revealing QR, least-squares solver, condition estimator,
blocked/BLAS acceleration, sparse QR, arbitrary precision, backward-error bound,
or benchmark-speedup claim is made.
