# Phase 17 sealing audit

## Decision

Phase 17 is sealed after the packed static rank/select implementation reached
merged `main` and the exact merged-main CI matrix completed successfully on GCC
release, Clang release, and GCC ASan+UBSan.

The phase has one intentionally bounded capability: an immutable binary sequence
index with packed 64-bit storage, exact half-open rank, deterministic select, and
explicit logical-payload accounting. Adding more wrappers, alternate bit orders,
or near-identical select variants would increase surface area without adding a
new algorithmic model.

## Correctness and evidence

The implementation keeps a prefix-one count at every packed-word boundary.
`rank1` combines that boundary count with one masked popcount; `rank0` is derived
from the exact logical prefix length. `select1` and `select0` locate a packed word
from cumulative counts and then select inside at most 64 valid bits. Final-word
padding is excluded from zero selection.

Evidence includes:

- deterministic empty, singleton, invalid-input, and bounds regressions;
- adversarial word-boundary lengths around 64-bit transitions;
- all-zero, all-one, alternating, sparse, and patterned vectors;
- 800 fixed-seed random vectors up to length 2048;
- every rank endpoint and every valid zero/one select checked against independent
  linear scans;
- exact logical-payload accounting checks;
- strict GCC, strict Clang, and ASan+UBSan focused builds;
- exact pull-request CI and merged-main CI on GCC release, Clang release, and GCC
  ASan+UBSan.

## Honesty boundary

The phase does **not** claim theoretical `n+o(n)` succinct space. Its prefix
index has one `size_t` entry per packed word boundary in addition to the packed
words. It also does **not** claim constant-time select: the implementation uses a
binary search over packed words plus bounded within-word work, so the direct
asymptotic claim remains `O(log n)` select.

`logical_payload_bytes()` reports the logical bytes of the two backing arrays; it
is not a measurement of allocator metadata or total process heap consumption.

## Architecture audit

No correctness or integration blocker remains. Phase 17 should not continue by
farming bit-vector variants. The highest-value promotion is to compose the sealed
binary index into a larger static sequence structure.

Phase 18 therefore targets a byte wavelet matrix. Eight stable-partition levels
reuse `PackedRankSelectBitVector` directly and add a qualitatively new query
model: byte access, prefix/range rank, global select, and range order statistics.
This is a cross-phase architectural promotion, not a count-driven breadth
expansion.
