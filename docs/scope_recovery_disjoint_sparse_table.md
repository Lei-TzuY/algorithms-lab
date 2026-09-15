# Scope recovery — disjoint sparse table

## Coverage decision

A fresh live audit at `main@0f7ed3d3c72e145159ec87f1e9c09c32618043e2` found no disjoint sparse-table implementation in default-branch code, pull-request history, or the `scope-recovery-*` branch namespace.

The existing `SparseTableMin` covers a different algebraic boundary: it answers RMQ with two overlapping power-of-two blocks because `min` is idempotent. This recovery adds immutable constant-time range queries for any caller-supplied **associative** operation, including operations that are neither idempotent nor commutative.

HyperLogLog and partially-persistent DSU remain occupied by live recovery branches. Full SPQR decomposition remains deliberately deferred as a much larger graph-decomposition surface. Frozen compiler/backend phases are not reopened.

## Production contract

`algorithms::data_structures::DisjointSparseTable<T, BinaryOperation>` owns an immutable copy of the input sequence and the binary operation.

- public ranges are zero-based, non-empty, half-open `[begin, end)`;
- invalid/empty ranges throw `std::out_of_range`;
- singleton queries return the original element without invoking the operation;
- non-singleton queries invoke the operation exactly once after preprocessing;
- operation order is preserved, so noncommutative associative operations are supported;
- associativity is a caller precondition and is not dynamically checked;
- this direct baseline materializes `T` at each level and therefore requires copyable values.

No update operation, lazy rebuild, idempotence requirement, commutativity requirement, inverse operation, benchmark speedup, or succinct-storage claim is implied.

## Invariant and query proof

At level `k`, let `h = 2^k`. The sequence is partitioned into adjacent blocks of up to `2h` elements. For every block midpoint:

- positions in the left half store ordered suffix aggregates ending immediately before the midpoint;
- positions in the right half store ordered prefix aggregates starting at the midpoint.

For a query `[l, r)` with at least two elements, set `x = l XOR (r-1)` and let `k` be the highest set-bit index of `x`. Then `l` and `r-1` share all more-significant bits but differ at bit `k`; therefore they lie in opposite halves of one level-`k` block. The stored left suffix at `l` covers exactly `[l, midpoint)`, and the stored right prefix at `r-1` covers exactly `[midpoint,r)`. One ordered application of the associative operation returns the aggregate over `[l,r)`.

Unlike the existing overlapping sparse table, these two pieces are disjoint, so no idempotence assumption is needed.

## Complexity

For `n` values there are `O(log n)` levels and `O(n)` direct work/storage per level:

- construction: `O(n log n)` operation applications/copies;
- query: `O(1)` direct work, with zero combines for a singleton and one combine otherwise;
- storage: `O(n log n)` values.

## Verification

Focused final bytes passed:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with fail-fast: 4/4.

Committed evidence covers empty/singleton/bounds validation, noncommutative string concatenation, an instrumented one-combine query regression, and 500 fixed-seed arrays of length 0..160 with 180 random range-sum queries per non-empty array against an independent naïve left-to-right fold.

The first focused build caught a real strict-warning integration issue: `std::bit_width` returns `int`, and implicit conversion to `std::size_t` violated the repository's `-Wsign-conversion -Werror` policy. The final implementation uses explicit conversions rather than weakening the warning gate.
