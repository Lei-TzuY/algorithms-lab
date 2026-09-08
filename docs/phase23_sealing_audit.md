# Phase 23 sealing audit

## Integrated checkpoint

Phase 23 is sealed only after the run-boundary-sampled full-locate implementation reached `main@03181d5d949c3f8b0a7fa9a085c1fc8c7c40f6a0` and merged-main CI run `34177039499` completed successfully under GCC release, Clang release, and GCC ASan+UBSan.

At audit time there were no open pull requests or issues. The authoritative ROADMAP contained no Phase-24 implementation, so the audit did not displace an in-flight implementation surface.

## Correctness and integration audit

The sealed query path `BwtByteIndex::locate_run_sampled(pattern)`:

- reuses the exact backward-search interval from the earlier BWT index;
- resolves every matched conceptual BWT row through LF until the conceptual sentinel or a Phase-22 full-BWT run-boundary suffix-position sample is reached;
- reconstructs the original suffix position from the LF/SA modular recurrence;
- fails closed on impossible sample ordinals, invalid stored positions, unresolvable walks, or reconstructed positions outside `0..n`;
- never reads the Phase-20 periodic sampled-row membership/position arrays and therefore represents a genuinely different locate space/time point;
- adds no resident suffix-position array beyond the sealed `O(R)` run-boundary sample state.

The direct-scan differential suite covers deterministic arbitrary-byte/sentinel/repetitive cases and 350 fixed-seed random texts with two deliberately different Phase-20 sample rates. The two Phase-23 indexes must return the same complete sorted occurrence set despite their different periodic-sample populations. Selected queries are also cross-checked against the sealed periodic-sample locate path.

No correctness, integration, arbitrary-byte, conceptual-sentinel, oracle-independence, or representability blocker was found.

## Complexity and claim audit

The implementation intentionally permits each matched row to walk `O(n)` LF steps. With resident run count `R`, pattern length `m`, and `occ` results, the sealed conservative bound remains

`O(m log R + occ*n*log R + occ log occ)`.

Resident run-aware suffix-position sampling remains `O(R)` and query output storage is `O(occ)`. Construction still inherits temporary `O(n)` suffix-array/BWT workspace.

The phase does **not** claim:

- r-index-optimal locate time;
- universally short LF walks;
- succinct or compressed construction;
- that run-length state is always smaller than an uncompressed representation.

Those boundaries are explicit rather than inferred from the successful randomized corpus.

## Why Phase 23 stops here

Additional wrappers around the same independent-per-row LF walk would not add a new algorithmic model. The meaningful remaining gap inside this representation is the repeated query-time work: separate match rows can each rediscover suffix positions that the same query has already resolved.

## Phase 24 promotion

Phase 24 is **query-local memoized run-sampled BWT locating**.

The promoted slice must:

1. keep the sealed Phase-22 `O(R)` resident run-boundary suffix-position samples unchanged;
2. allocate only query-local `O(n)` row-position memoization;
3. seed the memoization with the conceptual sentinel and run-boundary samples;
4. resolve matched rows by LF walking until a memoized row, then fill the traversed path using the same modular LF/SA recurrence;
5. expose enough query diagnostics to verify that newly traversed LF rows are bounded by `n+1` per query;
6. return the complete exact sorted occurrence set and compare it with independent direct substring scanning;
7. preserve arbitrary-byte and conceptual-sentinel semantics;
8. make no r-index or compressed-construction claim.

The target conservative query bound is `O(m log R + n log R + occ log occ)` with `O(n + occ)` query workspace and unchanged `O(R)` resident run-aware sampling state.
