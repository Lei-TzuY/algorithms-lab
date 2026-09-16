# Scope recovery: bounded-exact L2 isotonic regression

## Coverage decision

A fresh live-state/code audit at `main@8a59adec60dd0ceb4873e02ab94682ccccb8e7b8`
found no isotonic-regression / pool-adjacent-violators implementation in the
default branch. Recent recovery work covers submodular minimization, matroid
parity, filters, selection, subset sum, graph realization, coding, sketches,
geometry, graph optimization, and many other standard surfaces. This slice
changes proof model to convex projection over a total order rather than extending
the immediately preceding matroid/submodular frontier.

Prospective scope remains governed by `docs/scope_recovery_after_phase69.md`;
the frozen Phase-45--69 compiler/backend history and stale historical ROADMAP
frontier remain untouched.

## Production contract

`least_squares_isotonic_regression(observations)` computes the unweighted L2
isotonic regression of an integer sequence under the constraint
`x[0] <= x[1] <= ... <= x[n-1]`.

The API deliberately returns exact rational block values instead of floating
point fitted samples. Each block stores `[begin,end)`, the exact integer `sum`,
and `count`; its fitted value is exactly `sum / count`. Adjacent equal means are
coalesced, so returned block means are strictly increasing.

The educational exact domain is explicit:

- at most 50,000 observations;
- every observation lies in `[-10^9,10^9]`.

Under those bounds every block sum has magnitude at most `5e13`, and every
cross-product used to compare two block means has magnitude at most `2.5e18`,
strictly inside signed `int64_t`. Production therefore performs no floating-point
comparison and does not depend on implementation-specific wider integers.

## PAVA invariant and proof obligation

Production starts with one block per observation. While the two final block means
are non-increasing it replaces them by their union. After each merge the stack is
the PAVA partition of the processed prefix and its means are strictly increasing.

The least-squares isotonic projection theorem states that repeatedly pooling
adjacent violating blocks and replacing them by their arithmetic mean produces
the unique fitted vector minimizing squared error over the monotone cone. Equal
adjacent block means can be coalesced without changing that vector, giving the
canonical coarsest block representation returned here.

The universal optimality claim comes from this PAVA projection argument, not from
the randomized corpus.

## Verification independence

Focused repo-style verification passed before upload under GCC C++20 strict
warnings-as-errors, Clang C++20 strict warnings-as-errors, and actual GCC
ASan+UBSan.

Deterministic tests cover empty input, equal-value coalescing, a fully decreasing
sequence that pools into one rational block, mixed violations, and both exact-domain
validation boundaries.

The primary randomized oracle is structurally independent. For 700 fixed-seed
integer sequences of length 1..9 it enumerates every contiguous partition,
computes each partition's block means directly, discards non-monotone partitions,
and selects the globally minimum squared-error fit. Production must have the same
objective value, cover the input exactly, remain monotone, and expose strictly
increasing canonical block means.

## Complexity and non-claims

PAVA pushes each original block once and every merge permanently removes one
block, so this implementation is `O(n)` time and `O(n)` result/auxiliary storage.

This slice does not claim weighted isotonic regression, partial-order isotonic
regression, L1/other losses, online updates, unconstrained full-width integer
inputs, or floating-point observations.

## Scope

Exactly four paths change: the header-only production API, one recovery test-case
header, one include line in `tests/test_main.cpp`, and this proof/coverage document.
No CMake, README, historical ROADMAP, workflow, benchmark, frozen backend, or
temporary-file surface changes.
