# Phase 20 — sampled BWT locate space/time tradeoff

## Scope

Phase 20 keeps the exact Phase-19 BWT backward-search API but changes the resident locate representation. The full augmented `row -> text position` table is removed after construction. Exact positions are reconstructed from periodic suffix-position samples through bounded LF walks.

This is a representation-depth change, not another pattern-query wrapper.

## Sampling contract

Construction accepts a positive locate sample rate `r` (default `32`). For every augmented suffix row with text position `p`:

- ordinary text positions are sampled when `p % r == 0`;
- the sentinel-only suffix position `n` is always sampled, independent of `n % r`.

Row membership is stored in the sealed Phase-17 `PackedRankSelectBitVector`. Sampled suffix positions are stored in one dense vector in the same order as one bits in the membership vector. Therefore for sampled row `row`, `rank1(row)` is exactly its zero-based sample ordinal.

`r == 0` is rejected. `r == 1` samples every row and performs zero LF steps, but may use more locate payload than the old full position vector because packed membership metadata is still present. Larger `r` stores fewer `size_t` sample positions and permits more LF work.

## LF reconstruction invariant

For a non-sentinel BWT byte `c` in augmented row `i`,

`LF(i) = C[c] + Occ(c, i)`.

The unique row whose BWT predecessor is the conceptual sentinel maps to row zero, the suffix starting at `n`.

If an augmented row represents suffix position `s > 0`, one LF step maps it to suffix position `s - 1`. Position zero is always sampled because `0 % r == 0`; position `n` is explicitly sampled. Consequently resolution never needs to wrap past zero: every non-sampled position reaches the greatest sampled multiple of `r` not exceeding it in strictly fewer than `r` LF steps.

Production code checks this bound on every locate reconstruction. Once a sampled row is reached, if its stored suffix position is `p` and the walk used `d` LF steps, the original position is exactly `p + d`. Representability and range invariants are checked before returning it.

## Resident locate storage

The Phase-19 member `std::vector<std::size_t> row_positions_` is removed. Construction still uses a temporary augmented row-position vector because the existing suffix-array API returns suffix starts; this is build-time workspace, not resident locate state.

The resident locate payload is reported through diagnostics:

- `sampled_membership_payload_bytes()` — packed membership words plus rank-prefix entries;
- `sampled_position_payload_bytes()` — sampled `size_t` values only;
- `sampled_locate_payload_bytes()` — the checked sum of those two logical payloads;
- `sampled_row_count()` and `locate_sample_rate()` make the tradeoff inspectable.

These accounting methods intentionally exclude vector objects, allocator metadata, and spare capacity. No general succinct-FM-index claim is made: the BWT byte occurrence structure remains the Phase-18 wavelet matrix, and the packed membership rank index itself has explicit metadata.

## Query and construction complexity

For text length `n`, pattern length `m`, `k` matches, and sample rate `r`:

- suffix/BWT construction keeps the Phase-19 `O(n log^2 n)` suffix-array bound plus linear fixed-byte-alphabet index construction;
- `count(pattern)` remains `O(m)`;
- each located row performs fewer than `r` LF steps, each using a fixed eight-level byte wavelet access/rank path;
- `locate(pattern)` is `O(m + k*r + k log k)` under the fixed-byte-alphabet model, including sorting positions into text order;
- sampled suffix-position storage is `O(n/r)` `size_t` values plus one packed membership/rank structure over `n + 1` rows.

The phase makes the space/time parameter explicit rather than claiming one universally better setting.

## Verification

All Phase-19 deterministic and randomized direct-scan checks remain active under the default sampling rate.

Phase-20-specific evidence additionally covers:

- zero sample rate rejection;
- rates `1, 2, 3, 4, 7, 32, 100` on overlapping classic queries;
- empty-pattern locate at each rate, which forces reconstruction of every augmented suffix row and therefore exercises the LF-step bound globally for that index;
- arbitrary bytes including NUL/high-bit values under multiple rates;
- 250 fixed-seed random byte texts with a random sample rate in `1..64`, 60 direct-scan-checked queries per text;
- explicit sample-count/payload identities;
- a 2048-byte deterministic text at rate 32 where sampled locate logical payload is checked to be smaller than the former `(n + 1) * sizeof(size_t)` full-row table.

Focused pre-upload verification passed under repository strict warnings with GCC and Clang, and under a real GCC ASan+UBSan build. These tests provide implementation evidence; the `< r` LF bound follows from the sampling/LF invariant above rather than from sampling alone.

## Phase boundary

Phase 20 is sealed after PR #57 reached `main@bebf1543db4c56cce7b9f155bd91beaed7b8f8a0` and merged-main CI run `34172494194` completed successfully under GCC release, Clang release, and GCC ASan+UBSan.

The architecture audit rejects more wrappers around the same backward-search interval. The next substantial representation frontier is the other resident side of the index: Phase 21 will investigate a run-length byte rank/access representation for BWT occurrence queries. It must preserve exact count/locate semantics while exposing run count, logical payload, and query-cost evidence, and it must explicitly avoid any claim that run-length storage is universally smaller on non-repetitive inputs.
