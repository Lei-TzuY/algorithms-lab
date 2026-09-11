# Scope recovery: Mo's offline range-distinct queries

## Why this slice

The post-Phase-69 recovery freezes the historical compiler/backend excursion and
requires fresh coverage audits before each new conquest slice. After the bounded-
exact Li Chao minimum tree reached exact merged-main green, a live repository
search found no Mo's algorithm, square-root block ordering, or static offline
range-distinct capability. This slice deliberately changes proof model again
instead of farming another line-envelope or tree-query variant.

## Production contract

`mo_distinct_counts(values, queries)` answers static half-open range queries
`[begin,end)` over arbitrary signed 64-bit values. Results are returned in the
original query order. Empty ranges are valid and contain zero distinct values;
`begin > end` and endpoints beyond the sequence are rejected before processing.

Values are deterministically coordinate-compressed. Queries are reordered by a
square-root-sized left block and an alternating right-end order. A single mutable
window is then moved with add/remove operations while a frequency table stores
exactly the multiplicity of every compressed value inside the current window.
`std::sort`, `std::unique`, and `std::lower_bound` are supporting primitives for
compression/offline ordering; no library routine answers the range-distinct
problem itself.

## Correctness obligations

**Window invariant.** Before recording a query result, the maintained window is
exactly that query's half-open interval. Every array position enters or leaves
through one boundary update.

**Frequency invariant.** For every compressed symbol, its table entry equals the
number of occurrences in the current window. The `distinct` counter is therefore
exactly the number of positive frequency entries. Removing a zero-frequency
symbol is treated as an internal invariant violation rather than silently
normalizing corrupted state.

**Offline-order independence.** Reordering queries changes only evaluation order.
Answers are written back through the original query index, so input query order
has no semantic effect on any range result.

## Complexity boundary

For `N` values and `Q` queries, deterministic coordinate compression costs
`O(N log N)`, query ordering costs `O(Q log Q)`, and the chosen square-root block
order performs `O((N+Q) sqrt(N))` window add/remove operations in the standard
worst-case accounting. Each window update is `O(1)` after compression. Auxiliary
storage is `O(N+Q)`.

This is the classical static-query Mo baseline. It does not claim online query
support, point updates, Hilbert-order bounds, frequency-of-frequency statistics,
or a universal speedup over direct scanning on every workload.

## Verification

Deterministic tests cover empty input, empty ranges, reversed/out-of-range query
rejection, duplicate-heavy data, full/singleton ranges, exact `INT64_MIN` /
`INT64_MAX` values, repeated execution, and query-order reversal.

A fixed-seed differential corpus runs 1,200 arrays of length 0-96 with up to 128
queries each. Every production answer is compared with an independent direct
scan using `std::set`; the same query set is then shuffled and checked again.
Focused GCC strict-warning, Clang strict-warning, and real GCC ASan+UBSan builds
all pass 4/4 native test groups.

## Recovery governance

This slice does not resume the frozen Phase-45-69 compiler/backend sequence and
does not rely on the historically stale active ROADMAP heading. It is selected
by the recovery authority plus a fresh live coverage audit after Li Chao reached
merged-main green.
