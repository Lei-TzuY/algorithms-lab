# Scope recovery: deterministic Greenwald-Khanna quantile summary

## Coverage decision

A fresh post-Phase-69 live-code, pull-request-history, and recovery-authority
review after the merged Berlekamp-Massey checkpoint found no Greenwald-Khanna
summary, deterministic streaming quantile synopsis, or equivalent rank-error
summary. Existing streaming structures cover frequency estimation / heavy hitters,
and the wavelet-matrix family answers exact queries only after retaining a static
sequence index. Neither maintains a one-pass compact ordered summary with an
explicit deterministic rank-error invariant.

This slice follows `scope_recovery_after_phase69.md`: the frozen compiler/backend
surface and stale historical ROADMAP frontier remain untouched. It deliberately
moves away from the immediately preceding FKS hashing and linear-recurrence slices
and introduces a different proof model: rank intervals plus deterministic tuple
compaction.

## Production contract

`GreenwaldKhannaSummary(error_denominator)` accepts a denominator `d >= 2` and
processes signed 64-bit stream values online. For a non-empty stream of length
`n`, the public integer error budget is

`E = ceil(n / d)`.

`query_rank(r)` accepts a 1-based target rank `1 <= r <= n` and returns a stored
value whose exact stream occurrence interval intersects `[r-E, r+E]` after
clamping at the stream boundaries. Empty summaries return `nullopt`.

The public tuple replay state is sorted by value and stores `(value, g, delta)`.
The running sum of `g` values is the tuple's lower rank; adding `delta` gives its
upper rank. The first and last tuples retain zero uncertainty. Construction is
deterministic and contains no hidden randomness or floating-point epsilon.

## Rank invariant and insertion

Let `rmin_i` be the prefix sum of `g` through tuple `i` and
`rmax_i = rmin_i + delta_i`. The maintained gap invariant is

`g_i + delta_i <= 2E`.

A new minimum/maximum receives `(g=1, delta=0)`. For an interior insertion before
successor tuple `s`, production uses

`delta_new = g_s + delta_s - 1`.

The new tuple therefore inherits the old successor's upper-rank bound while its
lower rank is exactly one after the predecessor lower rank. Since the successor
already satisfied the gap invariant, the inserted tuple does too. The vector
insertion occurs before the stream count is committed, so allocation failure does
not leave `count()` ahead of resident tuple state.

## Compression proof obligation

A left tuple may be removed into its right neighbor only when

`g_left + g_right + delta_right <= 2E`.

Production adds `g_left` to `g_right` and leaves the right tuple value / delta
unchanged. Thus the right tuple keeps the same lower and upper rank interval it
had before compaction, while the merged gap continues to satisfy the invariant.
The first and last tuples are never removed as left merge operands, preserving
exact stream extrema.

For a query rank `r`, let `L = max(1, r-E)` and choose the first tuple whose lower
rank is at least `L`. The preceding lower rank is `< L`; therefore the chosen
upper rank is less than `L + (g+delta) <= L + 2E <= r+E` (with the first-tuple
boundary handled exactly by zero delta). Hence the chosen tuple's feasible rank
interval lies inside the requested error window. The implementation searches for
that condition directly and treats failure to find one as an internal invariant
error.

This is the code-local proof boundary. Tests independently inspect the exact
sorted stream; they do not derive expected answers from GK tuples.

## Verification

Repo-native focused verification passed before upload under:

- GCC C++20 strict warnings-as-errors;
- Clang C++20 strict warnings-as-errors;
- actual GCC ASan+UBSan with leak detection.

Deterministic coverage includes invalid denominators, empty semantics, rank bounds,
full signed-64 value boundaries, duplicates, deterministic replay, and a 5,000-item
increasing stream whose resident summary is strictly smaller than the input (and
below 200 tuples for `d=50`).

The small-domain primary oracle exhausts every stream over `{-1,0,1}` for lengths
0 through 7, four denominators, and every legal target rank. For each returned
value, tests independently sort the original stream and require at least one exact
occurrence rank to intersect the promised target window.

A second fixed-seed differential corpus runs 240 streams of length up to 512 with
heavy duplicate noise plus `INT64_MIN` / `INT64_MAX`. Structural invariants are
checked after every insertion, several ranks are queried at regular checkpoints,
and rebuilding the same stream must reproduce the exact public tuple vector.

Finite testing is implementation evidence. The deterministic rank guarantee rests
on the tuple-rank invariant and compression argument above, not on empirical
success frequency.

## Complexity and non-claims

Let `m` be the current resident tuple count.

- locating an insertion point uses `O(log m)` comparisons, while vector insertion
  costs `O(m)` moves;
- this direct eager compression scans `O(m)` tuple positions and may perform
  repeated vector erases, so one insertion is conservatively bounded by `O(m^2)`
  time in this implementation;
- rank query scans at most `m` tuples: `O(m)` time;
- resident state is `O(m)`.

This slice does **not** claim the strongest classical Greenwald-Khanna asymptotic
space bound for this exact eager/vector strategy, does not claim mergeability of
independently built summaries, and does not provide weighted updates, deletion,
windowed quantiles, floating-point epsilon, randomized KLL-style guarantees, or
benchmark-backed performance claims.

## Scope

Exactly three repository files change: the header-only production summary, the
existing registered streaming test translation unit (`tests/test_count_min.cpp`),
and this recovery proof document. No CMake, README, ROADMAP, recovery-authority,
workflow, benchmark, or frozen compiler/backend files change.
