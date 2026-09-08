# Phase 26 Sealing Audit

## Checkpoint

Phase 26 seals bounded-substitution approximate substring search on top of the Phase-25 bidirectional BWT extension state.

Implementation PR #69 merged as `main@4b3386f84d9ee93f06c2cb8ef02f30e95dd6db88`. Its exact pull-request CI passed GCC release, Clang release, and GCC ASan+UBSan. The merged-main run `34183381797` subsequently passed the same three jobs, including their Build and Test steps.

## Capability boundary

The sealed query relation is fixed-length Hamming distance over arbitrary byte strings. Search may spend a bounded number of substitutions, but it does not insert or delete bytes. Returned positions are exact witnesses: every reported fixed-length text window satisfies the requested mismatch budget.

Production reuses the sealed bidirectional BWT exact intervals and sampled/run-aware locate machinery. There is no direct-scan fallback, suffix-array-search bypass, probabilistic path, or hidden seed-and-extend implementation.

The deterministic center-out schedule exercises both left and right exact interval extension. Empty intervals are pruned immediately, terminal exact intervals are resolved through the sealed BWT position resolver, and duplicate reconstructed positions are treated as invariant violations rather than silently hidden.

## Correctness evidence

The primary oracle is structurally independent: tests directly scan each fixed-length text window and count byte mismatches without calling BWT, suffix-array, or bidirectional search code.

Evidence covers exact budget zero, full substitution budget, empty text/pattern, pattern longer than text, overlapping/repeated occurrences, embedded `0x00` and `0xFF`, deterministic diagnostics replay, and 240 fixed-seed randomized full-byte cases.

A read-only post-merge source audit found no unresolved correctness blocker in mismatch accounting, center-out left/right scheduling, exact-interval disjointness, locate reconstruction, or checked diagnostic counters.

## Complexity and claim discipline

For pattern length `m` and `k=min(budget,m)`, the number of candidate strings may reach `sum_{j=0..k} C(m,j) 255^j`. The sealed implementation therefore explicitly permits exponential dependence on mismatch budget and alphabet branching. It inherits the Phase-25 extension cost and the sealed sampled-BWT locate cost; it does not claim polynomial-time approximate matching, Levenshtein support, heuristic seed guarantees, r-index-optimal bounds, or probabilistic success bounds.

## Seal decision

No second Hamming-search variant is required for a coherent Phase-26 checkpoint. Alternative branch ordering, center schedules, or repeated substitution-only flows would add low-value variants without a new correctness contract.

Phase 26 is therefore SEALED.

The next substantial frontier is Phase 27: bounded byte-oriented Levenshtein search over the BWT state machine. That phase must add insertion and deletion semantics, dominated-state deduplication, variable-length candidate reconstruction, and an independent dynamic-programming substring oracle while keeping the exponential baseline cost explicit.
