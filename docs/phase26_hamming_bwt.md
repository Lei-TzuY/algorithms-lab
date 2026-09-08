# Phase 26 — Bounded Hamming-Distance BWT Search

## Scope

Phase 26 adds exact fixed-length substring search with a bounded number of byte substitutions. It is approximate in the query relation, not in the returned result: every returned position has Hamming distance at most the caller's budget and every such text window is returned.

The implementation deliberately builds on the sealed Phase-25 `BidirectionalBwtByteIndex`. It does not introduce a direct-scan production path and it does not implement insertion/deletion edit distance.

## Deterministic center-out state expansion

For a non-empty pattern of length `m`, the search anchors the middle pattern byte and grows one contiguous candidate substring outward in a fixed schedule: center, one byte left, one byte right, then alternates outward while a side remains.

Each frontier node contains only a sealed bidirectional exact interval state plus the number of substitutions already spent. The expected pattern byte creates a zero-cost transition. If budget remains, every other byte value `0..255` creates a one-substitution transition. Any transition whose exact interval becomes empty is pruned immediately.

This schedule is deterministic and exercises both Phase-25 extension directions; it is not a heuristic seed selection.

## Result reconstruction and invariants

Terminal states represent distinct full-length candidate byte strings, so their forward suffix-array intervals must be disjoint. Each matched conceptual suffix row is resolved through the sealed forward BWT locate machinery. For a non-empty pattern, every reconstructed position must be inside the text and leave room for the full pattern length.

Returned positions are sorted. A duplicate reconstructed position is treated as an internal invariant failure instead of being silently removed, because duplicates would mean two supposedly distinct terminal exact intervals overlap.

The empty pattern keeps the repository-wide convention of matching all `n+1` text boundaries. A non-empty pattern longer than the text returns no matches without expanding the search frontier.

## Diagnostics

The result exposes deterministic counts for expanded states, transitions considered, empty-interval prunes, surviving terminal states, and peak frontier size. Counter overflow is rejected rather than wrapped.

## Complexity boundary

Let `m` be pattern length, `k = min(max_substitutions, m)`, and `R` the relevant run count in the sealed Phase-25 index. The number of possible full candidate strings within budget is

`S(m,k) = sum_{j=0..k} C(m,j) * 255^j`.

The direct baseline may therefore have exponential dependence on `k` and the byte alphabet. Search cost is proportional to explored prefix states times the sealed Phase-25 extension cost `O(256 log(R+1))`; Phase 26 makes no polynomial-time, seed-and-extend, Levenshtein, or optimized approximate-index claim. Terminal row reconstruction additionally inherits the sealed sampled-BWT LF-walk bound.

## Verification

The primary oracle is an independent direct scan over every fixed-length text window. It counts byte mismatches directly and never calls BWT, suffix-array, or bidirectional-extension production code.

Coverage includes budget zero, budget at least pattern length, empty text/pattern, patterns longer than text, overlapping occurrences, arbitrary bytes including `0x00`/`0xFF`, deterministic diagnostics replay, and fixed-seed randomized full-byte differential cases.

Focused state-machine verification passed strict GCC, strict Clang, and actual ASan+UBSan using an independent exact suffix/BWT stub. Exact remote PR CI and the merged-main GCC/Clang/ASan+UBSan matrix also passed on the current run-length BWT and sampled-locate backend.

## Status

Phase 26 is sealed. The bounded-substitution state machine, exact locate reconstruction, deterministic diagnostics, direct-scan differential oracle, and explicit exponential-in-budget/alphabet boundary are integrated on verified `main`. Further Hamming scheduling/branch-order variants are intentionally out of scope; the next architectural frontier is bounded edit-distance search with insertion/deletion state semantics.
