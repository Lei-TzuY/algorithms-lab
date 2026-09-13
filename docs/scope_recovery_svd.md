# Scope recovery: one-sided Jacobi singular-value decomposition

## Recovered capability

This checkpoint adds a first-principles dense numerical SVD baseline for finite
rectangular `double` matrices:

```text
A ~= U diag(sigma) V^T
```

The public result is thin: `r = min(rows, columns)`, `U` is `rows x r`, `V` is
`columns x r`, and `sigma` contains `r` nonnegative singular values in
descending order. Wide inputs are handled by decomposing the transpose and
swapping the thin factors.

The implementation uses one-sided Jacobi column orthogonalization. It does not
form `A^T A`, so this recovery avoids deliberately squaring the input condition
number merely to obtain singular vectors. It also does not delegate the SVD to
BLAS/LAPACK or another numerical library.

## Numerical contract and invariants

For a tall work matrix `B`, each Jacobi rotation acts on a column pair `(p,q)`
and applies the same orthogonal rotation to the accumulated right factor `V`.
Therefore every sweep preserves

```text
B = A V
```

up to floating-point rounding. Convergence is defined by scaled pairwise column
correlations:

```text
|b_p^T b_q| <= tolerance * ||b_p|| * ||b_q||.
```

After convergence, column norms become the singular-value estimates and the
nonzero left vectors are normalized work columns. Singular values below the
explicit tolerance-scaled numerical-rank threshold are reported as zero; the
corresponding factor columns are completed deterministically to an orthonormal
basis. This is a numerical rank convention, not an exact symbolic rank claim.

The returned factors are checked by the executable obligations

- `sigma[i] >= 0` and descending;
- thin `U^T U ~= I` and `V^T V ~= I`;
- reconstruction `A ~= U diag(sigma) V^T`;
- for nonzero reported singular values, `A v_i ~= sigma_i u_i` and
  `A^T u_i ~= sigma_i v_i`;
- `sum sigma_i^2 ~= ||A||_F^2`.

Input matrices must be rectangular and finite. A nonpositive/nonfinite
tolerance, zero sweep budget, nonfinite input, arithmetic overflow, or failure
to converge within the caller's sweep budget is rejected explicitly.

## Complexity boundary

For `m >= n`, a sweep examines all column pairs and touches `m` entries per
pair, so the direct baseline costs `O(S m n^2)` time for `S` sweeps. Wide
matrices are transposed first, giving the symmetric `O(S n m^2)` bound. The
factor/work storage is `O(mn + r^2)`.

This is an educational dense baseline. It does not claim LAPACK-grade backward
stability, bidiagonal QR iteration, divide-and-conquer SVD, randomized low-rank
SVD, or asymptotically optimal dense linear algebra.

## Verification evidence

Focused GCC, Clang, and real ASan+UBSan builds all pass under the repository's
strict warning policy. The recovery cases include:

- empty, zero-column, tall, wide, rank-deficient, repeated-singular-value,
  malformed, nonfinite, and overflow inputs;
- 500 fixed-seed random `1..6 x 1..6` matrices checking reconstruction,
  orthogonality, singular equations, ordering, and Frobenius-energy identity;
- an independent 500-case `2 x 2` closed-form oracle. For a `2 x 2` matrix the
  squared singular values are the two roots determined by
  `trace(A^T A)` and `det(A)^2`, so this check does not reuse the Jacobi
  recurrence or the reconstruction path;
- exact repeated execution on one deterministic matrix to lock replayable
  ordering/rotation behavior.

The randomized corpus is evidence for the implementation. It is not a proof of
floating-point stability for arbitrary matrices.
