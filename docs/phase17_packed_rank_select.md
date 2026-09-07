# Phase 17 — packed static rank/select indexing

## Scope

`PackedRankSelectBitVector` is an immutable index over an input sequence whose
values must be exactly zero or one. Bits are packed LSB-first into 64-bit words.
A prefix-one count is stored at every word boundary.

This phase deliberately does **not** claim a theoretical succinct `n+o(n)`
representation. The implementation exposes an exact logical-payload accounting
for the arrays it actually stores and documents their direct asymptotic bounds.

## Rank invariant

For every word boundary `w`, `prefix_ones[w]` is exactly the number of one bits
in positions `[0, 64*w)`, clipped by the logical bit-vector length. Therefore
`rank1(end)` for the half-open prefix `[0,end)` is the prefix entry for all full
words plus a popcount of the masked remainder. `rank0(end) = end-rank1(end)`.

Rank queries validate `0 <= end <= n` and are `O(1)` under the machine-word
popcount model.

## Select

`select1(j)` and `select0(j)` use zero-based ordinals and return `nullopt` when
that occurrence does not exist. They binary-search word-boundary prefix counts to
locate the containing 64-bit word, then clear lower matching bits until the
requested within-word ordinal is reached.

The direct bound is `O(log ceil(n/64) + 64)` time, which is `O(log n)`
asymptotically. No constant-time-select claim is made.

For zero selection, trailing padding bits in the final packed word are masked out
before the within-word selection step.

## Storage accounting

For `W = ceil(n/64)`, the logical arrays are:

- `W` packed `uint64_t` words;
- `W+1` `size_t` prefix-one entries.

`logical_payload_bytes()` reports exactly
`W*sizeof(uint64_t) + (W+1)*sizeof(size_t)`.
It intentionally excludes vector object headers, allocator metadata, and any
implementation-dependent spare capacity.

## Verification

- empty and singleton vectors;
- invalid bit values and rank/bit bounds;
- adversarial lengths 63/64/65/127/128/129;
- all-zero, all-one, alternating, sparse, and patterned inputs;
- 800 fixed-seed random vectors of length 0..2048;
- every random vector checks every rank endpoint and every valid one/zero select
  against independent linear scans;
- logical payload accounting and total one/zero counts are checked exactly;
- strict GCC, strict Clang, and ASan+UBSan focused builds pass.

## Frontier

Phase 17 implementation is complete; phase-level integration and sealing audit is
the next gate before any further promotion.
