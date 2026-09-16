# Scope recovery: Schroeppel-Shamir exact subset sum

## Coverage decision

A fresh live code, pull-request-history, and branch audit after the graphical
simple-degree-sequence checkpoint found no subset-sum, meet-in-the-middle, or
Schroeppel-Shamir production surface in `algorithms-lab`. Nearby recovery work
on BFPRT selection, Count-Sketch, and Schreier-Sims is already occupied and is
left untouched. This slice changes proof model again: it studies an exact
exponential algorithm whose main lesson is a time/space tradeoff rather than an
adjacent graph-realization variant.

## Production contract

`schroeppel_shamir_subset_sum(values, target)` accepts at most 40 signed 64-bit
values and returns either no solution or a replayable witness of original dense
indices whose values sum exactly to `target`.

The input is split into four balanced contiguous groups. Every quarter subset
sum is materialized together with its local mask. The two left quarters are
combined lazily in ascending pair-sum order, and the two right quarters lazily
in descending order. Both Cartesian-product streams use the repository's
first-principles `BinaryHeap`; the full half-sum tables are never materialized.
A two-ended monotone search advances the left stream when the current total is
too small and the right stream when it is too large.

The empty subset is a valid witness for target zero. Returned indices are unique,
ascending original indices. The API does not claim a canonical or
lexicographically-minimum solution when multiple subsets satisfy the target.

## Representability boundary

Before enumeration, the implementation sums the magnitudes of all positive and
negative inputs separately. It rejects inputs for which some mathematical
subset sum could leave the signed 64-bit domain. This makes every quarter sum,
pair sum, and final comparison exactly representable rather than relying on
signed overflow. `INT64_MIN` is handled through unsigned magnitude arithmetic.
Inputs longer than 40 elements are rejected explicitly.

## Correctness obligations

Every subset of the original input corresponds to exactly one choice of local
mask in each of the four groups, hence to one left pair sum and one right pair
sum. Sorting each quarter by `(sum, mask)` lets each fixed first-quarter row be
enumerated monotonically through the second quarter. A heap merge therefore
emits the complete left Cartesian product in nondecreasing order; the symmetric
max-heap emits the complete right product in nonincreasing order.

At any step, if `left + right < target`, no unseen right value can repair the
current left value because every future right value is no larger, so advancing
the left stream is safe. The symmetric argument holds when the total is too
large. Equality reconstructs the four masks and therefore a valid original-index
witness. These invariants establish completeness of the streamed two-sum search.

## Verification

Focused exact-candidate evidence includes:

- strict GCC and Clang warnings-as-errors builds;
- an actual GCC ASan+UBSan build with fail-fast runtime checks;
- deterministic empty, duplicate, mixed-sign, no-solution, length-bound, and
  signed-representability boundary cases;
- 260 fixed-seed random instances through 16 elements checked against exhaustive
  enumeration of every subset;
- 160 fixed-seed 20--28 element instances checked against an independent ordinary
  two-way meet-in-the-middle implementation;
- a 40-element structured instance exercising the four-way heap streams.

The exhaustive and two-way solvers are verification oracles only. They do not
share the production four-way heap recurrence and do not replace the proof
obligations above.

## Complexity and non-claims

For balanced quarters, at most `Theta(2^(n/2))` pair sums can be consumed and
each heap operation costs `O(log 2^(n/4))` in this direct implementation.
Accordingly the conservative implementation bound is
`O(2^(n/2) log 2^(n/4))` time and `O(2^(n/4))` auxiliary storage, plus the
returned witness. The 40-element contract is an engineering bound for this lab
implementation.

No polynomial-time subset-sum claim, pseudo-polynomial DP claim, approximation
scheme, cryptographic hardness claim, canonical-witness claim, arbitrary-
precision arithmetic, or benchmark speedup is implied.
