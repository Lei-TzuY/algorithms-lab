# Phase 18 — byte wavelet matrix static sequence indexing

## Scope

`ByteWaveletMatrix` is an immutable index over arbitrary byte sequences. It
promotes the sealed Phase-17 binary rank/select primitive into a fixed 8-bit
alphabet structure instead of maintaining one independent prefix table per byte.

Construction processes bits from most-significant to least-significant. At each
of the eight levels it builds a `PackedRankSelectBitVector`, records the number of
zeros, and stably partitions the current byte ordering into zero-bit values
followed by one-bit values.

## Level invariant

At level `l`, the stored packed bitvector is exactly bit `7-l` of the sequence in
the ordering produced by the previous levels. `zero_counts[l]` is the size of the
zero partition. Therefore a position or half-open interval maps to the next level
using Phase-17 `rank0`/`rank1` plus the zero-partition offset.

This invariant supports:

- `access(i)`: follow one position through all eight levels while reconstructing
  the byte bits;
- `rank(value, end)` and range rank: descend the half-open interval for the fixed
  value and return its final width;
- `select(value, ordinal)`: descend to the final value interval, then invert all
  eight stable partitions using Phase-17 `select0`/`select1`;
- `kth_smallest(begin,end,k)`: count zeros in the current range at each level and
  choose the zero or one partition according to the requested ordinal.

## Complexity and storage boundary

The alphabet width is fixed at eight levels for `uint8_t` values.

- build: `O(8n) = O(n)`;
- access: eight rank/bit steps, `O(1)` for the fixed byte alphabet under the
  Phase-17 rank model;
- prefix/range rank: eight rank steps, `O(1)` for the fixed byte alphabet;
- range `kth_smallest`: eight rank steps, `O(1)` for the fixed byte alphabet;
- select: eight Phase-17 select operations, therefore `O(log n)` under the
  current packed-index select implementation.

`logical_payload_bytes()` is deliberately narrow: it sums the logical payload of
the eight packed rank/select levels plus the eight stored `size_t` zero counts.
It excludes vector object headers, allocator metadata, spare capacity, and the
temporary construction buffers. No theoretical succinct-space claim is made.

## Verification

Deterministic cases cover empty input, query bounds, a known mixed-byte vector,
all 256 byte values, and payload accounting around packed-word boundaries.

Cross-layer integration uses binary input and verifies every rank endpoint and
every valid zero/one select against the sealed Phase-17
`PackedRankSelectBitVector`.

A fixed-seed randomized suite builds 400 byte sequences of length 0..768 and
executes 80 query rounds per sequence. `access` is checked directly; rank uses a
linear scan; select uses an independent linear occurrence scan; range quantiles
use independently sorted copies of the requested slice. The focused candidate
passes strict GCC, strict Clang, and actual ASan+UBSan builds.

## Frontier

Phase 18 implementation is bounded to the byte wavelet-matrix indexing model.
It does not claim an FM-index or compressed full-text index. A later phase may
compose this static sequence substrate with the sealed suffix/BWT-related string
capabilities, but only after Phase 18 passes full repository integration and its
own sealing audit.
