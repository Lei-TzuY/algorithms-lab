# Phase 23 — run-boundary-sampled full BWT locating

## Scope

Phase 23 adds an exact full-occurrence locating path that reuses the Phase-22 full-conceptual-BWT run-boundary suffix-position samples and deliberately does **not** consult the Phase-20 periodic suffix samples.

`BwtByteIndex::locate_run_sampled(pattern)` returns the complete sorted set of matching text positions. Empty-pattern semantics remain the repository convention `0..text_size()`.

The existing `locate()` remains available with the Phase-20 bounded periodic-sample LF walk. Phase 23 is an additional space/time point, not a silent replacement of that API.

## Row-resolution invariant

Let the conceptual BWT contain `n+1` rows and let row `r` have suffix-array value `SA[r]` in `0..n`.

For every row in the exact backward-search interval, Phase 23 repeatedly applies LF until reaching either:

- the conceptual-sentinel row, treated as an implicit sample with suffix position zero; or
- a Phase-22 run-boundary row with its stored exact suffix position.

Each LF step decrements the suffix-array value modulo `n+1`:

`SA[LF(r)] = SA[r] - 1 (mod n+1)`.

Therefore, if `k` LF steps carry the original row to a sample with known suffix position `s`, then

`SA[original] = s + k (mod n+1)`.

The implementation computes this modular addition without overflowing `size_t`. It also fails fast if a walk performs `n+1` steps without reaching either kind of sample, if a run-sample ordinal is inconsistent, or if a stored/reconstructed suffix position escapes `0..n`.

The single-sentinel BWT LF permutation forms one cycle over all conceptual rows, so the sentinel alone is already a correctness backstop. Phase-22 run-boundary samples reduce many walks but are not required to invent any new resident sampling structure.

## Independence from Phase-20 periodic samples

The Phase-23 implementation lives in `src/strings/bwt_run_sampled_locate.cpp` and does not read `sampled_rows_`, `sampled_positions_`, or call the periodic resolver used by `locate()`.

Tests construct the same text with periodic sample rate `1` and with a rate larger than the text. The periodic sample populations differ, but `locate_run_sampled()` must return the same complete direct-scan occurrence set in both objects.

No new resident suffix-position arrays are added in Phase 23. The only run-aware locating state remains the Phase-22 boundary row/position vectors, whose logical payload is `O(R)`.

## Complexity boundary

Let:

- `occ` be the number of matches;
- `n` be text length;
- `R` be the resident BWT run count.

Backward search keeps the existing Phase-21 conservative rank cost. For full locating, each matched row may in the worst case walk `O(n)` LF steps before reaching a run-boundary sample or the sentinel. Each step uses run-length rank/access and a binary search over `O(R)` boundary samples, conservatively `O(log R)`.

The intentionally conservative full bound is therefore

`O(m log R + occ * n * log R + occ log occ)`,

including final sorting of returned positions. Query output storage is `O(occ)`, and Phase 23 adds no resident index arrays.

This is a deliberate resident-space/query-time tradeoff. It is **not** an r-index locate bound, not a claim that LF walks are short on all inputs, and not evidence of compressed construction. Suffix-array construction and temporary `O(n)` build workspace remain inherited from earlier phases.

## Verification

Deterministic tests cover:

- empty text and empty/absent patterns;
- `banana` overlaps;
- periodic sample-rate independence;
- a long highly repetitive text;
- the Phase-22 conceptual-sentinel run-split adversary;
- arbitrary bytes including `0`, `128`, and `255`.

Randomized differential verification uses 350 fixed-seed texts of length `0..80`, two deliberately different periodic sample rates per text, and 60 patterns per text. Both Phase-23 instances must exactly equal an independent direct substring scan. Selected queries are also cross-checked against the sealed periodic-sample `locate()` path.

A separate theorem prototype exhaustively resolved every conceptual BWT row over thousands of small random byte strings using only run-boundary samples plus the sentinel, validating the modular LF reconstruction recurrence independently from the production implementation.

Randomized equality is implementation evidence. The exactness argument comes from the LF/SA modular recurrence and the guaranteed eventual sample/sentinel hit; no empirical corpus is used to infer a universal time bound.

## Phase status

Phase 23 is **SEALED** after the implementation reached `main@03181d5d949c3f8b0a7fa9a085c1fc8c7c40f6a0` and merged-main CI run `34177039499` passed GCC release, Clang release, and GCC ASan+UBSan. The final architecture audit is recorded in `docs/phase23_sealing_audit.md`.

The next frontier is Phase 24 query-local memoized run-sampled locating: preserve the same `O(R)` resident run-boundary samples, spend `O(n)` query workspace to memoize resolved conceptual rows, and bound newly traversed LF rows to at most `n+1` per query. This is an explicit time/query-memory tradeoff, not an r-index or compressed-construction claim.
