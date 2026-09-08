# Phase 20 sealing audit

## Gate

Phase 20 is sealed only after sampled locate reached merged `main` and the exact merged-main GCC release, Clang release, and GCC ASan+UBSan jobs all completed successfully. The sealed implementation is `main@bebf1543db4c56cce7b9f155bd91beaed7b8f8a0`; CI run `34172494194` completed successfully in all three jobs.

## Capability boundary

Phase 20 changes resident locate state rather than query semantics:

- Phase-19 exact BWT backward search and conceptual-sentinel semantics are preserved;
- the full augmented row-to-position table is no longer resident after construction;
- sampled-row membership uses the sealed Phase-17 packed rank/select bit vector;
- sampled suffix positions are retained at a positive configurable rate;
- unsampled result rows reconstruct exact positions through LF walking with a production-checked `< sample_rate` bound.

Exact `count` and sorted exact `locate` remain valid for arbitrary bytes, including empty-pattern boundary matches.

## Verification independence

Existing Phase-19 direct-scan verification remains active. Phase-20 evidence adds multiple deterministic sample rates, all-row empty-pattern reconstruction, arbitrary-byte cases, and 250 fixed-seed random texts with independently scanned queries. Payload identities and a larger rate-32 example exercise the representation claim separately from search correctness.

The test corpus is implementation evidence. The bounded LF-walk guarantee follows from sampling positions by text-position multiples plus the LF suffix-position predecessor invariant; it is not inferred from randomized testing.

## Claim audit

The phase does not claim a succinct FM-index. The BWT occurrence side still uses the full Phase-18 byte wavelet matrix, construction still uses temporary linear row-position workspace and the `O(n log^2 n)` comparison-sorted suffix array, and logical payload diagnostics intentionally exclude allocator/vector-object overhead.

Sampling is a tunable space/time tradeoff. Rate one can use more locate payload than the former full position vector; larger rates reduce sampled-position state but increase LF work. No universal best rate is claimed.

## Promotion decision

Further wrappers around count/locate would be low-value breadth. A substantial remaining representation gap is the occurrence side: the resident BWT bytes are still indexed through a full eight-level wavelet matrix even when the BWT has few runs.

Phase 21 therefore promotes a run-length BWT occurrence hypothesis. It should provide exact byte `access`/prefix-`rank` over BWT runs, integrate that representation into the existing BWT index without changing exact query semantics, expose run-count/logical-storage diagnostics, verify the rank/access structure independently against naïve sequences, and retain direct-scan BWT query verification. Query complexity must honestly include run-search costs, and repetitive-input compression evidence must not be generalized to arbitrary data.

## Seal result

Phase 20 is complete and should not receive further sampled-locate variants merely to increase algorithm count. Phase 21 is the active frontier, subject to exact implementation, independent rank/access and end-to-end query verification, explicit storage accounting, and full merged-main CI.
