# Phase 21 — run-length BWT occurrence representation

## Scope

Phase 21 changes the resident BWT occurrence representation while preserving the exact Phase-19 backward-search semantics and the Phase-20 sampled-locate representation. The eight-level `ByteWaveletMatrix` is replaced inside `BwtByteIndex` by an immutable `RunLengthByteRankIndex` that supports only the operations the BWT index actually needs: byte `access` and prefix/range `rank`.

This is a representation-depth change, not a new search wrapper and not a claim that run-length encoding is universally smaller.

## Run representation and invariant

For a byte sequence of length `n` with `R` maximal equal-byte runs, the index stores:

- `run_starts[r]` — start position of run `r`;
- `run_values[r]` — byte carried by run `r`;
- `run_cumulative_after[r]` — total occurrences of that run's byte through the end of run `r`;
- for each of the 256 byte values, an ordered list of the global run indices carrying that byte.

Run starts are strictly increasing, adjacent run values differ, every input position belongs to exactly one run, and each global run index appears exactly once in the symbol-specific lists.

`access(i)` finds the greatest run start not exceeding `i`. `rank(c, end)` binary-searches only `c`'s run-index list, takes the cumulative total before the last run that starts before `end`, and adds the prefix length consumed inside that run. Therefore both operations are exact without expanding the BWT bytes.

## Complexity

Let `R` be the total number of BWT byte runs and `R_c` the number of runs carrying byte `c`.

- construction: `O(n)` time;
- `access`: `O(log R)`;
- prefix `rank(c, end)`: `O(log R_c)` (bounded by `O(log R)`);
- range rank: two prefix ranks;
- logical run payload: `R * (3*sizeof(size_t) + sizeof(uint8_t))` bytes.

The logical payload deliberately excludes the 256 `std::vector` objects, allocator metadata, and spare capacity. It is a representation diagnostic, not a process-RSS claim.

When integrated into `BwtByteIndex`, a pattern of length `m` performs `O(m log R)` occurrence work under the conservative bound. Each Phase-20 LF step now uses one run access and one rank, so locating `k` matches at sample rate `r` has conservative bound `O(m log R + k*r*log R + k log k)`, including output sorting. The existing suffix-array construction bound is unchanged.

## BWT integration

`BwtByteIndex` keeps:

- conceptual-sentinel cumulative counts;
- exact backward-search intervals;
- Phase-20 packed sampled-row membership and sampled suffix positions;
- the production-checked `< sample_rate` LF reconstruction bound.

Only the resident BWT byte occurrence backend changes. Diagnostics expose `bwt_run_count()` and `bwt_occurrence_payload_bytes()` so the representation is inspectable.

## Verification

Correctness is checked at two independent layers:

1. `RunLengthByteRankIndex` is tested directly against naïve byte sequences: deterministic edge cases plus 1000 fixed-seed random sequences of length 0..300, every access position, and 200 random prefix/range-rank queries per sequence.
2. The existing Phase-19/20 BWT direct-scan suites remain unchanged and therefore exercise the new backend through exact `count`, `locate`, arbitrary-byte, empty-pattern, sampling-rate, and LF-walk paths. A Phase-21 integration regression additionally checks a 2048-byte repetitive text whose BWT is a single byte run.

The repetitive case verifies that the reported run payload is smaller than a `ByteWaveletMatrix` built over the same all-equal BWT byte sequence. This is representative compression evidence only. On non-repetitive data `R` can approach `n` and this implementation can use more memory than the wavelet representation; no universal compression claim is made.

Focused pre-upload verification passed under GCC and Clang strict warnings and under a real GCC ASan+UBSan build for both the standalone run-rank index and a BWT integration harness. Exact PR CI and merged-main CI subsequently passed in all three repository jobs.

## Phase boundary

Phase 21 is sealed at `main@5576e045068c739e0e4b8c0cca2ec307cf7e15b1` after merged-main CI run `34174427111` completed successfully under GCC release, Clang release, and GCC ASan+UBSan.

The occurrence-representation hypothesis is complete: exact BWT semantics are preserved, run-dependent complexity/storage claims are explicit, independent rank/access verification remains active, and repetitive-input evidence is not generalized into a universal compression claim. Additional RLE wrappers or container variants would not add a new correctness/storage model.

The promoted frontier is Phase 22 run-aware BWT toehold locating. That phase must introduce a new run-boundary suffix-position sampling invariant and return a directly verifiable exact occurrence from a non-empty backward-search interval without restoring the full suffix-row position table. It must treat the conceptual sentinel explicitly, expose run-aware sample storage, and keep direct-scan query verification independent from the sampling theorem.
