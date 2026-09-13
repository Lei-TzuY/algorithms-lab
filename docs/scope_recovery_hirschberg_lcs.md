# Scope recovery: linear-space LCS reconstruction via Hirschberg

## Coverage decision

This slice continues post-Phase-69 algorithms/data-structures scope recovery with a classical dynamic-programming space/reconstruction proof model absent from live code, branches, and PR history: Hirschberg longest-common-subsequence reconstruction.

Exact base is `main@22e05c56142d528340a2a4ad3039a8b9215d67ae`, where Duval Lyndon factorization is merged-main green in CI run `34728051944` on GCC release, Clang release, and GCC ASan+UBSan. The repository had zero open PRs and zero open issues before remote implementation. Fresh searches found no `Hirschberg`, `LCS`, or longest-common-subsequence implementation or occupied branch. The existing dynamic-programming phase contains LIS and edit distance but no LCS surface. The genuinely occupied long-lived `scope-recovery-minimum-cycle-basis` surface remains untouched, and `docs/scope_recovery_after_phase69.md` remains the prospective authority rather than stale historical ROADMAP headings.

## Production contract

`hirschberg_lcs(first, second)` returns increasing index pairs `(i,j)` such that `first[i] == second[j]`; replaying those pairs spells one maximum-length common subsequence.

- empty inputs return an empty witness;
- arbitrary bytes are compared by exact byte equality;
- the result is deterministic for the same byte strings but is not claimed to be the globally lexicographically smallest or otherwise canonical LCS;
- split ties choose the smallest second-sequence split, and one-byte base cases choose the earliest available match;
- when the caller's second input is longer, production swaps internal roles and swaps result indices back, keeping every DP row bounded by the shorter original input.

No edit-script API, Unicode collation, canonical-LCS ordering, bit-parallel LCS, or subquadratic-time claim is implied.

## Algorithm and proof boundary

For a non-trivial first interval, production splits it in half. A forward two-row dynamic program computes the LCS length of the left half against every prefix of the second interval. A backward two-row dynamic program computes the LCS length of the right half against every suffix. A split maximizing `forward[j] + backward[j]` lies on an optimum LCS path; production recursively solves the two rectangles and concatenates their witnesses.

The DP row vectors are destroyed before recursive calls. Top-level orientation makes the second logical input no longer than the first, and recursive second intervals remain subranges of that shorter original input, so DP-row width is `O(min(m,n))`. The first dimension halves at each recursion, giving logarithmic recursion depth; output storage is proportional to the returned LCS length. The direct total time bound is `O(mn)`.

Correctness relies on the ordinary LCS recurrence and the Hirschberg split theorem. Tests are implementation evidence rather than a substitute for those results or an empirical proof of the space bound.

## Independent verification

The focused exact candidate passed before upload under:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with sanitizer failures halting: 4/4.

The primary oracle is an independent full `O(mn)` DP table that computes only the exact optimum LCS length; it does not use Hirschberg forward/backward split selection. Every returned index-pair witness is independently replayed for bounds, byte equality, and strict increase in both input coordinates.

Evidence includes empty/disjoint/identical inputs, the classical `ABCBDAB` / `BDCABA` length-four case, arbitrary bytes, highly unbalanced dimensions in both caller orders, exhaustive binary-string pairs through length six on both sides (**16,129 input pairs**), and 1,400 fixed-seed arbitrary-byte pairs with lengths through 28.

## Scope

Exactly four paths should differ from the merged-green base:

- `include/algorithms/dynamic_programming/hirschberg_lcs.hpp`;
- `tests/test_hirschberg_lcs_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this proof/coverage document.

No CMake, README, historical ROADMAP, recovery authority, workflow, benchmark, frozen compiler/backend, occupied minimum-cycle-basis, or temporary-file surface is modified. Connector file-level commits should be squash-merged so `main` receives one coherent recovery checkpoint.
