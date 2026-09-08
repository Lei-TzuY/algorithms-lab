# Phase 31 Sealing Audit

## Decision

Phase 31 is sealed. The periodic-sample local BWT extraction capability is implemented on `main`, the exact merged-main CI matrix is green, and the remaining unchecked ROADMAP state is status debt rather than missing production behavior.

## Executable capability

`BwtPeriodicSampleTextExtractor` re-indexes the sealed Phase-20 periodic locate samples by suffix position and exposes exact half-open extraction without retaining a constructor-text copy. For a non-empty request `[begin,end)`, production chooses the nearest sampled suffix position `q >= end`, starts from that sample's conceptual BWT row, and LF-walks backward only until `begin`.

The implementation exposes exact returned bytes, LF-step count per extraction, inverse periodic-sample count, and logical inverse-sample payload bytes. Constructor validation rejects inconsistent sample cardinality, duplicate or missing inverse slots, invalid suffix positions, and payload-size overflow.

## Correctness evidence

The production invariant is independently exercised by direct source-substring comparison rather than by reconstructing the expected answer through BWT/LF logic. Deterministic coverage includes empty text/ranges, classic `banana`, sample rate one, non-periodic tail samples, arbitrary NUL/high bytes, sample rates larger than the text, payload/cardinality checks, invalid ranges, and a short range that performs fewer LF steps than the sealed Phase-29 full-reconstruction baseline.

Randomized differential verification runs 300 source texts with varied lengths and locate sample rates, with 30 random ranges per text, comparing every result directly with `std::string::substr` and checking the LF-step bound and sample-payload invariants.

The integrated `main@9d56d8613467e8279312beb499b9d15237750325` CI run `34193363155` completed successfully on GCC release, Clang release, and GCC ASan+UBSan; configure, build, and test steps all succeeded in every job.

## Cost and claim boundary

For `S` periodic samples, extractor construction is `O(S)` time and `O(S)` additional resident row identifiers. For a requested range of length `L` and locate sample rate `r`, the direct bound is `O(L+r)` LF/rank-access work with `O(L)` output storage. Production checks `L <= lf_steps <= L + r - 1` for every non-empty request.

This is not a universal speedup, compressed-optimal random access, or r-index extraction claim. Large sample rates can make short requests expensive, and full-range extraction remains linear.

## Architecture audit

Phase 31 cleanly adds a local extraction substrate without changing Phase-30 search semantics. It reuses the sealed Phase-20 sample policy and the existing BWT/LF representation, while leaving the sealed Phase-29 full reconstruction available as an explicit baseline. No unresolved production correctness or integration blocker was found, and adding more extraction variants inside Phase 31 would now be low-value surface farming.

## Promotion

The next coherent cross-layer gap is to use the sealed local extractor inside the sealed Phase-30 seed-and-verify bounded edit-distance pipeline. Phase 32 should preserve the complete `k+1` seed candidate-generation argument but verify each candidate from exact local extraction windows instead of reconstructing the entire source text once per query.

That next slice must expose candidate/window/extracted-byte/LF-work diagnostics, compare exact positions against the sealed Phase-30 implementation and an independent direct-DP substring oracle, cover arbitrary bytes and varied sample rates, and keep an explicit non-universal-speedup boundary: dense candidate sets or large sample rates can still approach or exceed the old full-reconstruction work.
