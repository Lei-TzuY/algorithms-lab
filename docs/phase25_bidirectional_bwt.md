# Phase 25 — Bidirectional BWT Exact Interval Extension

## Scope

Phase 25 adds an exact bidirectional search state over the repository's sealed BWT index. The state simultaneously represents a byte pattern `P` as a suffix-array interval in the forward text and `reverse(P)` as an interval in the reversed text. From one common empty state it can prepend or append one arbitrary byte while preserving both intervals exactly.

This is a first-principles bounded-byte-alphabet baseline. It deliberately reuses the sealed BWT occurrence machinery rather than introducing a second suffix-search implementation. It is not an r-index, approximate matcher, compressed constructor, or claim of asymptotically optimal bidirectional indexing.

## State invariant

A state stores half-open intervals

- forward `[l, r)` for `P`, and
- reverse `[L, R)` for `reverse(P)`.

Both intervals are valid conceptual suffix-array ranges over `n+1` rows and have equal cardinality:

`r - l == R - L`.

`match_count()` is that shared cardinality. The empty state is `[0,n+1)` in both indexes because the empty pattern matches every text boundary. States are tied to the index instance that produced them; using a state with a different index is outside the API contract.

## Extension partition and conceptual sentinel

For an interval `I=[a,b)` in one BWT index and a byte `c`, define the offset of the `c` partition as

`offset(I,c) = sentinel_in_I + sum_{x<c}(Occ(x,b)-Occ(x,a))`.

`sentinel_in_I` is one exactly when the conceptual sentinel row lies inside `I`. It must be counted because the conceptual sentinel is lexicographically smaller than every byte and therefore precedes all byte-labelled groups in the peer interval.

The byte alphabet is fixed at 256 values, so the baseline computes this partition offset by explicit alphabet enumeration.

## Left extension

To extend `P` to `cP`:

1. backward-extend the forward interval with the sealed `BwtByteIndex::extend` recurrence;
2. let `k` be the new forward interval cardinality;
3. compute `offset(forward_interval,c)`; and
4. project the reverse interval to `[L+offset, L+offset+k)`.

The projection is exact because occurrences of `P`, ordered by the suffix following `P`, are partitioned by their preceding conceptual symbol in sentinel-then-byte order.

## Right extension

To extend `P` to `Pc`, apply the symmetric operation to `reverse(P)`:

1. backward-extend the reverse interval by `c`, producing the interval for `c reverse(P) = reverse(Pc)`;
2. use the reverse interval's byte-partition offset; and
3. project the forward peer interval by that offset and new cardinality.

Thus left and right extension use the same invariant rather than separate search implementations.

## Exactness and absent states

An empty interval remains a valid exact insertion-point interval. Extending an absent state therefore remains well defined: the corresponding longer pattern is also absent, and both paired zero-cardinality intervals continue to match the independent suffix-array insertion points.

All byte values `0..255` are data. The sentinel remains conceptual and never consumes a byte value.

## Complexity

Let `R` be the number of resident BWT byte runs in the relevant sealed index. One extension performs one ordinary run-length BWT backward step plus at most 256 rank-difference queries. With the current run-length occurrence representation, the conservative extension bound is

`O(256 log(R+1))`,

which is effectively constant-alphabet work but is intentionally written with the alphabet factor visible. State storage is `O(1)`.

The bidirectional index owns two full `BwtByteIndex` instances, one for the text and one for its reversal. Construction and resident storage therefore inherit the sealed BWT indexes' current suffix-array construction and sampling structures; Phase 25 makes no compressed-construction or space-optimal claim.

## Verification

Focused pre-upload verification used an independent raw suffix sorter rather than the production suffix-array implementation. After every left or right extension it compared all four interval endpoints and the shared match count with the independently computed forward/reverse suffix-array intervals.

Coverage includes:

- empty text and empty pattern;
- classic repeated-pattern cases;
- absent states followed by further extensions;
- arbitrary bytes including `0x00` and `0xFF`;
- all 256 single-byte extensions; and
- randomized mixed left/right extension sequences over full-byte texts.

The implementation merged as `main@8b0b9618ee1e53b47b36ef47e3ac6149d223902d`. Post-merge CI run `34181328896` completed successfully on GCC release, Clang release, and GCC ASan+UBSan. The independent Phase-25 sealing audit found no correctness or integration blocker.

## Status

**SEALED.** Exact paired-interval extension, arbitrary-byte semantics, conceptual-sentinel ordering, absent-state continuation, and the conservative complexity boundary are verified. The next frontier is bounded-substitution approximate matching built on this exact bidirectional state; Phase 25 itself does not claim approximate matching.
