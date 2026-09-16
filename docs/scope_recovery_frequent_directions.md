# Scope recovery: Frequent Directions matrix sketch

## Coverage decision

A fresh live audit at `main@0bf3269de286166d79bc5d12391f06079b181431`
found no Frequent-Directions / deterministic covariance-sketch implementation in
default-branch code or pull-request history. The historical
`scope-recovery-frequent-directions` branch is only a stale pointer to the
balanced-parentheses checkpoint and contains no implementation. The prospective
post-Phase-69 recovery authority freezes compiler/backend expansion, so this
slice deliberately changes proof model after balanced-parentheses tree topology
and weighted minimum cycle basis: deterministic streaming matrix sketching and
spectral energy loss rather than another graph/tree variant.

## Production contract

`algorithms::streaming::FrequentDirectionsSketch` consumes finite dense rows of a
fixed `d`-dimensional real stream and retains exactly an `ell x d` sketch with
`1 <= ell <= d`.

- rows are appended online; the original stream is not retained;
- before inserting into a full sketch, production reuses the sealed
  `one_sided_jacobi_svd` implementation;
- if the current singular values are `sigma_1 >= ... >= sigma_ell`, production
  sets `delta = sigma_ell^2` and rebuilds rows as
  `sqrt(max(sigma_i^2-delta,0)) * v_i^T`;
- the final row is thereby released for the incoming row;
- diagnostics expose dimensions, rows seen, compression count, occupied rows,
  accumulated input Frobenius energy, accumulated shrink delta, the resident
  sketch, and a structural/finite-state check;
- dimension mismatches, non-finite rows, invalid construction parameters, and
  representability failures reject explicitly.

The implementation does not hide a copy of the input matrix or batch-recompute a
rank-`ell` approximation at query time.

## Correctness / approximation boundary

In exact arithmetic, one shrink subtracts the same `delta` from every squared
singular direction. Therefore for every vector `x`, sketch covariance never
exceeds stream covariance. Summing the per-shrink loss yields the classical
Frequent-Directions guarantee: for every `k < ell`,

`0 <= ||A x||^2 - ||B x||^2 <= ||A-A_k||_F^2 / (ell-k)`

for every unit `x`, where `A_k` is a best rank-`k` approximation. The exact
arithmetic proof follows the standard shrink invariant and tail-energy charging
argument; finite tests are implementation evidence rather than a theorem proof.

This concrete implementation deliberately reuses the repository's floating
one-sided Jacobi SVD. That SVD applies a caller-controlled numerical-rank
tolerance, so ill-conditioned floating inputs inherit its numerical truncation
and convergence boundary. The repository therefore does **not** claim bit-exact
PSD ordering, exact `ell * sum(delta)` loss, or LAPACK-grade stability for every
double input. Controlled deterministic and randomized corpora verify those
identities/bounds within explicit numerical tolerances.

## Independent verification

Focused final candidate passed before upload:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with fail-fast/leak detection: 4/4.

Deterministic evidence covers constructor/row validation, transactional rejected
rows, an exactly analyzable diagonal shrink (`delta=4`), Frobenius-loss replay,
rank-deficient compression, and deterministic repeated execution.

The primary randomized corpus contains 260 fixed-seed integer matrices with
`d=2..6`, `ell=1..d`, and up to 28 streamed rows. The oracle independently:

1. forms `A^T A` and `B^T B` by direct multiplication;
2. diagonalizes those small symmetric covariance/loss matrices with a separate
   symmetric Jacobi eigenvalue routine rather than production SVD;
3. checks positive-semidefinite covariance loss within tolerance;
4. checks the Frequent-Directions tail bound for **every** `k < ell`;
5. checks independent random unit-vector quadratic forms; and
6. replays input/sketch Frobenius energy against accumulated shrink diagnostics.

The oracle shares neither streaming state nor the production shrink recurrence.

## Complexity / non-claims

Between compressions, append is `O(d)`. A full-sketch compression invokes the
sealed Jacobi SVD on an `ell x d` matrix; this slice inherits that implementation's
iterative dense numerical cost rather than claiming an asymptotically optimal
SVD. Resident sketch storage is `O(ell*d)` plus SVD temporaries during a shrink.

No randomized projection, mergeable/distributed sketch, sparse-row acceleration,
low-rank factor API, PCA model-selection guarantee, finite-precision spectral
proof, arbitrary-precision arithmetic, GPU/SIMD acceleration, or benchmark
speedup is claimed.

## Scope

Exactly four repository paths change:

- `include/algorithms/streaming/frequent_directions.hpp`;
- `tests/test_frequent_directions_cases.hpp`;
- one registry include in `tests/test_main.cpp`;
- `docs/scope_recovery_frequent_directions.md`.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, weighted-cycle-basis, or temporary-file churn is
included. Merge should be squash if connector transport creates more than one
construction commit, so `main` receives one recovery checkpoint.
