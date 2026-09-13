# Scope recovery: dense SPD conjugate-gradient solver

## Coverage decision

A fresh live audit at `main@8e51e8d7d54454b125cdddfd6a5c1fafebceeade`
found no conjugate-gradient, Krylov-subspace, Cholesky, QR, or other iterative
linear-system solver in default-branch code, pull-request history, or recovery
branches. The latest weighted-set-cover checkpoint reached exact merged-main
GCC release, Clang release, and GCC ASan+UBSan success before this slice began.

This deliberately changes proof and arithmetic model after the recent exact
combinatorial / geometry recovery stream. The new capability is floating-point
iterative numerical linear algebra with an explicit convergence/breakdown
contract. It does not touch the frozen compiler/backend surface or the occupied
minimum-cycle-basis recovery surface.

## Production contract

`algorithms::numerical::conjugate_gradient_solve` solves a dense symmetric
system `A x = b` by the first-principles conjugate-gradient recurrence.

- `A` is a square `double` matrix and is required to be exactly symmetric.
- `b`, `A`, an optional initial guess, and the absolute tolerance must be finite.
- the mathematical semantic precondition is that `A` is symmetric positive
  definite (SPD).
- positive definiteness is intentionally not checked by an `O(n^3)` direct
  factorization, because that would replace the intended iterative capability
  with a direct-solver gate.
- if the actually executed Krylov sequence observes `p^T A p <= 0`, the result
  reports `non_positive_curvature` instead of claiming convergence.
- the stopping rule is explicit absolute residual norm
  `||b - A x||_2 <= absolute_tolerance`.
- exhausting the caller-provided iteration budget returns `iteration_limit` and
  the last iterate rather than silently treating it as a solution.
- non-finite intermediate arithmetic fails closed with `std::overflow_error`.
- an empty `0 x 0` system is the canonical converged empty solution.

The API therefore separates mathematical preconditions, finite-precision
breakdown, successful convergence, and a finite work budget.

## Recurrence and proof obligations

Starting from `r_0 = b - A x_0` and `p_0 = r_0`, production iterates

`alpha_k = (r_k^T r_k) / (p_k^T A p_k)`,

`x_(k+1) = x_k + alpha_k p_k`,

`r_(k+1) = r_k - alpha_k A p_k`,

`beta_k = (r_(k+1)^T r_(k+1)) / (r_k^T r_k)`,

`p_(k+1) = r_(k+1) + beta_k p_k`.

In exact arithmetic with SPD `A`, the search directions are mutually
`A`-conjugate, residuals are mutually orthogonal, and each iterate minimizes the
quadratic energy over the growing Krylov subspace. Those classical facts imply
termination in at most `n` steps in exact arithmetic.

This implementation does **not** transfer that finite-step theorem unchanged to
IEEE-754 execution. Rounding can destroy exact orthogonality/conjugacy, so the
public contract is residual-based and budgeted. Tests are implementation
evidence, not a floating-point convergence theorem or a condition-number bound.

## Verification

The final focused repository-style candidate passed:

- GCC C++20 with repository strict warnings-as-errors: 4/4;
- Clang C++20 with repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with leak detection / halt-on-error: 4/4.

Deterministic evidence covers shape/rhs/initial-guess/tolerance validation,
non-finite input and intermediate overflow rejection, the canonical empty
system, an exact small SPD system, zero-iteration warm-start convergence,
iteration-budget exhaustion, and a symmetric-indefinite input that exposes
non-positive curvature immediately.

Primary randomized evidence uses 500 fixed-seed dense SPD systems of dimensions
1..12. Each matrix is constructed as `M^T M + 2I` from small exact integer
entries, so positive definiteness is independent of production. The primary
oracle is dense Gaussian elimination with partial pivoting; it contains no
Krylov recurrence. Production must converge, replay a small residual, and agree
coordinate-wise with the independently solved system.

The test corpus is deterministic from raw `std::mt19937_64` output and does not
rely on implementation-specific `std::uniform_*_distribution` mappings.

## Complexity and non-claims

For dense dimension `n` and `k` executed iterations, each matrix-vector product
costs `O(n^2)` and vector operations cost `O(n)`, for `O(k n^2)` time and `O(n)`
algorithm workspace beyond the caller-owned dense matrix / result. Exact
symmetry and input validation cost `O(n^2)`.

No sparse-matrix format, preconditioned CG, Cholesky factorization, QR solver,
GMRES/MINRES, condition-number estimate, backward-error theorem, universal
floating-point convergence rate, exact arithmetic, SIMD/BLAS acceleration, or
benchmark speedup is claimed.
