# Scope recovery: symmetric Lanczos tridiagonalization

## Coverage decision

After primitive BCH(15,7,5) coding merged as
`main@24406c1491ef0cac483bf8e017a424eeb58eec18`, a fresh live-state audit
found zero open pull requests and zero open issues.

The repository already contains:

- dense SPD conjugate-gradient solving;
- dense unrestarted GMRES with Arnoldi basis construction;
- two-sided Householder bidiagonalization;
- QR/SVD and other dense linear-algebra foundations.

It did **not** contain a Lanczos tridiagonalization implementation, branch, or
prior implementation pull request.

This slice therefore adds a different executable capability: reduction of a
finite exact-symmetric dense matrix to a Krylov orthonormal basis and symmetric
tridiagonal projection using the classical three-term Lanczos recurrence. It
does not add another linear-system solver.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; the historical compiler/backend
frontier remains frozen.

## Public contract

`lanczos_tridiagonalize(matrix, starting_vector, max_steps,
breakdown_tolerance)` accepts:

- a finite dense square matrix;
- exact entrywise symmetry;
- a finite non-zero starting vector of matching dimension;
- for non-empty matrices, a step limit in `[1,n]`;
- a finite non-negative breakdown tolerance.

The empty matrix is represented by an empty matrix, empty starting vector, and
zero steps.

The result contains:

- status: `empty`, `invariant_subspace`, or `iteration_limit`;
- basis vectors `q_0,...,q_{k-1}`;
- diagonal coefficients `alpha_0,...,alpha_{k-1}`;
- subdiagonal coefficients `beta_0,...,beta_{k-2}`;
- the terminal unnormalized residual vector;
- its Euclidean norm;
- completed step count.

## Three-term recurrence

The normalized start vector is `q_0`.

For step `j`:

`r = A q_j - beta_{j-1} q_{j-1}`

`alpha_j = q_j^T r`

`r = r - alpha_j q_j`

`beta_j = ||r||_2`

If `beta_j <= breakdown_tolerance`, the current Krylov subspace is treated as
an invariant-subspace boundary and production returns
`invariant_subspace`.

If the user-requested step limit is reached first, production returns
`iteration_limit` together with the terminal residual.

Otherwise:

`q_{j+1} = r / beta_j`.

For the returned final basis vector:

`A q_{k-1} = beta_{k-2} q_{k-2} + alpha_{k-1} q_{k-1} + residual`.

For every earlier column the corresponding residual term is represented by the
stored next basis vector and subdiagonal coefficient.

## Why no full reorthogonalization

This slice intentionally implements the classical symmetric three-term
recurrence.

Full modified-Gram-Schmidt reorthogonalization against all previous basis
vectors would change the executed recurrence and introduce additional
projection coefficients outside the tridiagonal structure.

Therefore production does **not** silently convert Lanczos into Arnoldi.

The consequence is explicit: in floating-point arithmetic, global basis
orthogonality may degrade on long or ill-conditioned runs. That numerical
phenomenon is a documented boundary, not hidden by a stronger claim.

## Independent verification

The committed reference implementation in tests uses **two-pass full Arnoldi
modified Gram-Schmidt**, not the production three-term recurrence.

For each random symmetric matrix and starting vector it constructs an
independent Krylov basis and upper-Hessenberg projection.

Because the matrix is symmetric, exact arithmetic implies that Arnoldi's
projected Hessenberg matrix is tridiagonal. The tests therefore compare:

- Lanczos diagonal coefficients against Arnoldi diagonal entries;
- Lanczos subdiagonal coefficients against Arnoldi subdiagonal entries;
- Arnoldi entries above the first superdiagonal against zero;
- returned Lanczos basis orthonormality;
- the explicit three-term matrix recurrence;
- terminal residual norm against the residual vector.

Committed deterministic coverage also includes:

- empty input;
- invalid rectangular/non-symmetric matrices;
- starting-vector size mismatch;
- zero starting vector;
- invalid step limits;
- invalid breakdown tolerance;
- non-finite input;
- identity-matrix one-step invariant-subspace breakdown;
- a truncated multi-step run that must retain a nonzero terminal residual;
- non-finite intermediate arithmetic failing closed.

Randomized committed coverage uses 120 fixed-seed symmetric matrices of dimension
2..8.

A supplementary model-level differential pass checked 10,000 additional random
symmetric matrices of dimension 2..8. It found zero Krylov-dimension mismatch;
the worst observed relative alpha/beta differences versus two-pass Arnoldi were
approximately `2.8e-13` and `8.2e-14`, and the worst observed basis
orthogonality error was approximately `7.7e-11`.

Those model observations are supporting evidence only. They are not benchmark
claims and do not replace repository compiler/sanitizer CI.

## Arithmetic failure boundary

All public inputs must be finite.

Production checks matrix-vector products, recurrence updates, dot products,
normalization, and norms for non-finite intermediate results. Overflow or
non-finite intermediate arithmetic throws `std::overflow_error` instead of
continuing with NaN/Inf state.

Exact positive-definiteness is **not** required; only exact symmetry is part of
the mathematical input contract.

## Complexity boundary

For an `n x n` dense matrix and `k <= n` requested Lanczos steps:

- each dense matrix-vector product is `O(n^2)`;
- the three-term recurrence adds `O(n)` work per step;
- total time is `O(k n^2)`;
- returned basis storage is `O(k n)`;
- tridiagonal coefficient storage is `O(k)`.

The input dense matrix itself occupies `O(n^2)` storage.

This slice does not claim sparse-matrix complexity because the public matrix
representation is dense.

## Non-claims

This slice does not claim:

- full or selective reorthogonalization;
- finite-precision exact orthogonality;
- eigenvalue/eigenvector convergence guarantees;
- Ritz-pair extraction;
- implicit restart;
- thick restart;
- block Lanczos;
- sparse matrix support;
- generalized eigenproblems;
- benchmark-backed performance.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/numerical/lanczos.hpp`;
- `tests/test_lanczos_cases.hpp`;
- `docs/scope_recovery_lanczos.md`.

Pre-PR hardening includes explicit `<utility>` inclusion for the independent
test reference's move operation.

No CMake, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `24406c1491ef0cac483bf8e017a424eeb58eec18`.
