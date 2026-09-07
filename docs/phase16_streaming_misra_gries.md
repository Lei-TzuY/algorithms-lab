# Phase 16 — Misra–Gries streaming heavy hitters

## Scope

This slice adds a true one-pass Misra–Gries state machine for signed 64-bit item
identifiers. The parameter `k >= 2` allows at most `k-1` live counters. The API
supports incremental `update()` calls as well as a one-shot convenience wrapper;
the one-shot path is implemented by feeding the same streaming state, not by an
offline frequency table.

## State transition

For an arriving item `x`:

1. if `x` is already tracked, increment its residual counter;
2. otherwise, if fewer than `k-1` counters are live, insert `x` with count one;
3. otherwise the table is full: conceptually cancel the new occurrence of `x`
   together with one occurrence from every tracked item, decrement every counter,
   erase zero counters, and increment the decrement-round witness `D`.

Counters are kept internally in deterministic insertion order. `summary()` sorts
its returned snapshot by item so repeated runs have a canonical presentation.

## Cancellation invariant

Every full-table miss cancels exactly `k` distinct stream occurrences: the
untracked incoming occurrence plus one unit from each of the `k-1` tracked
counters. Therefore, after processing `n` items,

`sum(residual_counts) = n - k*D`.

The implementation returns `D` explicitly. Tests replay this identity rather than
reporting only the final candidate set.

## Frequency guarantee

For every item `x`, let `f(x)` be its exact stream frequency and let `c(x)` be the
returned residual count, using zero when `x` is absent from the summary. Standard
Misra–Gries cancellation reasoning gives

`0 <= f(x) - c(x) <= D <= floor(n/k)`.

Consequently every item with `f(x) > n/k` must remain a candidate. This is a
deterministic additive-error theorem, not a probability statement. Tests provide
executable evidence for the implementation; they do not replace the proof.

## Verification

- exhaustive streams over alphabet `{-1,0,1}`, lengths `0..8`, and `k=2..4`:
  29,523 complete streams;
- each exhaustive case checks the cancellation identity, counter-capacity bound,
  per-item additive error, and mandatory retention of true heavy hitters;
- 1,200 fixed-seed randomized streams compare residual bounds against an exact
  frequency map;
- incremental and one-shot execution are checked for identical summaries;
- deterministic output and decrement-round witnesses are checked explicitly;
- GCC and Clang strict-warning builds plus ASan+UBSan focused execution pass.

## Complexity and boundary

This direct educational implementation stores at most `k-1` counters, so streaming
state is `O(k)`. It uses a vector intentionally: finding a counter and a full-table
decrement are each `O(k)` worst case, so one update is `O(k)` worst case and a
stream of length `n` is `O(nk)` worst case. No `O(1)`-update claim is made.

Counts and the processed-item/decrement witnesses use `std::size_t`; explicit
overflow checks reject unrepresentable state instead of wrapping.

## Frontier

Phase 16 implementation is complete; phase-level sealing/integration audit is the
next gate before any further promotion.
