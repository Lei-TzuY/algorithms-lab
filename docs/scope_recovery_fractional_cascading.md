# Scope recovery: static fractional cascading

## Coverage decision

This slice follows `docs/scope_recovery_after_phase69.md`: the historical
Phase-45--69 compiler/backend sequence remains frozen. A fresh live audit after
Yen k-shortest loopless paths reached exact merged-main green at
`main@c72e8d9f6c1914cfe7433e21b2b535c2518d5c14` (CI run `34712279163`,
GCC release, Clang release, and GCC ASan+UBSan all successful) found no
fractional-cascading implementation in live code or pull-request history. The pre-existing
`scope-recovery-fractional-cascading` branch had no commit ahead of current
`main` and was dozens of commits stale, so it no longer represented active work.
The still-ahead minimum-cycle-basis branch remains untouched.

The repository already contains binary search, orthogonal range trees, KD trees,
wavelet-matrix-style indexing, and Elias-Fano predecessor/search structures, but
none of those implement the classical cross-catalog bridge invariant. This slice
therefore adds a distinct static multi-catalog search proof model rather than
another graph, compiler, or one-catalog search variant.

## Production contract

`FractionalCascadingIndex` owns an immutable snapshot of a sequence of sorted
`int64_t` catalogs.

- every input catalog must be nondecreasing; duplicates and empty catalogs are
  supported;
- `lower_bound_indices(x)` returns, for every catalog, the exact insertion index
  of the first value not less than `x`;
- full signed-64-bit values are supported because the implementation only
  compares catalog values;
- preprocessing walks catalogs backward, merges each complete catalog with
  every second entry of the next augmented catalog, and stores bridge positions
  to both the original catalog and the next augmented level;
- the first augmented level is searched with a first-principles half-open binary
  search; each later level uses one stored bridge plus at most one predecessor
  correction;
- diagnostics expose augmented sizes, total augmented entries, the immutable
  catalog snapshot, and an expensive structural replay.

## Bridge invariant / proof obligation

Let `A_i` be augmented catalog `i`. For the last level, `A_last = C_last`.
For every earlier level,

`A_i = merge(C_i, every_second(A_(i+1)))`.

All entries of `C_i` therefore occur in `A_i`. If `p` is the lower-bound
position of query `x` in `A_i`, the stored original-catalog bridge at `p` is
exactly `lower_bound(C_i, x)`: any smaller original value still at least `x`
would itself occur before `p` in `A_i`.

The stored bridge into `A_(i+1)` points to the lower bound of `A_i[p].value`.
Between two entries sampled into `A_i`, at most one `A_(i+1)` entry is omitted.
Consequently the true lower bound for `x` at the next level is either that
stored bridge or its immediate predecessor. The query checks exactly that one
predecessor and never performs a second binary search.

This is the fractional-cascading proof obligation; finite randomized testing is
implementation evidence rather than a proof of the theorem.

## Complexity / space boundary

For `m` catalogs containing `N` original entries total, augmented sizes satisfy

`|A_i| = |C_i| + floor(|A_(i+1)| / 2)`.

Summing the recurrence gives fewer than or equal to `2N` augmented entries.
Construction is `O(N + m)` time, resident storage is `O(N + m)` including the
owned original catalogs and augmented bridges, and one query is
`O(log(N + 1) + m)` time. The `m` term is unavoidable because the API returns
one lower-bound index per catalog.

`valid_structure()` is a diagnostic and is not part of the query bound.

## Verification

The focused final candidate passed strict GCC, strict Clang, and actual GCC
ASan+UBSan, six test groups each.

Evidence includes:

- decreasing-input rejection;
- empty index and long chains of empty/sparse catalogs;
- duplicate-heavy catalogs and signed-64 boundary values;
- a deterministic case where the true next-level lower bound is the single
  unsampled predecessor of the stored bridge;
- immutable input-snapshot semantics;
- 900 fixed-seed random catalog chains with 0..10 catalogs, 0..32 entries per
  catalog, and 80 queries per trial;
- every result compared catalog-by-catalog against independent test-only
  `std::lower_bound`;
- structural replay and the `total_augmented_entries <= 2 * original_entries`
  bound checked on every randomized instance.

The oracle does not reuse production augmentation, bridge construction, or
bridge correction.

## Scope / non-claims

This slice is static and one-dimensional. It does not claim dynamic catalog
updates, fractional cascading over arbitrary graph-shaped catalog networks,
range reporting, persistence, comparison-model optimal constants, cache-aware
layout, or automatic acceleration of the existing orthogonal range tree.

The intended repository change is exactly four paths: one header-only production
API, one repo-native test-case header, one include in `tests/test_main.cpp`, and
this proof document. No CMake, README, historical ROADMAP, recovery authority,
workflow, benchmark, frozen compiler/backend, minimum-cycle-basis branch, or
temporary-file churn is required.
