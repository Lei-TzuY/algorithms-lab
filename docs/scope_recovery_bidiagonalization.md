# Scope recovery: two-sided Householder bidiagonalization

## Coverage decision

A fresh post-Phase-69 live-code, pull-request-history, and branch audit found no
bidiagonal / Golub-Kahan reduction surface. The repository already contains a
first-principles Householder QR reduction and a symmetric Jacobi eigensolver, but
neither performs a two-sided orthogonal reduction of a general rectangular matrix.
This slice therefore changes proof model rather than adding another ordered-set or
heap variant after the scapegoat-tree and skip-list recovery checkpoints.

## Production contract

`algorithms::numerical::householder_bidiagonalize(A)` accepts a finite dense
rectangular `double` matrix and returns full orthogonal factors `U`, `V` and a
matrix `B` such that `A ~= U B V^T` in floating-point arithmetic.

- `U` is `m x m`, `B` is `m x n`, and `V` is `n x n`;
- `B` is explicitly upper bidiagonal: only `(i,i)` and `(i,i+1)` may be nonzero;
- tall, square, wide, rank-deficient, zero-row-content, and zero-column shapes are
  valid;
- ragged or non-finite inputs are rejected;
- non-finite intermediate norms, dot products, or updates fail closed with
  `std::overflow_error`;
- returned reflector counters are diagnostics only.

No singular values, numerical rank, pseudoinverse, least-squares solution, or SVD
convergence claim is implied.

## Invariant / proof obligation

At step `k`, a left Householder reflector `H_k` acts on rows `k..m-1` and zeros
all entries below `(k,k)`. Production updates `B <- H_k B` and `U <- U H_k`.
Because `H_k` is symmetric orthogonal, `U B V^T` is unchanged.

A right reflector `G_k` then acts on columns `k+1..n-1`, zeros row `k` beyond the
superdiagonal, and updates `B <- B G_k`, `V <- V G_k`. Again orthogonality gives
`U (B G_k) (V G_k)^T = U B V^T`. Later reflector supports do not disturb the
already-established bidiagonal zeros. Explicitly clearing mathematically zero
eliminated tails makes the public structure contract exact despite roundoff.

Correctness relies on the Householder reflection identity and the support argument
above. Reconstruction tests are implementation evidence, not substitutes for that
algebraic proof.

## Complexity / storage boundary

Let `k=min(m,n)`. This direct educational implementation materializes full `U`
and `V` and updates them eagerly. Its conservative time bound is
`O(k * (m^2 + mn + n^2))`; resident result/storage is `O(m^2 + mn + n^2)`.
This deliberately does **not** claim the lower constants/storage of a LAPACK-style
compact-reflector representation.

## Independent verification

Focused repo-native verification covers:

- empty, zero-column, ragged, and non-finite contracts;
- square, tall, wide, and exactly rank-deficient matrices;
- 700 fixed-seed random rectangular matrices with dimensions independently sampled
  in `1..10`;
- direct generic matrix multiplication checks `A ~= U B V^T`;
- independent Gram products check `U^T U ~= I` and `V^T V ~= I`;
- every off-bidiagonal entry of `B` is required to be exactly zero;
- a full-width overflow construction must fail closed.

The verification does not call Householder QR, an SVD routine, or another
bidiagonalization recurrence.

## Scope / non-claims

The intended recovery slice is header-only production plus one dedicated native
test header, one `test_main.cpp` include, and this proof document. It does not
modify CMake, README, historical ROADMAP, recovery authority, workflows,
benchmarks, frozen compiler/backend surfaces, or unrelated recovery work.
