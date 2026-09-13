# Scope recovery: exact tournament feedback arc set

## Coverage decision

A fresh live audit after the deterministic greedy weighted-spanner recovery found no tournament feedback-arc/minimum-upset-ordering production surface on the default branch, no pull request mentioning feedback arc, and no occupied `scope-recovery-feedback-arc*` branch. Several superficially attractive alternatives were rejected because they were already present (minimum mean cycle, stable matching, SMAWK, B-tree, optimal BST, chordal recognition, cuckoo hashing, Elias-Fano, and related structures). Full SPQR decomposition also remains uncovered, but it was deferred because it is a much larger triconnected-decomposition surface with a poorer review/verification boundary for one recovery slice.

The historical compiler/backend ROADMAP remains non-authoritative prospectively. This recovery is selected from fresh executable-coverage evidence rather than by inventing another numbered phase.

## Production contract

`minimum_feedback_arc_set_tournament(graph)` solves the unweighted minimum feedback-arc-set / minimum-upset-ordering problem exactly for a **simple directed tournament**.

- every unordered vertex pair must have exactly one directed arc;
- self-loops, parallel arcs, missing pairs, bidirectional pairs, and undirected input are rejected;
- edge weights are intentionally ignored: the objective counts backward arcs;
- `n > 20` is rejected with `std::length_error`; this is an exact exponential educational baseline, not a scalable general solver;
- the result returns the minimum backward-arc count, the lexicographically smallest optimum ordering, and the explicit original arcs that point backward in that ordering.

## Exact subset-DP invariant

For a vertex subset `S`, let `dp[S]` be the minimum number of backward arcs among all linear orderings of exactly `S`. If vertex `v` is chosen first, every arc `u -> v` with `u` in `S \ {v}` is necessarily backward, while all other backward arcs lie completely inside the remaining subset. Therefore

`dp[S] = min_{v in S} (dp[S - {v}] + |N^-(v) intersect (S - {v})|)`.

The empty subset has value zero. Incoming-neighbor sets are stored as bit masks, so each transition counts the new contribution with a population count.

For equal-cost choices the implementation selects the smallest first vertex. By induction, the recursively reconstructed remainder is already the lexicographically smallest optimum ordering of the smaller subset, so this tie rule yields the lexicographically smallest optimum ordering for `S`.

After reconstruction, every original arc is replayed against the position map. The returned feedback-edge witness is exactly the set of arcs whose tail appears after their head; production checks that the witness cardinality equals the DP optimum.

## Independent verification

Focused candidate verification passes under:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with fail-fast/leak detection: 4/4.

Deterministic tests cover the empty tournament, a transitive tournament, a directed 3-cycle, ignored edge weights, every invalid tournament shape above, the `n=20` domain boundary via validation, and deterministic replay.

Primary randomized evidence uses 280 fixed-seed tournaments with `1 <= n <= 8`. The oracle enumerates **every permutation**, directly counts backward arcs, and retains the first lexicographic optimum. Production must match both optimum count and complete ordering exactly. Witness edges are independently replayed against the original adjacency matrix. The oracle does not use subset DP or the production recurrence.

## Complexity and non-claims

With `n <= 20`, the direct implementation uses

- `O(n 2^n + n^2)` time;
- `O(2^n + n^2)` memory.

No polynomial-time result for general feedback-arc set is claimed. This slice does not solve weighted tournament FAS, arbitrary directed-graph FAS, approximation variants, parameterized algorithms, or large-`n` instances. The explicit size limit is part of the public correctness contract rather than a hidden performance caveat.

## Scope

The recovery slice changes exactly four paths:

- `include/algorithms/graphs/tournament_feedback_arc_set.hpp`;
- `tests/test_tournament_feedback_arc_set_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- `docs/scope_recovery_tournament_feedback_arc_set.md`.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark, frozen compiler/backend, or temporary-file churn is required.
