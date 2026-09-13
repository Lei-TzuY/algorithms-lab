# Scope recovery: symmetric Jacobi eigendecomposition

## Coverage decision

This post-Phase-69 recovery slice adds a numerical spectral-decomposition proof model that is absent from live default-branch code, pull-request history, and the branch namespace. The immediately preceding recovery checkpoint is bounded exact directed feedback vertex set, so this deliberately changes from exponential combinatorial cycle breaking to orthogonal-similarity iteration rather than farming another bounded NP-hard graph deletion variant. Historical Phase-45–69 compiler/backend headings remain frozen; `docs/scope_recovery_after_phase69.md` remains the prospective authority.

## Production contract

`algorithms::numerical::symmetric_jacobi_eigendecomposition` accepts a finite, square, exactly symmetric dense `double` matrix and returns:

- eigenvalues sorted in nondecreasing order;
- eigenvectors as columns paired with those eigenvalues;
- sweep and rotation counts;
- final maximum absolute off-diagonal entry; and
- an explicit `converged` flag.

The caller supplies a finite relative tolerance in `[0,1]` and a positive sweep budget. Convergence means the current maximum off-diagonal magnitude is at most `tolerance * max(1, max_ij |A_ij|)`. Exhausting the budget returns `converged=false`; it is not silently promoted to success. Eigenvector signs are canonicalized by making the largest-magnitude component non-negative. Repeated eigenvalues do not imply a unique basis inside their eigenspace.

Required non-finite intermediate arithmetic fails closed with `std::overflow_error`. The implementation does not silently symmetrize nearly symmetric input.

## Algorithm and proof boundary

Production uses cyclic Jacobi plane rotations. For each `p < q`, an orthogonal rotation is chosen to annihilate `A[p][q]`, the symmetric matrix is updated as `A <- J^T A J`, and the accumulated eigenvector matrix is updated as `V <- V J`. Rotation parameters are computed after scaling the local `2x2` block so forming the tangent does not unnecessarily overflow. An extreme scale-underflow case either applies the equal-diagonal 45-degree rotation or makes no unsupported progress; a finite sweep budget then exposes non-convergence instead of fabricating an answer.

In exact arithmetic, every step is an orthogonal similarity transformation, so the spectrum is invariant. Classical Jacobi convergence for real symmetric matrices is the mathematical proof obligation. Floating-point residual/orthogonality tests are implementation evidence rather than a backward-stability theorem or a universal eigenvalue-error bound.

## Verification

Focused exact candidate execution before upload passed:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4; and
- actual GCC ASan+UBSan with leak detection / halt-on-error: 4/4.

Deterministic evidence covers empty/singleton inputs, a known `2x2` spectrum, repeated eigenvalues and replay determinism, ragged/non-square/non-symmetric/non-finite rejection, invalid tolerance/sweep budgets, explicit one-sweep non-convergence, representability overflow, and extreme-scale subnormal off-diagonal behavior.

The primary randomized construction does not solve the eigenproblem with another Jacobi recurrence. For 600 fixed-seed cases with `1..10` dimensions, tests choose a known diagonal spectrum, build an independent orthogonal matrix from deterministic plane rotations, and materialize `A = Q D Q^T`. Production must recover the sorted known spectrum within numerical tolerance. Every result is additionally replayed through both certificate equations:

- `A V ~= V Lambda`; and
- `V^T V ~= I`.

The known-spectrum construction plus these two replay checks exercise spectrum recovery, eigenvector pairing, and orthogonality without importing an external eigensolver.

## Complexity and non-claims

One cyclic sweep applies `O(V^2)` rotations, each updating `O(V)` matrix/eigenvector entries, for `O(V^3)` work per sweep. With caller budget `S`, the direct bound is `O(S V^3)` time and `O(V^2)` resident result/work storage.

This slice makes no claim of nonsymmetric/complex eigenvalues, Schur form, tridiagonal reduction, QR iteration, divide-and-conquer eigensolvers, arbitrary precision, bitwise cross-platform floating reproducibility, a backward-error theorem, or LAPACK-level performance.

## Scope

Exactly four paths are intended to differ from the recovery base:

- `include/algorithms/numerical/jacobi_eigen.hpp`;
- `tests/test_jacobi_eigen_cases.hpp`;
- one include line in `tests/test_main.cpp`; and
- `docs/scope_recovery_jacobi_eigen.md`.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark, frozen compiler/backend, or occupied recovery-surface files are changed.
