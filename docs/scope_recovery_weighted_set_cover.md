# Scope recovery: weighted greedy set cover

## Coverage decision

A fresh post-Phase-69 live audit at `main@2e7d70251fe705ac4136ce9f3f4b9c0cf34331ef` found no weighted/unweighted set-cover production surface in default-branch code, pull-request history, or the branch namespace. The long-lived `scope-recovery-minimum-cycle-basis` branch remains genuinely ahead of main and is deliberately untouched. The most recent merged recovery slices were online dynamic minimum spanning forest, Dulmage-DM decomposition, and exact bounded convex Minkowski sum, so this slice deliberately changes proof model again: from exact structural/geometry algorithms to a polynomial-time approximation with an explicit quality theorem.

Historical compiler/backend ROADMAP headings are non-authoritative after Phase 69; this slice follows `docs/scope_recovery_after_phase69.md` plus the fresh live coverage audit.

## Production contract

`greedy_weighted_set_cover(universe_size, sets)` solves the standard non-negative weighted set-cover approximation problem over dense universe `[0, universe_size)`.

- each input set has a full-width `uint64_t` cost and a list of element ids;
- duplicate mentions inside one input set are normalized and never inflate its uncovered gain;
- every mentioned element is validated globally; malformed ids throw `std::out_of_range` even when the universe is empty;
- empty sets are accepted and never selected;
- at each step production chooses a positive-gain set minimizing exactly `cost / newly_covered`;
- ratio comparison uses a continued-fraction / Euclidean comparator and never relies on overflowing cross multiplication or floating point;
- exact ratio ties choose the smaller original set index;
- zero-cost sets therefore dominate every positive-cost alternative while they still have positive gain;
- a feasible result exposes selection order, per-step cost/gain/covered-count diagnostics, and the first selected set that covered every universe element;
- an uncoverable universe returns `std::nullopt`;
- selected-cost accumulation is checked and throws `std::overflow_error` only if this greedy witness itself is not representable as `uint64_t`.

The input family is not retained or mutated.

## Greedy invariant and approximation proof obligation

Immediately before every selection, let `U'` be the uncovered elements. For every input set `S`, production computes exactly `|S intersect U'|`; among positive gains it chooses minimum `cost(S) / |S intersect U'|`. The selected set is then applied atomically to the local result state, so every successful step strictly increases covered cardinality. Consequently there are at most `min(universe_size, set_count)` selections.

For the classical weighted-set-cover guarantee, charge every newly covered element in a selected set the step price `cost / gain`. The charges of one step sum exactly to that selected-set cost, so total charge equals the returned greedy cost.

Fix any set `S` of an optimum cover and order the elements of `S` by the time greedy first covers them. When the j-th such element is about to be covered, at least `|S|-j+1` elements of `S` are still uncovered. Since `S` itself remains an available candidate, greedy's current price is at most

`cost(S) / (|S|-j+1)`.

Thus the total charge assigned to elements of `S` is at most `H_|S| * cost(S) <= H_n * cost(S)`. Every universe element belongs to at least one optimum set, so summing over an optimum cover gives

`greedy_cost <= H_n * OPT`,

where `H_n = 1 + 1/2 + ... + 1/n` for non-empty universes. This charging argument is the approximation theorem/proof obligation; finite randomized tests are implementation evidence, not a proof of the universal bound.

## Verification

Focused exact candidate verification before upload:

- GCC C++20 with repository strict warnings-as-errors: 4/4 tests pass;
- Clang C++20 with the same strict warnings: 4/4 tests pass;
- actual GCC ASan+UBSan build/run: 4/4 tests pass.

Deterministic coverage includes empty-universe validation order, duplicate element mentions, exact-ratio tie breaking, zero-cost covers, infeasible families, final-cost overflow rejection, repeated deterministic execution, and a full-width ratio regression where naïve `uint64_t` cross multiplication would overflow but the correct first choice has total cost exactly `UINT64_MAX`.

Primary randomized verification uses 800 fixed-seed instances with `0..8` universe elements and `0..10` sets. An independent exhaustive oracle enumerates every subset of input sets and computes the exact feasible optimum cost. Production existence must match the oracle. Every successful result is replayed step-by-step against the original sets, including exact chosen gain, first-cover witnesses, cumulative coverage, total cost, and (in the bounded random domain) direct ordinary-integer comparison against every competing cost/gain ratio. The test also checks the observed `greedy_cost <= H_n * OPT` inequality exactly using the small-domain LCM representation of `H_n`.

As an additional local stress check, the production continued-fraction ratio comparator was compared on 200,000 arbitrary full-width numerator/denominator pairs against an exact arbitrary-precision rational oracle; no divergence was found. This stress corpus is evidence only and is not added to repository CI.

## Complexity / non-claims

Let `n` be universe size, `k` the number of input sets, and `M` the total number of element mentions after normalization up to duplicates. Normalization/validation is `O(n + M)` state/work. Each successful selection covers at least one previously uncovered element, so there are at most `min(n,k)` rounds. Each round scans all normalized memberships, giving direct fixed-word complexity

`O(n + M + min(n,k) * M)` time

and `O(n + M)` auxiliary/result state excluding the caller-owned input. Exact ratio comparison uses bounded-width Euclidean division and contributes only a fixed 64-bit-word factor to that code-local bound.

This slice does **not** claim an exact polynomial-time set-cover solver, a better-than-`H_n` universal approximation factor, LP relaxation/rounding, primal-dual set cover, submodular cover, online/streaming cover, negative set costs, arbitrary-precision result costs, or benchmark speedup.

## Scope

The recovery candidate changes exactly four paths:

- `include/algorithms/approximation/weighted_set_cover.hpp`
- `tests/test_weighted_set_cover_cases.hpp`
- one include line in `tests/test_main.cpp`
- `docs/scope_recovery_weighted_set_cover.md`

No CMake, README, historical ROADMAP, recovery-authority rewrite, workflow, benchmark, frozen compiler/backend, occupied minimum-cycle-basis, or temporary-file surface is changed.
