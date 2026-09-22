# Scope recovery: exact static range-mode index

## Coverage decision

A fresh live-state audit after deterministic pair-replacement grammar merged as
`main@bc6d151a0501beba3476fe1cf96219966920bc6c` found zero open pull
requests and zero open issues. Default-branch code, branch names, and pull-request
history contained no exact static range-mode index or occupied recovery branch
for that capability.

The repository already contains:

- Mo's offline exact range-distinct queries;
- a byte wavelet matrix with byte-valued range rank and order statistics;
- ordinary and persistent segment-tree infrastructure;
- streaming Misra-Gries heavy-hitter summaries.

This slice is intentionally different from all four. It answers online static
subarray **mode** queries over arbitrary signed 64-bit values and returns both
the exact winning value and its exact frequency.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; the frozen compiler/backend history and
stale historical ROADMAP remain untouched.

## Public contract

`StaticRangeMode` is constructed from a fixed sequence of signed 64-bit
values.

Queries use half-open ranges `[begin,end)`.

- `query(begin,end)` returns `nullopt` for an empty range;
- every nonempty query returns the value with maximum frequency in the range;
- if multiple values have the same maximum frequency, the numerically smaller
  value wins deterministically;
- the returned frequency is exact;
- `frequency(value,begin,end)` returns the exact occurrence count of that value
  in the same half-open range;
- reversed ranges or endpoints beyond the sequence are rejected;
- `INT64_MIN` and `INT64_MAX` are ordinary values.

The structure is static after construction. Point updates, append operations,
persistence, and fully dynamic range-mode queries are outside this slice.

## Representation

Construction coordinate-compresses all distinct values in sorted numeric order.
Compressed ids therefore preserve the public tie-break: a smaller id means a
smaller original value.

For every compressed value, production stores its sorted occurrence positions.
An exact frequency query becomes two binary searches in that position list.

The sequence is also partitioned into blocks of size approximately `sqrt(N)`.
For every inclusive interval of complete blocks, construction precomputes one
exact core mode, again breaking frequency ties by smaller compressed id.

The block table is used only for intervals composed entirely of whole blocks;
a final partial block is never treated as complete by a query.

## Query candidate theorem

Consider a nonempty query and split it into:

1. a left boundary fringe;
2. zero or more complete blocks forming the core;
3. a right boundary fringe.

If there is no complete-block core, production simply considers every value in
the query.

Otherwise production considers:

- the precomputed exact mode of the complete-block core;
- every distinct value that occurs in either boundary fringe.

For each candidate, the final frequency is recomputed exactly over the full
query from its occurrence-position list.

Why is this candidate set sufficient?

Let `x` be the public winning mode of the full query.

If `x` occurs in a fringe, it is explicitly included.

Otherwise every occurrence of `x` lies in the core, so its full-query frequency
equals its core frequency. Let `c` be the precomputed core mode. By definition,
`freq_core(c) >= freq_core(x)`, and adding fringes cannot decrease
`freq_query(c)`. Therefore `c` reaches at least the full-query frequency of
`x`.

Since `x` is already a full-query mode, `c` must also be a full-query mode.
If their frequencies tie, the core table's smaller-value tie break together with
the public smaller-value tie break ensures that the chosen candidate is exactly
the required deterministic mode.

Thus the core mode plus fringe values is sufficient; scanning every distinct
value in every query is unnecessary.

## Precomputation invariant

For each first block, construction clears one frequency vector and extends the
right block one block at a time.

After processing block `j`, each counter equals the exact number of
occurrences of that compressed value in the block interval `[first,j]`.
The stored table entry is therefore the exact mode of that complete-block
interval, with smaller-value tie breaking.

The mode table does not store approximate summaries or Boyer-Moore residuals.

## Complexity boundary

Let:

- `N` be the sequence length;
- `U <= N` be the number of distinct values;
- `B = Theta(sqrt(N))` be the chosen block size;
- `K = Theta(sqrt(N))` be the number of blocks.

Construction performs:

- deterministic coordinate compression in `O(N log N)`;
- occurrence-list construction in `O(N log U)` with the straightforward
  lower-bound mapping used here;
- complete-block mode precomputation in `O(KN + KU) = O(N sqrt(N))`;
- `O(N + U + K^2) = O(N)` resident indexing storage asymptotically for the
  chosen block shape.

A nonempty query has at most `O(B)` fringe candidates plus one core candidate.
Candidate deduplication and exact occurrence-list counting give a conservative
`O(sqrt(N) log N)` query bound.

`frequency(value,begin,end)` is `O(log U + log N)`.

This slice does **not** claim:

- dynamic updates;
- optimal theoretical range-mode bounds;
- constant-time queries;
- succinct storage;
- a universal speedup over direct scanning on small inputs;
- benchmark-backed latency or throughput.

The square-root decomposition is an exact baseline, not a performance claim.

## Independent verification

The primary committed oracle does not reuse block decomposition, coordinate
compression, precomputed core modes, or occurrence-position lists.

It directly scans each queried subarray into an ordered
`std::map<int64_t,size_t>`, selects the maximum count, and applies the same
public smaller-value tie break.

Committed coverage includes:

- empty-sequence and empty-range behavior;
- reversed/out-of-range rejection;
- exact frequency queries for present and absent values;
- deterministic mode ties;
- all-unique ranges;
- duplicate-heavy ranges whose boundary fringes can change the core winner;
- exact `INT64_MIN` / `INT64_MAX` values;
- every subrange of fixed deterministic arrays;
- 320 fixed-seed random arrays of length `0..24`, checking **every** subrange;
- 100 larger fixed-seed arrays of length `32..256`, each with 300 random mode
  queries plus independent exact-frequency checks.

A supplementary model-level differential pass over many additional short random
arrays found no mismatch between the candidate theorem implementation and direct
mode counting. That supporting model check is not a compiler/sanitizer claim.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/data_structures/static_range_mode.hpp`;
- `tests/test_static_range_mode_cases.hpp`;
- `docs/scope_recovery_static_range_mode.md`.

Two pre-PR correctness hardening commits:

- correct an all-distinct deterministic tie expectation in the test;
- avoid an unnecessary block-end addition overflow surface.

No CMake, test-main, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Base: `bc6d151a0501beba3476fe1cf96219966920bc6c`.
