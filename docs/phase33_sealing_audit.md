# Phase 33 sealing audit

## Integrated checkpoint

Phase 33 was implemented by PR #83 and merged as `a182dccc5b18ed08ececb0f4fdd9a5a3d3260988`.

The exact merged-main CI run `34317741938` completed successfully on all repository gates:

- GCC release;
- Clang release;
- GCC ASan+UBSan.

There are no open pull requests or open issues at the sealing checkpoint.

## Correctness boundary

Phase 33 changes only verification scheduling. Candidate generation remains the sealed Phase-32 candidate set, including the Phase-30 `k+1` exact-seed completeness argument and `+-k` start displacement. Every candidate retains its original Phase-32 bounded verification window and independent bounded-Levenshtein DP work.

Overlapping or touching windows are coalesced only for extraction. Each merged interval is extracted once through the sealed Phase-31 periodic-sample extractor, and each candidate is verified from the exact slice corresponding to its original window. Therefore batching cannot introduce or remove a candidate merely by changing the extraction grouping.

## Verification evidence

The merged implementation compares every result against both sealed Phase 32 and an independent direct substring Levenshtein-DP oracle. Seed counts, seed occurrences, unique/verified candidate counts, and DP-cell counts are required to remain identical to Phase 32.

Deterministic coverage includes boundary fast paths, substitutions, insertions/deletions, arbitrary bytes, sparse candidates, sample-rate variation, and a dense clustered-candidate regression. Fixed-seed randomized arbitrary-byte cases exercise varied text/pattern lengths, budgets, and sample rates. The clustered workload demonstrates a real measured reduction in merged-window count, extracted bytes, and LF work without turning that observation into a universal performance claim.

## Claim audit

The phase does **not** claim universal speedup, sublinear worst-case search, compressed-optimal extraction, r-index bounds, or shared dynamic-programming acceleration. Coalescing is `O(C)` because candidate windows are already ordered; exact candidate verification remains `O(C * m * (m+k))`. Periodic-sample alignment can erase an LF-step benefit even when extracted-byte duplication is reduced.

No correctness, sanitizer, integration, or representability blocker remains at the merged-main checkpoint.

## Why the phase stops here

Further variants that only change overlap thresholds, merge ordering, or window-packing heuristics would farm the same verification-scheduling surface without adding a new algorithmic correctness contract. Phase 33 is therefore sealed after one coherent batching slice.

## Promotion decision

The coverage audit promotes the next substantial architectural gap rather than another BWT micro-optimization. The repository already contains static/heavy-light tree decomposition, rollback connectivity, and a sealed splay-tree ordered set, but it lacks an online dynamic-forest data structure that changes represented-tree topology while preserving path queries.

Phase 34 therefore targets a first-principles link-cut tree supporting dynamic-forest `link`, `cut`, connectivity, and exact path edge-distance. The implementation must distinguish auxiliary-splay structure from represented-forest structure, reject cycle-creating links and non-edge cuts, expose the standard amortized rather than worst-case-per-operation complexity boundary, and verify long randomized traces against an independent naïve forest/BFS oracle.
