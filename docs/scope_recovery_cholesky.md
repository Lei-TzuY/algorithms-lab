# Scope recovery: dense Cholesky factorization and SPD solve

## Coverage decision

Fresh live recovery audit at `main@1d47e10fa64b174afdc0106abb8a9e9881bd18b1` found no Cholesky production API in default-branch code, pull-request history, or the `scope-recovery-cholesky` branch namespace. The only live mention is the conjugate-gradient recovery note, which explicitly excludes Cholesky from that iterative solver's contract. The latest merged capability is validated A* shortest path, so this slice deliberately changes proof model from heuristic graph search to direct dense numerical factorization. Long-lived occupied recovery surfaces such as minimum cycle basis remain untouched. `docs/scope_recovery_after_phase69.md` remains the prospective authority; frozen compiler/backend work and the stale historical ROADMAP are not resumed.

## Production contract

`algorithms::numerical::cholesky_factorize(A)`:
- requires a finite, dense, exactly symmetric square `double` matrix;
- returns an explicit lower-triangular factor `L` with strictly positive diagonal;
- the executable witness is `A ~= L L^T` under ordinary floating-point replay;
- empty `0x0` input returns an empty factor;
- ragged, non-finite, or asymmetric input is rejected;
- if the executed recurrence encounters a non-positive diagonal residual, it throws `std::domain_error` rather than claiming an SPD factorization;
- non-finite or non-`double`-representable required intermediates fail closed with `std::overflow_error`.

`cholesky_solve(factor, rhs)`:
- revalidates square/lower-triangular/finite/positive-diagonal factor structure;
- performs forward substitution through `L` followed by backward substitution through `L^T`;
- rejects malformed factors, rhs shape mismatches, and non-finite rhs values;
- required unrepresentable solve intermediates fail closed.

Dot products/residuals use `long double` accumulation before final `double` factor/solution entries are narrowed. This is a bounded numerical implementation choice, not an arbitrary-precision or platform-independent bit-pattern claim.

## Correctness boundary

In exact arithmetic, after finishing column `j`, every finalized lower entry satisfies

`A[i][j] = sum_{k<j} L[i][k] L[j][k] + L[i][j] L[j][j]`

for `i >= j`. On the diagonal, positive definiteness makes

`A[j][j] - sum_{k<j} L[j][k]^2 > 0`,

so choosing its positive square root yields the unique positive-diagonal Cholesky factor. Off-diagonal entries then follow by division by `L[j][j]`. Forward/back substitution solve the two triangular systems whose product is `A`.

The classical Cholesky theorem is the mathematical proof obligation. IEEE-754 execution is not claimed to be an exact SPD classifier for arbitrarily ill-conditioned matrices: the public finite-precision contract is operational, based on the signs and representability of the actually computed residuals.

## Verification

Focused pre-upload execution passed:
- GCC 14 C++20 with repository strict warnings-as-errors: 4/4;
- Clang 17 C++20 with the same strict warnings: 4/4;
- actual GCC ASan+UBSan with fail-fast/leak detection: 4/4.

Deterministic evidence covers:
- the classical `[[4,12,-16],[12,37,-43],[-16,-43,98]]` factor with exact small-integer `L`;
- factor replay and a known linear solve;
- empty input;
- ragged/asymmetric/non-finite input rejection;
- positive-semidefinite and symmetric-indefinite breakdown;
- malformed factor and rhs validation;
- a full floating-range diagonal case using `DBL_MAX` and `DBL_MIN`;
- repeated deterministic factorization.

Primary randomized verification is structurally independent of the Cholesky recurrence. Across 700 fixed-seed SPD matrices with dimensions 1..10, each matrix is generated as `M^T M + (n+1)I` from small integer entries. Every returned factor is replayed through direct matrix multiplication. For each matrix a random rhs is solved both by production and by an independent partial-pivot Gaussian-elimination implementation; solutions must agree within a tight scale-aware tolerance.

The Gaussian oracle uses elimination/pivoting rather than square-root factorization. Reconstruction and randomized equality are implementation evidence, not a numerical-stability theorem.

## Complexity / non-claims

For dense dimension `n`:
- factorization: `O(n^3)` arithmetic and `O(n^2)` returned/resident factor storage;
- one solve from an existing factor: `O(n^2)` time and `O(n)` additional vectors plus the returned solution.

No sparse Cholesky, pivoted/modified Cholesky, LDL^T, rank-revealing behavior, condition estimate, backward-error/stability theorem, BLAS/LAPACK performance, arbitrary precision, or benchmark speedup is claimed.

## Scope

Exactly four paths are intended to differ from `main`:
- `include/algorithms/numerical/cholesky.hpp`;
- `tests/test_cholesky_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- `docs/scope_recovery_cholesky.md`.

No CMake, README, historical ROADMAP, recovery authority, workflow, benchmark, frozen compiler/backend, minimum-cycle-basis, or temporary-file churn is intended.
