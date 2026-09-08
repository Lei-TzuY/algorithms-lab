# Phase 27 — Bounded Edit-Distance BWT Search

## Scope

Phase 27 extends the sealed BWT approximate-search surface from fixed-length Hamming distance to bounded byte-oriented Levenshtein distance. The returned values are exact substring start positions: a position is returned iff at least one substring beginning there is within the requested edit budget of the pattern.

The baseline supports unit-cost substitution, insertion, and deletion. It does not implement affine gaps, weighted edits, seed-and-extend heuristics, probabilistic filtering, or a polynomial-time approximate-index guarantee.

## Right-to-left BWT state machine

Search consumes the pattern from right to left so every text-consuming transition can reuse the sealed `BidirectionalBwtByteIndex::extend_left` operation.

A state contains:

- the sealed exact bidirectional BWT interval for the candidate text string constructed so far;
- the number of pattern bytes already consumed from the right;
- the edit cost spent;
- the exact candidate bytes, retained only so terminal witnesses can be resolved through the sealed exact BWT locate path.

Transitions are:

1. **match** — consume one pattern byte and prepend the same byte, cost 0;
2. **substitution** — consume one pattern byte and prepend any different byte, cost 1;
3. **pattern deletion** — consume one pattern byte without extending the BWT state, cost 1;
4. **text insertion** — prepend any byte without consuming a pattern byte, cost 1.

Empty exact intervals are pruned immediately. Zero-cost match transitions go to the front of a deque and unit-cost edits go to the back, giving a deterministic 0-1 search order.

## Dominated-state deduplication

Insertion/deletion/substitution scripts can converge on the same exact search state. A state key consists of pattern progress, candidate length, and both forward/reverse exact intervals. The search retains the smallest edit cost seen for each key. An equal-or-more-expensive duplicate is discarded; if a cheaper route appears, the previous queued copy becomes stale.

Unlike Phase 26, duplicate **positions** across different terminal candidate strings are legitimate: variable-length strings within the Levenshtein ball can begin at the same text position. Positions are therefore unioned, not treated as an invariant failure.

## Exact terminal reconstruction

Every non-empty terminal candidate is resolved through the sealed BWT-only exact path by invoking zero-budget exact Hamming locate for that candidate. No source-text scan is introduced. A query whose pattern is empty, or whose budget can delete the entire pattern, returns every `n+1` text boundary immediately because the empty substring is already a valid witness.

This terminal delegation intentionally repeats exact interval work instead of adding a new private row-resolution API. The cost is part of the first-principles baseline and is not hidden in a stronger complexity claim.

## Diagnostics

The result exposes:

- expanded non-stale states;
- semantic transitions considered;
- empty exact-interval prunes;
- dominated-state prunes;
- terminal states processed;
- peak deque size.

All diagnostic counters reject overflow instead of wrapping. Repeating a query on the same immutable index must reproduce both positions and diagnostics exactly.

## Complexity boundary

The explored state space can grow exponentially with the edit budget and the 256-byte alphabet. Each surviving text-consuming branch inherits the sealed bidirectional BWT extension cost, and each distinct terminal candidate additionally pays for an exact BWT locate. The domination map uses ordered-map lookup and the result union uses `O(n)` query-local boundary storage.

Phase 27 deliberately makes no seed-and-extend, affine-gap, bit-parallel, polynomial-time approximate-index, r-index-optimal, or universal performance claim.

## Verification

The primary oracle is independent dynamic programming. For every text start boundary, tests enumerate feasible substring lengths from `max(0, m-k)` through `min(remaining, m+k)` and compute ordinary byte-oriented Levenshtein distance directly. No BWT, suffix-array, Hamming-search, or production state-machine routine is used by this oracle.

Coverage includes deterministic substitution/insertion/deletion witnesses, zero-budget equality with sealed exact search, deletion-to-empty boundary semantics, arbitrary `0x00`/`0xFF` bytes, deterministic diagnostic replay, and fixed-seed randomized differential cases.

A focused state-machine harness passed strict GCC, strict Clang, and actual ASan+UBSan before remote integration. The exact implementation PR and merged-main integration both passed the repository GCC release, Clang release, and GCC ASan+UBSan matrix.

## Status

**SEALED.** PR #71 merged as `main@ca194938af5fa05cd09dfd02d90d61197c5ee0d0`, and merged-main CI run `34184763972` completed successfully across GCC release, Clang release, and GCC ASan+UBSan. The independent post-merge audit found no unresolved correctness, witness-reconstruction, oracle-independence, or complexity-claim blocker. See `docs/phase27_sealing_audit.md`.

The next promoted frontier is Phase 28: bounded affine-gap BWT search. It must add explicit gap-open/gap-extend state without weakening the exact witness or cost-boundary discipline sealed here.
