# Phase 19 — exact BWT backward-search text index

## Scope

Phase 19 adds an immutable byte-string substring index built around exact Burrows-Wheeler backward search. The index composes two already-sealed capabilities rather than reimplementing them:

- Phase 5 supplies lexicographic suffix order through `build_suffix_array`.
- Phase 18 supplies byte occurrence counts through `ByteWaveletMatrix::rank`.

All 256 byte values remain valid input symbols. The unique end-of-text sentinel is conceptual and is never encoded as a byte value.

## Augmented suffix rows and conceptual sentinel

For text length `n`, the augmented suffix order contains `n + 1` rows:

1. row for the sentinel-only suffix starting at `n`;
2. the `n` non-empty suffix starts returned by the sealed Phase-5 suffix array.

The existing suffix-array implementation already ranks an implicit end-of-string sentinel below every real byte, so `[n] + suffix_order` is the augmented lexicographic row order.

For a row whose suffix starts at `s`:

- if `s == 0`, the BWT predecessor is the conceptual sentinel;
- otherwise the predecessor is `text[s - 1]`.

Exactly one BWT row therefore contains the sentinel. That row index is retained separately. The remaining `n` real BWT bytes are stored in a Phase-18 `ByteWaveletMatrix`.

## Backward-search invariant

Let `[l, r)` be the current augmented suffix-row interval matching the suffix of the pattern processed so far. Before processing any pattern byte, `[l, r) = [0, n + 1)`.

For byte `c`, define

`C[c] = 1 + number of text bytes strictly smaller than c`.

The leading `1` accounts for the conceptual sentinel in the first column. Let `Occ(c, x)` be the number of byte `c` values in augmented BWT rows `[0, x)`. Because the wavelet matrix omits the sentinel row, `x` is mapped to the compressed BWT prefix by subtracting one exactly when that prefix crosses the sentinel row.

The LF/backward-search update is

`l' = C[c] + Occ(c, l)`

`r' = C[c] + Occ(c, r)`.

This preserves the invariant that `[l', r')` is exactly the set of suffix rows prefixed by `c` followed by the already-processed pattern suffix.

## Exact count and locate

`count(pattern)` returns `r - l` after the final backward-search step.

`locate(pattern)` retains the full augmented row-to-text-position vector, gathers positions from `[l, r)`, and sorts them into ascending text order. Consequently locate is exact but intentionally not succinct.

The empty pattern matches every boundary position `0..n`, consistent with the repository's KMP empty-pattern convention.

## Complexity

For text length `n` and pattern length `m`:

- construction: `O(n log^2 n)` from the current suffix-array implementation plus `O(n)` fixed-eight-level wavelet construction;
- count: `O(m)` because each byte performs a constant eight-level rank traversal;
- locate: `O(m + k log k)` for `k` matches because retained suffix rows are sorted back into text order;
- storage: `O(n)` including the full row-position table and byte wavelet matrix.

No succinct FM-index storage claim is made. The row-position table is deliberately retained for exact locate.

## Verification

Deterministic tests cover empty text/pattern semantics, overlapping matches, repeated bytes, and arbitrary binary data including `0x00`, `0x80`, and `0xFF`. A text containing every byte value verifies that the conceptual sentinel does not consume an alphabet value.

The randomized differential corpus uses 400 fixed-seed arbitrary-byte texts with 80 queries each. Every `count` and `locate` result is compared with an independent direct substring scan. Pattern generation mixes actual text substrings with unrelated random byte strings, including empty patterns.

An additional exhaustive formulation check was performed over every length-0..6 string on a three-symbol alphabet and every pattern of length at most three; conceptual-sentinel backward search matched direct scanning in every case.

## Sealed boundary

Phase 19 is sealed after exact merged-main verification. The capability delivered here is the BWT/LF search model itself plus exact locate using a deliberately retained full row-position table. More pattern-query wrappers would not add architectural depth.

The next frontier changes the representation rather than the API surface: Phase 20 replaces the full row-position table with packed sampled-row membership and periodic suffix-position samples. Exact positions are then reconstructed by bounded LF walks, making the locate space/time tradeoff executable and measurable without pretending the Phase-19 structure was already succinct.
