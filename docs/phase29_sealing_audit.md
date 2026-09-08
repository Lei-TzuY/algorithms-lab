# Phase 29 sealing audit

Phase 29 is sealed after exact BWT/LF text reconstruction and bounded extraction reached merged `main` as `e6ae62b5e10d84c3daff58b91608d245fa1ed025` and exact merged-main CI run `34188386490` completed successfully on GCC release, Clang release, and GCC ASan+UBSan.

## Correctness boundary

The phase closes one specific structural gap: the resident conceptual-sentinel BWT can recover the constructor bytes without storing or consulting a source-text fallback.

Construction places suffix position `n` at conceptual row zero and records suffix position `0` as `sentinel_row_`. For every non-sentinel row representing suffix position `p > 0`, the resident BWT byte is `text[p-1]`, and LF maps that row to suffix position `p-1`. Reading before each LF transition from row zero therefore reconstructs `text[n-1]..text[0]`; exactly `n` transitions must end at the sentinel row. Production rejects early or late sentinel arrival as a structural logic error.

`extract_text(begin,end)` is deliberately a correctness baseline: it validates the half-open range, performs one complete reconstruction, then slices the recovered bytes. Every successful query exposes `lf_steps == reconstructed_bytes == text_size()`.

## Verification evidence

- PR #75 exact candidate CI completed successfully on all three repository jobs;
- merged-main run `34188386490` completed successfully after squash merge;
- deterministic tests cover empty/classic text, invalid ranges, every byte value, embedded NUL/high bytes, repeated calls, and locate-sample-rate independence;
- 400 fixed-seed arbitrary-byte texts of length `0..128` are reconstructed byte-for-byte against the constructor input;
- 40 independently chosen valid slices per randomized text are compared against direct `std::string::substr`;
- source-level audit confirmed the recurrence against the live constructor and LF implementation rather than relying only on tests that exercise the new code.

## Complexity and claim audit

The implementation intentionally uses `O(n)` reconstruction workspace and performs exactly `n` LF transitions per successful extraction query before copying the requested output. Each LF step inherits the sealed run-length occurrence representation's access/rank cost. The phase does not claim constant-time LF, compressed-optimal random access, r-index extraction bounds, source-compressed construction, or a universal speedup.

Those limitations are explicit design boundaries, not unresolved correctness blockers. Farming faster extraction variants inside Phase 29 would turn a completed correctness substrate into open-ended performance churn.

## Promotion rationale

The next coherent architecture is exact seed-and-verify bounded edit-distance search. For edit budget `k < m`, partitioning a pattern of length `m` into `k+1` non-overlapping non-empty exact seeds gives a completeness handle: any alignment within `k` Levenshtein edits leaves at least one seed untouched, while preceding insertions/deletions can shift that seed's text occurrence from the candidate start by at most `k` positions.

Phase 30 will therefore use sealed exact BWT seed locate to generate candidate starts, reconstruct the source text once through Phase 29, and verify each unique candidate with exact dynamic programming. It must compare against an independent direct-DP substring oracle and cross-check bounded cases with sealed Phase 27. This is an architectural shift from exponential state expansion to exact candidate filtering plus polynomial verification, not another edit-cost variant.
