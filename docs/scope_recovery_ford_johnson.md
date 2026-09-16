# Scope recovery: Ford-Johnson merge-insertion sorting

## Coverage decision

A fresh live-state audit after the bounded-exact half-plane-intersection checkpoint found no Ford-Johnson / merge-insertion / Jacobsthal-insertion implementation in default-branch code, pull-request history, or the active branch namespace. Several initially plausible alternatives were rejected as already covered or occupied, including Hopcroft DFA minimization, Ukkonen suffix trees, Greenwald-Khanna quantiles, exact simplex, optimal-BST Knuth optimization, push-relabel flow, splay trees, and minimum-enclosing-circle work.

This slice follows `docs/scope_recovery_after_phase69.md`: it returns to a comparison-complexity proof model rather than extending the immediately preceding geometry surface or the prospectively frozen compiler/backend history.

## Production contract

`ford_johnson_sort(input)` accepts a span of signed 64-bit values and returns:

- an ascending sorted copy of the complete input multiset;
- the exact number of value comparisons performed by this implementation.

Empty, singleton, duplicate, and full signed-64 boundary values are supported. The implementation under study does not call `std::sort`, `std::stable_sort`, or another library sorting implementation. The public baseline intentionally fixes the value domain to `int64_t`; it does not imply a generic exception-safety contract for arbitrary caller comparators.

## Merge-insertion invariant / comparison proof boundary

Adjacent inputs are paired with one comparison. Each pair retains its smaller member `b_i` and larger member `a_i`. The `a_i` values are recursively sorted by the same algorithm, preserving each `b_i` partner identity.

The main chain starts as

`b_1, a_1, a_2, ..., a_m`.

Remaining `b_i` values are inserted in the classical Jacobsthal-group order `b_3,b_2,b_5,b_4,b_11,...`. A partnered `b_i` is binary-searched only in the prefix strictly before its already-resident `a_i`; an odd unpaired element is searched against the complete current chain. The Jacobsthal group boundaries make those searched prefix sizes align with near-complete binary-search trees.

For `n` elements the classical merge-insertion comparison bound is

`sum_{k=1..n} ceil(log2(3k/4))`.

This bound is the algorithmic proof obligation; tests do not manufacture it from sampled timings. Ford-Johnson is historically comparison-efficient, but this PR does **not** claim information-theoretic optimality for every input size.

## Verification

Focused final candidate passed before upload under:

- GCC C++20 repository strict warnings-as-errors;
- Clang C++20 repository strict warnings-as-errors;
- actual GCC ASan+UBSan with fail-fast/leak detection.

Evidence includes signed-64 extremes, duplicates, empty/singleton semantics, and a deterministic nine-element adversarial ordering that requires exactly 19 comparisons.

The primary exhaustive comparison check enumerates every permutation for `n=0..8`, requires exact sorted output, enforces the classical upper bound on every permutation, and observes exact worst-case counts `0,0,1,3,5,7,10,13,16`.

A separate 1,500-case fixed-seed corpus uses lengths `0..128` with heavy duplicates. `std::sort` is used only as an independent result oracle; production never calls it. Repeated execution must reproduce both output and comparison count.

## Complexity / non-claims

The comparison count follows the merge-insertion bound above. This direct educational implementation stores identity vectors, linearly locates current partners, and inserts into `std::vector`; consequently ordinary bookkeeping and data movement are conservatively `O(n^2)` time with `O(n)` resident auxiliary state plus recursion. The point of this slice is comparison structure, not a claim of fastest wall-clock sorting.

No stability guarantee, generic-comparator API, in-place-storage bound, sorting network, lower-bound proof, universal optimal-comparison claim, or benchmark speedup is implied.

## Scope

The recovery change is exactly three paths:

- `include/algorithms/sorting/ford_johnson.hpp`;
- appended tests in the already-registered `tests/test_searching_sorting.cpp`;
- `docs/scope_recovery_ford_johnson.md`.

No CMake, test registry, README, historical ROADMAP, recovery-authority, workflow, benchmark, frozen compiler/backend, geometry, or temporary-file churn is required.
