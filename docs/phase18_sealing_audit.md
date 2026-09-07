# Phase 18 sealing audit

## Decision

Phase 18 is sealed after the exact merged-main CI matrix for the byte wavelet
matrix completed successfully. The implementation adds one coherent structural
layer: eight sealed Phase-17 binary rank/select indexes are composed through
stable partitions into an immutable byte-sequence index.

Adding wavelet-tree naming variants, alternate bit traversal orders, or thin query
wrappers would not add a new algorithmic model and should not extend this phase.

## Correctness and integration

At each level the stored bitvector is the next most-significant bit of the current
stable ordering and the recorded zero count is the partition boundary. That
single invariant drives access, rank, reverse select, and range quantile queries.

The implementation reuses `PackedRankSelectBitVector` directly rather than
replicating binary prefix-count logic. Binary-input tests compare every rank
endpoint and every valid zero/one select with the sealed Phase-17 structure, while
byte-valued randomized tests use independent scans and sorted range copies.

## Evidence

- deterministic mixed-byte, empty/bounds, all-256-byte, and packed-word-boundary
  cases;
- binary cross-layer equality with Phase-17 rank/select;
- 400 fixed-seed random byte sequences of length 0..768;
- 80 randomized query rounds per sequence;
- direct access oracle, linear rank/select oracles, and sorted-copy quantile oracle;
- strict GCC and Clang focused builds;
- actual ASan+UBSan focused build;
- full pull-request CI and exact merged-main CI on GCC release, Clang release,
  and GCC ASan+UBSan.

## Honesty boundary

The byte alphabet has exactly eight levels, so access/rank/range-quantile use a
constant eight rank/bit steps. Global select inherits the sealed Phase-17 select
implementation and therefore remains `O(log n)` rather than being described as
constant-time.

`logical_payload_bytes()` reports the eight underlying logical packed-index
payloads plus eight zero-count values. It does not include vector headers,
allocator metadata, spare capacity, or construction temporaries, and no
`n+o(n)` succinct-space claim is made.

## Promotion

The next substantial frontier is text indexing rather than more wavelet variants.
Phase 19 composes the sealed Phase-5 suffix array with the sealed Phase-18 byte
wavelet matrix into an exact BWT backward-search index. A conceptual sentinel
outside the byte alphabet is tracked separately so arbitrary input bytes,
including `0x00`, remain valid. Because the planned first implementation keeps
full suffix-row positions for locate, it must not be marketed as a succinct
FM-index.
