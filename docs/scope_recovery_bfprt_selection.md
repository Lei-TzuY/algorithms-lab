# Scope recovery — deterministic linear-time selection (BFPRT)

## Coverage decision

A fresh live audit after the DSU-on-tree subtree-frequency checkpoint reached
merged-main green found no BFPRT / median-of-medians / quickselect / deterministic
order-statistic implementation in live code, commit history, pull-request history,
or branch names. The post-Phase-69 scope-recovery policy remains authoritative;
the historical compiler/backend ROADMAP tail is intentionally not used as a
frontier.

This slice deliberately changes proof model again. Recent recovery work covered
small-to-large tree aggregation, Cartesian-tree RMQ, graph isomorphism, and
Knuth-optimized interval DP. Deterministic selection instead adds a classical
prune-and-search recurrence with a worst-case linear guarantee.

## Production contract

`deterministic_select_kth(values, k)` returns the zero-based kth order statistic
of a non-empty signed-64-bit snapshot.

- the input is not mutated;
- `k >= values.size()` (including every empty-input call) throws
  `std::out_of_range`;
- duplicates have ordinary multiset order-statistic semantics;
- the full `int64_t` value domain is supported;
- execution is deterministic;
- production does not call `std::sort`, `std::nth_element`, or another library
  selection routine.

The implementation copies the snapshot once, so its public storage contract is
explicit rather than pretending to be an in-place selector.

## Algorithm and proof obligation

Every active range is divided into consecutive groups of at most five. Each
small group is insertion-sorted from first principles and its median is moved
into a compact prefix. The median of those medians is selected recursively and
used as the pivot for a Dutch-national-flag three-way partition into `< pivot`,
`== pivot`, and `> pivot` regions. Three-way partitioning makes duplicate-heavy
inputs terminate without repeatedly recursing through values equal to the pivot.

For every full five-element group whose median is on the low side of the chosen
median-of-medians pivot, at least three group elements are `<= pivot`; symmetrically,
for every qualifying high-side group at least three are `>= pivot`. Discarding a
constant number of exceptional groups for the pivot group and a possible final
partial group still removes at least `3n/10 - O(1)` elements from either strict
side. Therefore the worst-case recurrence has the classical form

`T(n) <= T(ceil(n/5)) + T(7n/10 + O(1)) + O(n) = O(n)`.

The main problem shrinks iteratively; recursion is used only for selecting the
median-of-medians pivot. The public function uses `O(n)` auxiliary storage for
its working copy, with `O(log n)` recursive pivot-selection depth.

The linear bound is a proof obligation of the group-of-five recurrence. Passing
finite tests is implementation evidence, not a proof of the asymptotic theorem.

## Verification

Focused final candidate passed:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with halt-on-error: 4/4.

Deterministic evidence covers empty/rank validation, singleton and two-element
inputs, `INT64_MIN` / `INT64_MAX`, all-equal data, every rank of sorted and
reverse-sorted 257-element arrays, duplicate-heavy data, and an organ-pipe shape.
Every all-ranks test also verifies that the caller's input snapshot is unchanged.

The primary randomized oracle is independent of BFPRT pivoting and partitioning:
test-only `std::sort` computes the exact kth value. The committed corpus includes
1,400 fixed-seed arrays of length 1..320 drawn from a duplicate-heavy signed
range and 500 additional full-width `int64_t` arrays of length 1..180. Production
must equal the sorted oracle exactly for every chosen rank.

## Complexity and non-claims

Worst-case time is `O(n)`. Public auxiliary storage is `O(n)` because the API is
non-mutating; median-of-medians recursion contributes only logarithmic stack
depth.

This slice does not claim stable partitioning, streaming/online selection,
weighted quantiles, multiple-rank batched selection, randomized quickselect,
Floyd-Rivest bounds, comparison-count optimality, or an in-place `O(1)`-storage
API. Standard-library sorting is used only by tests as an independent oracle.
