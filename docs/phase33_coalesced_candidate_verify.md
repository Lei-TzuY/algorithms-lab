# Phase 33 — coalesced candidate-window exact verification

Phase 33 preserves the sealed Phase-32 exact candidate set and bounded byte-oriented Levenshtein semantics while changing verification granularity from one extraction per candidate to one extraction per merged candidate-window group.

## Contract

`locate_bwt_coalesced_seed_verify_edit_distance` returns exactly the same substring start positions as sealed Phase 32. Empty patterns and `k >= pattern.size()` keep the repository boundary convention that every source boundary matches.

For non-trivial searches, Phase 33 reuses one private candidate builder with Phase 32. Candidate starts therefore retain the sealed Phase-30 `k+1` exact-seed completeness argument and the same `[-k,+k]` start displacement. Only verification scheduling changes.

Every actually verified start has the same Phase-32 window `[start, min(n, start + m + k))`. These windows are generated in ascending start order and coalesced whenever the next interval overlaps or touches the current merged interval. Each merged interval is extracted exactly once through the sealed Phase-31 `BwtPeriodicSampleTextExtractor`; each candidate is then verified against its original window as a `string_view` slice into that shared buffer.

## Diagnostics

The result exposes the sealed seed/candidate/DP counters plus:

- original candidate-window count;
- merged extraction-window count;
- total bytes extracted across merged windows;
- total LF transitions spent on merged extraction.

`candidate_windows == verified_candidates`, and `merged_extraction_windows <= candidate_windows`. Since merged ranges are interval unions, total extracted bytes cannot exceed the Phase-32 per-candidate extracted-byte total. No universal LF-step reduction is claimed because periodic-sample alignment contributes per-window slack; actual LF work remains measured and exposed.

## Verification

The native tests compare every Phase-33 position set against both:

1. the sealed Phase-32 candidate-local verifier; and
2. an independent direct substring Levenshtein-DP oracle.

They also require the seed count, seed-occurrence count, unique/verified candidate counts, and exact DP-cell count to equal sealed Phase 32, proving that batching changes extraction scheduling rather than candidate or DP semantics.

Deterministic coverage includes empty/large-budget fast paths, substitutions, insertions/deletions, embedded NUL/high bytes, a sparse one-window case, varied periodic sample rates, and a dense clustered-candidate case. The clustered regression requires one merged extraction window and strictly fewer extracted bytes and measured LF steps than Phase 32 for that workload.

Fixed-seed randomized arbitrary-byte cases vary text length, pattern length, edit budget, and locate sample rate while checking exact three-way equality.

## Complexity boundary

Let `C` be the number of verified candidates, `G` the number of merged windows, `U` the total number of bytes in their interval union, `m` the pattern length, `k` the edit budget, and `s` the locate sample rate.

Candidate generation is unchanged from Phase 32. Candidate windows are already produced in ascending start order, so coalescing is `O(C)` without an additional sort. Extraction performs at most `U + G(s-1)` LF transitions and stores at most one merged window at a time. Exact DP work remains `O(C * m * (m+k))` in the baseline because each candidate still receives an independent bounded edit-distance verification.

Dense or disjoint candidate sets can erase the batching benefit, and a large merged interval may approach full-text extraction. Phase 33 therefore makes no universal speedup, sublinear worst-case, compressed-optimal extraction, r-index, or shared-DP claim.

## Phase boundary

Phase 33 is one coherent batching step: it removes redundant overlapping extraction work while preserving sealed search exactness. Further progress should not farm nearby window-merging variants; after integration, the next decision belongs to a fresh architecture audit.
