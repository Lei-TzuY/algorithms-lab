# Scope recovery: Elias-Fano monotone integer indexing

## Coverage decision

A fresh live code / pull-request / branch audit at `main@ca69621bebc036de89545fc0b4a01267ad4ae2e1` found no Elias-Fano, monotone succinct-sequence, or equivalent predecessor/select representation. The prospectively frozen Phase-45--69 compiler/backend excursion remains out of scope. Existing fractional-cascading, minimum-cycle-basis, general-graph-isomorphism, van-Emde-Boas, and push-relabel recovery surfaces are occupied and are deliberately left untouched.

This slice changes proof model away from the immediately preceding Smith-normal-form / modular-linear-system work. It reuses the sealed Phase-17 packed rank/select bitvector as a lower-level succinct primitive instead of duplicating that machinery.

## Production contract

`EliasFanoMonotoneSequence` stores a nondecreasing sequence of full-width `uint64_t` values; duplicates are valid.

- construction rejects decreasing input;
- `at(i)` reconstructs the exact original value;
- `lower_bound_index(x)` returns the first index whose value is at least `x`;
- `predecessor_index(x)` returns the last index whose value is at most `x`, or `nullopt`;
- `contains(x)` is exact;
- full `uint64_t` values, including `UINT64_MAX`, are supported;
- diagnostics expose low-bit width, unary-high length, Elias-Fano logical code bits, actual packed/indexed payload bytes, and an expensive structural replay check.

The structure is immutable after construction.

## Elias-Fano invariant

For non-empty input of length `n` and maximum value `M`, production chooses

`l = floor(log2(floor(M/n)))`

when `M/n >= 1`, otherwise `l = 0`. Each value is split into high and low parts

`x_i = (high_i << l) | low_i`.

The `l` low bits are packed consecutively into `uint64_t` words. The monotone high parts are encoded by placing a one at bit position

`high_i + i`.

Because `high_i` is nondecreasing, these positions are strictly increasing even when input values are duplicated. The sealed packed rank/select structure stores this unary-high bitvector. Therefore

`high_i = select1(i) - i`,

which reconstructs every original value exactly together with its packed low bits.

With the chosen `l`, the high universe is less than `2n` for every non-empty sequence, so the unary-high bitvector has fewer than `3n` bits. The reported `encoded_bit_count()` is the logical Elias-Fano high-plus-low code length. `logical_payload_bytes()` is intentionally different: it includes the sealed packed rank/select implementation's rank-prefix index plus packed low words. No claim is made that this educational indexed representation achieves the information-theoretic or best-known succinct-space constant.

## Query complexity boundary

The sealed `PackedRankSelectBitVector::select1` locates a containing machine word with binary search over its word-prefix index, so this implementation conservatively claims:

- construction: `O(n)` logical encoding work plus zero-initialization proportional to the `O(n)` high-bitvector length;
- `at(i)`: `O(log n)` under the current sealed select implementation;
- `lower_bound_index`, `predecessor_index`, and `contains`: `O(log^2 n)` via binary search over decoded values;
- resident Elias-Fano logical code: `n*l + O(n)` bits, while physical indexed payload is exposed separately;
- `valid_structure()`: `O(n log n)` diagnostic work and excluded from normal query bounds.

This deliberately does **not** claim an optimal constant-time select or predecessor bound from a different Elias-Fano/rank-select implementation.

## Verification

Focused candidate verification passed under repository-equivalent strict GCC, strict Clang, and actual GCC ASan+UBSan execution.

Committed evidence covers:

- empty input and decreasing-input rejection;
- singleton zero and singleton `UINT64_MAX`;
- duplicates, lower-bound boundaries, predecessor-last-duplicate semantics, and membership;
- mixed full-width values including `2^63` and `UINT64_MAX`;
- 700 fixed-seed monotone sequences of length `0..256`, mixing duplicate-heavy small universes with full-width random values;
- exact replay of every stored value;
- 80 independent query values per randomized sequence compared with `std::lower_bound` / `std::upper_bound` on the original sorted vector;
- 300 additional full-width random sequences checking the `< 3n` unary-high bound, exact logical bit accounting, and physical-payload lower bound;
- `valid_structure()` replay on every randomized instance.

The primary oracle is the original sorted vector plus standard-library search used only in tests. It does not reuse Elias-Fano high/low splitting or rank/select decoding.

## Non-claims

This is not a dynamic sequence, compressed posting-list codec, gap coder, Elias gamma/delta codec, sampled predecessor accelerator, or best-known succinct predecessor structure. It does not claim universally fewer physical bytes than a raw `uint64_t` vector because the reused rank/select index intentionally carries auxiliary prefix metadata.
