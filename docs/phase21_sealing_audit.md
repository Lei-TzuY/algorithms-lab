# Phase 21 sealing audit

## Gate

Phase 21 is sealed only after the run-length BWT occurrence backend reached merged `main` and the exact merged-main GCC release, Clang release, and GCC ASan+UBSan jobs all completed successfully. The sealed implementation checkpoint is `main@5576e045068c739e0e4b8c0cca2ec307cf7e15b1`; CI run `34174427111` completed successfully in all three jobs.

## Capability boundary

Phase 21 changes the resident occurrence representation inside the existing BWT index rather than changing exact search semantics:

- the Phase-18 byte wavelet matrix is no longer the resident BWT occurrence backend;
- maximal equal-byte BWT runs are indexed through exact byte `access` and prefix/range `rank`;
- Phase-19 conceptual-sentinel backward search remains exact;
- Phase-20 periodic suffix-position sampling and bounded LF reconstruction remain unchanged;
- BWT run count and logical run payload are exposed as representation diagnostics.

The phase does not claim a universally smaller FM-index. The logical payload excludes vector objects, allocator metadata, and spare capacity; on non-repetitive BWTs the run count can approach the BWT length and this representation can exceed the old wavelet-matrix payload.

## Verification independence

The run-length rank/access structure is tested directly against naïve byte scans on deterministic edge cases and 1000 fixed-seed random sequences. Existing Phase-19/20 direct-scan BWT count/locate verification remains active after the backend replacement, so end-to-end correctness does not depend on reusing the run-index recurrence as an oracle.

A repetitive 2048-byte integration case demonstrates one-run BWT behavior and smaller reported logical run payload than the old wavelet representation on that same input. This is bounded representation evidence, not a universal compression theorem.

## Complexity / claim audit

For BWT length `n`, total runs `R`, and runs `R_c` carrying byte `c`, construction is `O(n)`, byte access is `O(log R)`, and byte rank is `O(log R_c)` under the implemented binary searches. Backward search therefore has conservative `O(m log R)` occurrence work, while existing sampled locate adds its LF-walk and output-sorting costs.

No succinct-space, cache-efficiency, wall-clock, or asymptotic improvement over every input is inferred from CI or the repetitive regression.

## Promotion decision

Further aliases around count/locate or alternate RLE containers would be low-value variants. A genuinely different remaining text-index gap is locating from BWT run structure itself: Phase 20 samples suffix positions periodically in text-position space, while Phase 21 exposes the BWT runs but does not use them to maintain a suffix-position witness during backward search.

Phase 22 therefore promotes a run-aware BWT toehold hypothesis. It should maintain one exact suffix-position witness through backward search using suffix-position samples at relevant BWT run boundaries, return one exact occurrence for a non-empty matched interval, handle the conceptual sentinel explicitly, expose run-aware sample storage, and verify the returned position against independent direct scan. It must not claim full r-index functionality unless all required locate structures and the corresponding complexity theorem are actually implemented.

## Seal result

Phase 21 is complete and should not receive additional run-length occurrence variants merely to increase algorithm count. Phase 22 is the active frontier, subject to an explicit toehold invariant, executable witness verification, honest run-dependent storage/query accounting, exact PR CI, and merged-main CI.
