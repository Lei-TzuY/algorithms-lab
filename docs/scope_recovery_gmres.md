# Scope recovery: dense unrestarted GMRES

## Why this slice

The post-Phase-69 recovery authority requires new work to add algorithmic depth rather than extend the frozen compiler/backend frontier or farm adjacent variants. The live repository already contains a dense symmetric-positive-definite conjugate-gradient solver, and that recovery checkpoint explicitly left GMRES/MINRES outside its scope. Fresh default-branch and PR-history searches found no generalized-minimal-residual implementation.

This slice changes proof and arithmetic model again after the bounded-exact Pell checkpoint: it adds a first-principles Krylov least-squares solver for general dense square real systems, where finite-precision residual minimization and Arnoldi breakdown are part of the public contract.

## Production contract

`gmres_solve(A, b, absolute_tolerance, max_iterations, initial_guess)` accepts a finite dense square `double` matrix, a matching finite right-hand side, an optional finite initial guess, a positive finite absolute tolerance, and a positive iteration budget for non-empty systems.

The solver is unrestarted. It performs at most `min(max_iterations, n)` Arnoldi steps and returns:

- `converged` only when the explicitly replayed dense residual satisfies `||b - A x||_2 <= absolute_tolerance`;
- `iteration_limit` when the effective Arnoldi budget is exhausted first;
- `arnoldi_breakdown` when the generated Krylov sequence becomes invariant/singular before the residual contract is met.

The empty `0 x 0` system is the canonical converged empty solution. Nonsingularity is deliberately not prevalidated with an `O(n^3)` factorization because that would replace the intended iterative capability with a direct-solver gate. Non-finite intermediate arithmetic throws instead of silently propagating NaN/Inf.

## Algorithm and invariants

Production implements unrestarted GMRES from first principles:

1. form the initial residual and normalize it to the first Krylov basis vector;
2. use modified Gram-Schmidt Arnoldi orthogonalization to extend the basis and upper-Hessenberg relation;
3. apply incremental Givens rotations to maintain an upper-triangular least-squares factor;
4. solve the current triangular least-squares system and reconstruct the iterate in the Krylov basis;
5. replay `A x` directly and use the resulting residual norm as the convergence certificate.

Important obligations are:

- Arnoldi column `j` lies in the span of `A q_j` minus all prior projections;
- previously generated Givens rotations keep earlier subdiagonal entries zero;
- the reconstructed iterate belongs to the affine Krylov space `x_0 + K_k(A, r_0)`;
- a returned `converged` status is certified by a fresh dense residual, not merely the Hessenberg residual estimate.

In exact arithmetic, GMRES minimizes the residual norm over the current Krylov affine space. This repository does **not** transfer the exact-arithmetic finite-step theorem or exact orthogonality to IEEE-754 execution.

## Verification

The exact focused candidate passed:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan: 4/4.

Deterministic evidence covers malformed shapes, rhs/initial-guess mismatches, invalid tolerances/budgets, non-finite input, empty input, exact warm-start convergence, a known nonsymmetric 3x3 solve, iteration-budget exhaustion, happy-breakdown convergence on an invariant Krylov subspace, zero-operator Arnoldi breakdown, and non-finite matrix-product overflow.

Primary randomized oracle: 500 fixed-seed nonsymmetric strictly diagonally dominant systems with dimensions 1..8. The oracle is independent partial-pivot Gaussian elimination; it uses no Arnoldi basis, Krylov recurrence, Hessenberg matrix, or Givens rotation. Production must report convergence, replay a residual at most `1e-9`, and agree coordinate-wise with the independent direct solve.

## Complexity and non-claims

For dimension `n` and `k <= min(max_iterations, n)` executed steps, dense matrix-vector products cost `O(k n^2)`. Modified Gram-Schmidt and repeated materialization/back-substitution contribute at most `O(k^2 n + k^3)` in this direct educational implementation. Storage is `O(k n + k^2)` for the Krylov basis and Hessenberg/rotation state.

No restarted GMRES, sparse-matrix format, preconditioner, flexible GMRES, MINRES, condition-number estimate, backward-error theorem, universal floating-point convergence rate, BLAS/SIMD acceleration, benchmark speedup, or exact-arithmetic guarantee is claimed.

## Scope

The intended recovery PR has exactly three paths:

- `include/algorithms/numerical/gmres.hpp`;
- `tests/test_gmres_cases.hpp`;
- `docs/scope_recovery_gmres.md`.

The isolated `test_*_cases.hpp` harness introduced by the verification-hardening checkpoint discovers the test header automatically, so no CMake/test registry edit is needed. Historical ROADMAP/compiler-backend scope remains untouched.
