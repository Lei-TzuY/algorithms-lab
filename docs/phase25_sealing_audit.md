# Phase 25 Sealing Audit

## Checkpoint

Phase 25 introduced exact bidirectional BWT interval extension and merged into `main` as `8b0b9618ee1e53b47b36ef47e3ac6149d223902d`. Post-merge CI run `34181328896` completed successfully for GCC release, Clang release, and GCC ASan+UBSan.

This audit is the phase-completion gate. It does not add production behavior.

## Correctness review

The implementation maintains two half-open conceptual suffix-array intervals of equal cardinality: the forward interval represents pattern `P`, while the reverse interval represents `reverse(P)` in the reversed text.

Left extension performs the sealed forward BWT backward step and projects the peer reverse interval. Right extension applies the symmetric operation through the reversed-text index. In both directions the peer offset counts the conceptual sentinel before byte partitions and then sums occurrence differences for byte values strictly smaller than the extension byte.

The review checked these obligations:

- interval bounds are validated before extension;
- paired interval cardinalities must agree;
- peer projection checks `offset + match_count` against the source peer cardinality without unchecked addition;
- conceptual-sentinel membership is counted exactly once when present in the source interval;
- arbitrary bytes `0..255` remain ordinary data;
- zero-cardinality absent intervals remain exact insertion-point states under further extension;
- the implementation reuses the sealed `BwtByteIndex::extend` and occurrence machinery rather than duplicating suffix search.

No correctness blocker was found.

## Verification independence

The Phase-25 tests construct suffix rows with an independent raw bytewise suffix sorter. After every mixed left/right extension, they compare all four interval endpoints and the shared match count against independently computed forward/reverse suffix-array intervals and also compare the count with a direct substring scan.

Coverage includes empty text/pattern, absent-state continuation, repeated strings, `0x00`, `0xFF`, every possible single byte, and fixed-seed randomized mixed-direction extension sequences.

The oracle does not call the production suffix-array builder, the production BWT backward-search recurrence, or the bidirectional projection routine.

## Complexity and scope claims

The sealed baseline intentionally exposes the fixed byte alphabet in its cost: each extension performs one ordinary run-length BWT backward step plus at most 256 occurrence-rank differences, conservatively `O(256 log(R+1))` with the current run-length occurrence representation.

The bidirectional index owns full forward and reversed `BwtByteIndex` instances. Phase 25 therefore makes no r-index, compressed-construction, resident-space-optimal, approximate-matching, or asymptotically optimal bidirectional-index claim.

## Phase decision

Phase 25 is **SEALED**. Additional exact interval variants would add little new architectural depth.

The promoted frontier is Phase 26: bounded Hamming-distance substring matching over arbitrary bytes. The first slice must reuse the sealed bidirectional extension state, support substitutions only (no insertion/deletion semantics), expose explicit mismatch-budget and search/pruning diagnostics, and verify exact results against an independent direct-scan Hamming oracle. Any exponential dependence on mismatch budget/alphabet exploration must be stated rather than hidden behind an "approximate matching" label.
