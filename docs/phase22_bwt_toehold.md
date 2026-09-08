# Phase 22 — run-aware BWT toehold locating

## Scope

Phase 22 adds a one-occurrence locating path above the sealed Phase-21 run-length BWT occurrence representation. It does **not** replace the Phase-20 periodic-sample `locate()` path and does not claim a full r-index.

`BwtByteIndex::locate_one_toehold(pattern)` returns one exact text position for a present pattern, `nullopt` for a non-empty absent pattern, and boundary zero for the empty pattern. The method maintains one exact suffix-array witness inside every non-empty backward-search interval.

## Toehold invariant

During backward search, maintain `(p, SA[p])` such that row `p` lies inside the current interval.

For the next left-extension byte `c`:

1. Compute the exact next FM interval with the same cumulative-count/rank recurrence used by ordinary backward search.
2. If full-BWT row `p` carries byte `c`, LF-map `p`. Since a byte row cannot be the conceptual-sentinel row, `SA[p] > 0`; the new witness is `(LF(p), SA[p]-1)`.
3. Otherwise, if the next interval is non-empty, choose the first `c` row after `p` inside the old interval when one exists; otherwise choose the last `c` row before `p` inside that interval.
4. Because `p` itself is not a `c` row and the selected row is the nearest `c` on that side, the selected row is a **full conceptual-BWT byte-run boundary**.
5. Phase 22 stores `SA` at every such byte-run start/end. Restore the selected boundary's suffix position from that sample and LF-map it to obtain the next interval's exact toehold.

Every production fallback checks that the selected row lies inside the old interval, has a boundary sample, has positive suffix position, and maps inside the newly computed interval. Missing samples or violated interval relations are internal logic errors rather than silent fallback to a full suffix-row table.

## Conceptual sentinel and run boundaries

The resident Phase-21 `RunLengthByteRankIndex` omits the conceptual sentinel. Deleting that one sentinel can make equal byte runs on its two sides appear adjacent in the compressed byte sequence.

Phase 22 therefore decides samples on the **full conceptual BWT**, treating the sentinel as an explicit run break. A deterministic regression uses text bytes `{0,1,1}`, whose full BWT is `{1, sentinel, 1, 0}` while its sentinel-deleted byte sequence begins `{1,1,...}`. The two `1` rows remain independently sampled boundaries.

## Run-length select support

`RunLengthByteRankIndex` now exposes zero-based `select(byte, ordinal)`. The symbol-specific run list is searched by cumulative occurrence counts to find the containing run, then the ordinal offset is converted back to a compressed BWT position.

This operation reuses the Phase-21 run metadata and adds no resident run-index arrays. Invalid ordinals fail with `std::out_of_range`.

## Storage and complexity

Let `R` be the number of runs in the sentinel-deleted resident BWT and let `S` be the number of sampled full-BWT byte-run boundary rows.

Inserting the single conceptual sentinel can split at most one compressed byte run, so the full conceptual BWT has at most `R+1` byte runs. Sampling each byte run's start/end at most once gives

`S <= 2(R+1)`.

Phase 22 stores two `size_t` vectors of length `S`: sampled row and exact suffix position. The exposed logical toehold payload is therefore

`S * 2 * sizeof(size_t)` bytes,

excluding vector objects, allocator metadata, and spare capacity.

For byte `c`, `select` is `O(log R_c)`. Boundary-sample lookup is `O(log S)`, and Phase-21 rank/access remain `O(log R)` conservatively. Each pattern character performs one exact FM extension and at most one run select/sample lookup, so `locate_one_toehold` is conservatively `O(m log R)` for pattern length `m`.

Suffix-array construction and temporary `O(n)` row-position workspace remain unchanged. Existing full `locate()` still uses Phase-20 periodic samples and keeps its previously documented storage/query tradeoff.

## Verification

Verification is deliberately split from the theorem:

- Phase-21 rank tests are extended so every occurrence in 1000 fixed-seed random byte sequences is replayed through `select`.
- deterministic BWT tests cover classic overlaps, empty/absent patterns, arbitrary bytes, multiple periodic sample rates, and the conceptual-sentinel run-split adversary;
- 500 fixed-seed random texts of length 0..80 execute 80 patterns each against an independent direct substring scan;
- a returned toehold position must belong to the direct-scan occurrence set, and absence must match direct scan exactly;
- run-toehold sample count and logical payload identities are checked independently from query correctness;
- existing Phase-19/20 exact `count`/`locate` suites remain active and unchanged except for the new shared FM-extension helper.

A separate pre-upload theorem prototype additionally exercised 2000 random texts with 100 direct-scan queries each under GCC strict warnings, Clang strict warnings, and real GCC ASan+UBSan.

The randomized corpus is implementation evidence. The `O(R)` sample bound and one-toehold-per-extension guarantee follow from the full-BWT nearest-occurrence/run-boundary argument above; they are not inferred from the test corpus.

## Claim boundary

This phase is not a full r-index. It does not implement locate-all from `O(R)` samples, suffix-array run samples sufficient for arbitrary occurrence enumeration, predecessor structures with published r-index bounds, or a compressed construction algorithm. The existing exact full locate path still keeps Phase-20 periodic samples.

Phase 22 is implementation-complete only after exact PR CI and merged-main CI pass. A later architecture audit must decide whether full run-aware locate-all would add a sufficiently different correctness/storage model to justify another phase; wrappers around `locate_one_toehold` do not.
