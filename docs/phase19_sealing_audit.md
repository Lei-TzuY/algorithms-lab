# Phase 19 sealing audit

## Gate

Phase 19 is sealed only after the exact BWT implementation reached `main` and the merged-main GCC release, Clang release, and GCC ASan+UBSan jobs all completed successfully.

## Capability boundary

The phase establishes an exact byte-oriented BWT backward-search index:

- Phase-5 suffix-array order supplies the augmented suffix rows;
- one conceptual sentinel is tracked outside the 0..255 byte alphabet;
- Phase-18 wavelet-matrix rank supplies exact BWT occurrence counts;
- LF/backward-search intervals produce exact substring counts;
- retained row-to-text positions produce exact locate results.

The empty-pattern boundary remains consistent with the sealed KMP contract: every text boundary `0..n` is a match.

## Verification independence

The production search recurrence is not used as its own oracle. Deterministic adversarial tests cover overlaps, repeated bytes, binary values, and a text containing all 256 byte values. The fixed-seed randomized corpus compares every count and located position against direct substring scanning. An additional exhaustive formulation check covers all length-0..6 texts over a three-symbol alphabet and all patterns of length at most three.

This evidence exercises the implementation and the conceptual-sentinel mapping; it does not replace the LF-mapping proof obligation documented in the Phase-19 design note.

## Claim audit

The phase makes no succinct-FM-index claim. `BwtByteIndex` deliberately retains the complete augmented suffix-row position vector, so locate storage is still linear in the number of rows. Construction remains `O(n log^2 n)` because it reuses the current comparison-sorted suffix array. Count is `O(m)` for byte-pattern length `m`; locate additionally sorts `k` returned positions.

No wall-clock CI measurement is presented as a benchmark or complexity result.

## Promotion decision

Adding more wrappers around the same backward-search interval would be low-value breadth. The next substantial frontier is a representation-level change: sampled locate.

Phase 20 should keep the exact BWT/wavelet search core while replacing the full row-position vector with:

- a packed sampled-row membership bitvector;
- periodic suffix-position samples;
- LF walking from an unsampled result row to the nearest sampled suffix position.

Sampling every `r` text positions (and always sampling the sentinel position) gives a concrete tradeoff: row-position storage becomes a packed bitvector plus `O(n/r)` positions, while each located row requires fewer than `r` LF steps before sample recovery. This is a genuine architecture/space-time promotion rather than another string-search variant.

## Seal result

Phase 19 is complete and should not receive further BWT query variants merely to increase algorithm count. Phase 20 is the active frontier, subject to its own exact implementation, storage accounting, adversarial tests, randomized direct-scan differential verification, and full merged-main CI.
