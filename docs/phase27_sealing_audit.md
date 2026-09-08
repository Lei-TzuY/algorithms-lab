# Phase 27 Sealing Audit

## Integrated checkpoint

Phase 27 is sealed only after the bounded byte-oriented Levenshtein BWT search reached merged `main` and the exact merged-main CI matrix passed.

- implementation PR: #71
- merged checkpoint: `main@ca194938af5fa05cd09dfd02d90d61197c5ee0d0`
- merged-main CI: run `34184763972`, completed / success
- GCC release: success
- Clang release: success
- GCC ASan+UBSan: success
- open implementation PRs/issues at audit time: none

## Correctness boundary

The production state space covers the four unit-cost Levenshtein transitions needed by the sealed contract:

1. exact byte match;
2. substitution;
3. pattern deletion;
4. text insertion.

Search proceeds right-to-left so every text-consuming transition reuses the sealed bidirectional exact-interval extension. Empty intervals are pruned. The domination key combines pattern progress, candidate length, and paired forward/reverse exact intervals, while the map stores the least edit cost seen for that key; stale more-expensive queued states are ignored.

Variable-length edit scripts can produce distinct terminal candidate strings that begin at the same text position. The implementation therefore unions terminal positions rather than treating duplicate positions as an invariant failure. Non-empty terminal candidates are resolved through the sealed exact BWT/Hamming-zero locate path; production does not fall back to a source-text scan.

If the pattern is empty, or the budget can delete the complete pattern, the empty substring is already a valid witness at every text boundary. Returning all `n+1` boundaries is therefore part of the explicit substring-start semantics rather than a shortcut that weakens correctness.

## Oracle independence

The primary randomized oracle is structurally independent from production. It enumerates feasible substring lengths at each text boundary and computes ordinary byte-oriented Levenshtein dynamic programming directly. It does not call the BWT index, suffix array, Hamming search, bidirectional extension, or the production search state machine.

Deterministic coverage separately exercises substitution, insertion, deletion, zero-budget equality with sealed exact search, arbitrary bytes, deletion-to-empty semantics, and deterministic diagnostics replay.

## Complexity and claim audit

No hidden polynomial-time or compressed-index-optimal claim is present. The first-principles search may grow exponentially with edit budget and alphabet branching. Each surviving text-consuming transition inherits the sealed bidirectional BWT extension cost; each distinct terminal candidate additionally pays for exact BWT locate; the ordered domination map and query-local output bitmap are accounted for explicitly.

No seed-and-extend, affine-gap, bit-parallel, probabilistic, r-index-optimal, or universal performance claim is made by Phase 27.

## Architecture conclusion

No unresolved correctness, integration, witness-reconstruction, oracle-independence, representability, or complexity-claim blocker was found. Farming additional unit-cost edit-transition variants would not add a new conceptual boundary, so Phase 27 is sealed.

A substantial next frontier does remain: affine-gap scoring changes the state machine itself because insertion/deletion runs need gap-open versus gap-extend memory. Phase 28 is therefore promoted as bounded affine-gap BWT search with an independent affine-gap dynamic-programming oracle, exact BWT witness reconstruction, deterministic diagnostics, and an explicit exponential baseline cost. It must not claim seed-and-extend, polynomial-time approximate indexing, or asymptotically optimal compressed search.
