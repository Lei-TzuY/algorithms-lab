# Scope recovery: bounded RRR rank/select bitvector

## Coverage decision

After the bounded exact characteristic-polynomial slice merged as
`main@a6bc1adbfb483cb20b59bc65ad16babfb1a83173`, a fresh live-state audit
found zero open pull requests and zero open issues.

The repository already contains a static byte wavelet matrix and a balanced-
parentheses tree, both of which consume bit-oriented indexing ideas, but it had
no standalone RRR / class-offset rank-select bitvector implementation, branch,
or prior implementation pull request.

This slice therefore adds a new data-structural proof obligation rather than a
wrapper around the existing wavelet matrix: exact combinatorial encoding of
fixed-size bit blocks plus exact access/rank/select semantics.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; the frozen compiler/backend frontier
remains untouched.

## Public contract

`RrrBitVector15` is immutable after construction.

Input is a byte sequence in which every element must be exactly `0` or `1`.
Any other value is rejected with `std::invalid_argument`.

The public operations are:

- `size()`, `empty()`, `ones()`, and `zeros()`;
- `access(i)` for the exact bit at position `i`;
- `rank1(end)` for the number of one bits in `[0,end)`;
- `rank0(end)` for the number of zero bits in `[0,end)`;
- `select1(k)` for the position of the zero-based `k`th one bit;
- `select0(k)` for the position of the zero-based `k`th zero bit;
- `valid_structure()` as an expensive representation replay.

Out-of-range access/rank endpoints are rejected. A select ordinal beyond the
available population returns `nullopt`.

The empty bitvector is valid and has zero encoded blocks and zero logical
superblocks.

## Bounded RRR representation

The block width is fixed at 15 bits.

Each block is represented by:

1. its actual length (the final block may be shorter than 15);
2. its **class**, the number of one bits;
3. its **offset**, the combinatorial rank among bit patterns with the same
   length and class.

The maximum class population is therefore bounded by
`C(15,7)=6435`, so every offset fits in 16 bits.

Sixteen encoded blocks form one superblock. Production stores exact cumulative
one and zero counts at superblock boundaries.

These prefix counters are an implementation choice for bounded query work; this
slice does not claim that the complete C++ object layout is entropy-optimal.

## Class/offset bijection

For a block with one-bit positions

`p_1 < p_2 < ... < p_k`

using one-based position rank inside the set of chosen positions, production
encodes the offset as the combinatorial-number-system value

`sum_{i=1..k} C(p_i, i)`

where physical bit positions are zero-based.

This is the standard colexicographic combinadic ranking of a `k`-subset of a
block of length `L`, yielding an integer in

`[0, C(L,k))`.

Decoding works in reverse. Starting from the highest selected position, it picks
the largest position whose binomial coefficient does not exceed the remaining
offset, subtracts that coefficient, and continues below that position.

The representation replay verifies:

- class does not exceed actual block length;
- offset is below `C(length,class)`;
- decoding produces exactly the recorded class population;
- no decoded bit lies beyond the block length;
- re-ranking the decoded block reproduces the stored offset.

## Rank

For `rank1(end)`:

1. locate the block containing `end`;
2. load the exact one-prefix at that block's superblock boundary;
3. add the classes of complete blocks between the superblock boundary and the
   query block;
4. if `end` lies inside a block, decode only that block and count the prefix
   bits.

At most fifteen complete blocks are scanned after the prefix lookup.

`rank0(end)` is exactly `end-rank1(end)`.

Exact block-boundary endpoints and the final partial block are handled
separately; the full-size aligned endpoint can return the stored total directly.

## Select

For one-select, production binary-searches the monotone superblock one-prefix
array to find the superblock containing the requested ordinal.

It then scans at most sixteen encoded blocks. Once the containing block is
known, that block is decoded and scanned for the requested local ordinal.

Zero-select is identical over the exact zero-prefix array.

Repeated prefix values are valid: an all-zero superblock contributes no one
population and an all-one superblock contributes no zero population. Using
`upper_bound` deliberately skips empty-population superblocks while preserving
the target ordinal.

## Complexity boundary

Let:

- `B=15` be the fixed block width;
- `S=16` be the fixed number of blocks per superblock;
- `N` be the logical bit count.

The straightforward combinadic decoder used here is bounded by a small
`O(B^2)` loop.

Therefore:

- access performs one bounded block decode;
- rank performs one superblock lookup, at most `S-1` class additions, and at
  most one bounded decode;
- select performs `O(log(N/(BS)+1))` superblock search, at most `S` block
  inspections, and one bounded decode;
- construction is linear in `N` for fixed `B`;
- logical encoded-block count is `ceil(N/B)`;
- superblock prefix count is `ceil(blocks/S)+1` for nonempty input and one
  sentinel entry for empty input.

Because `B` and `S` are compile-time constants, the block-local work is
strictly bounded in this implementation.

This slice intentionally does **not** claim:

- an entropy-optimal total C++ object footprint;
- the asymptotically optimal bit count of a packed theoretical RRR
  implementation;
- constant-time select independent of the superblock search;
- SIMD acceleration;
- benchmark-backed latency or compression ratio.

The class/offset encoding is real; stronger succinctness claims require a packed
representation and corresponding measurements.

## Independent verification

The committed oracle stores the original bits directly and uses plain linear
scans.

For every tested bitvector it independently verifies:

- every `access(i)`;
- every rank endpoint from zero through the full length;
- every valid one-select ordinal;
- every valid zero-select ordinal;
- the first invalid select ordinal for each bit value;
- public population counts;
- structural replay.

Committed coverage includes:

- empty input;
- invalid input bytes;
- a fixed mixed pattern;
- all-zero and all-one vectors;
- block boundaries around 15 bits;
- the 16-block / 240-bit superblock boundary;
- exhaustive enumeration of every bitstring of length 0..10;
- 350 fixed-seed random vectors of length 0..400.

A supplementary model-level exhaustive check enumerated every bit pattern of
every length 1..15, for 65,534 total masks, and verified
`decode(rank(mask)) == mask` with the same combinadic equations. No mismatch
was found.

That model check is supporting evidence only; exact repository GCC release,
Clang release, and GCC ASan+UBSan CI on the final pull-request head remain the
integration gate.

## Pre-PR hardening

The initial implementation review found and fixed an empty-state bug: the empty
constructor had emitted both an initial and final superblock sentinel, making
the logical superblock count inconsistent with the representation replay.

The fix leaves exactly one zero prefix sentinel for empty input.

The default constructor was also changed to pass an explicit empty
`std::span<const Bit>`, avoiding empty-brace conversion ambiguity.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/data_structures/rrr_bitvector.hpp`;
- `tests/test_rrr_bitvector_cases.hpp`;
- `docs/scope_recovery_rrr_bitvector.md`.

No wavelet-matrix change, balanced-parentheses change, CMake, test-main, README,
historical ROADMAP, workflow, benchmark, frozen compiler/backend, or temporary
file is required.

Base: `a6bc1adbfb483cb20b59bc65ad16babfb1a83173`.
