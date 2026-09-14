# Scope recovery: full-column-rank dense least squares

## Coverage decision

A fresh live default-branch, pull-request, branch, and code audit at
`main@6ca448920f95e73cc3eab03fa4bdbb435ebd3cce` found no dense least-squares
solver. The repository already contains first-principles Householder QR, PLU,
Cholesky, bidiagonalization, and one-sided Jacobi SVD, but their documented
contracts deliberately stop short of a least-squares solve. The immediately
preceding recovery slice added separation-pair triconnectivity, so this slice
changes proof model again from structural two-vertex failure to orthogonal
projection and triangular solution.

The historical compiler/backend ROADMAP remains prospectively frozen under
`docs/scope_recovery_after_phase69.md` and is not modified by this recovery.

## Production contract

`least_squares_qr(A, b, relative_rank_tolerance)` solves

`min_x ||A x - b||_2`

for a finite dense `m x n` matrix with `m >= n` and numerically full column
rank. It returns:

- the `n`-vector solution;
- the explicit residual `r = b - A x`;
- `||r||_2`;
- `||A^T r||_inf` as an executable first-order optimality witness;
- the accepted numerical rank, equal to `n` for every successful solve.

The implementation reuses the sealed Householder QR decomposition `A ~= Q R`,
computes the leading `n` entries of `Q^T b`, and solves the leading triangular
system by checked back substitution. The rank decision compares every leading
`|R_ii|` against `relative_rank_tolerance * max_j |R_jj|`.

Underdetermined systems and numerically rank-deficient systems are rejected.
This bounded contract deliberately does not silently switch to a pseudoinverse or
minimum-norm solution. Zero-column systems are valid: the solution is empty and
the residual is exactly the supplied right-hand side.

Every input and arithmetic path is required to remain finite. Non-finite input
is rejected and non-finite intermediate arithmetic fails closed with
`std::overflow_error`.

## Correctness obligation

For full-column-rank `A = Q R`, orthogonality preserves the Euclidean norm:

`||A x - b||_2 = ||R x - Q^T b||_2`.

Because the first `n x n` block of `R` is nonsingular upper triangular, choosing
`x` by back substitution makes the first `n` transformed residual coordinates
zero. The remaining transformed coordinates are independent of `x`, so this
choice minimizes the norm. Equivalently, the returned residual satisfies the
normal-equation optimality condition `A^T r ~= 0`; the implementation computes
and returns its infinity norm rather than hiding that obligation.

The numerical-rank threshold is an explicit API policy, not a claim of rank-
revealing QR or condition-number estimation.

## Verification

Focused verification against the exact live Householder-QR interface passed:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan: 4/4.

Deterministic cases cover empty and zero-column systems, underdetermined input,
right-hand-side shape and finiteness errors, invalid tolerance, rank deficiency,
a known overdetermined line fit with exact rational optimum/residual, and a
non-finite intermediate that must fail closed.

The primary randomized corpus contains 1,000 fixed-seed full-rank systems with
`n=1..7` and `m=n+1`. Each matrix is constructed as `[I; c^T]`. The oracle
chooses an exact solution `x*` and an independently constructed residual
`r=[-c t; t]`, for which `A^T r = 0` identically, then forms `b=A x*+r`.
Production must recover `x*`, replay the residual, and report a small normal
residual. This oracle does not run QR, normal equations, SVD, or another
least-squares solver.

## Complexity and non-claims

This educational baseline materializes the existing full `m x m` Householder
`Q`. Its asymptotic cost is therefore inherited from that decomposition plus
`O(mn + n^2)` for projection, triangular solve, residual replay, and the normal
residual. Resident storage is dominated by the full QR result.

This slice does **not** claim rank-revealing pivoting, rank-deficient
least-squares, Moore-Penrose pseudoinverses, underdetermined minimum-norm
solutions, weighted/constrained least squares, iterative sparse methods,
condition estimation, LAPACK-grade backward stability, or benchmark-backed
performance. Those are separate frontiers rather than hidden implications of
this bounded full-column-rank solver.
