# Scope recovery: bounded exact Lehmer prime counting

## Coverage decision

A fresh post-Phase-69 recovery audit at `main@2bcbd5208250c8d582182220b9ca2d8c4f6ac90c`
found no prime-counting / Lehmer / Meissel implementation in live code, pull-request
history, or branches. The genuinely occupied `scope-recovery-minimum-cycle-basis`
and `scope-recovery-exact-treewidth` surfaces are deliberately untouched. This
slice also leaves the frozen Phase-45--69 compiler/backend history and the stale
historical ROADMAP frontier unchanged.

The new proof model is combinatorial prime counting rather than another
polynomial, geometry, dynamic-connectivity, or matroid variant.

## Production contract

`algorithms::number_theory::LehmerPrimeCounter` is a reusable exact counter for
`pi(x)`, the number of primes not exceeding `x`.

- exact supported domain: `0 <= x <= 100,000,000,000`;
- larger inputs fail closed with `std::out_of_range`;
- construction builds an ordinary prime-prefix sieve below 400,001 and a compact
  `phi(x,a)` table for `x < 25,000`, `a <= 100`;
- every query clears only the memoized Lehmer subproblem cache; the deterministic
  precomputed tables are reused;
- floating-point `sqrt` / `cbrt` are estimates only: every integer square,
  cube, and fourth root is corrected by exact division-based comparisons before
  it participates in the combinatorial formula.

The object mutates an internal query cache from `count`, so concurrent calls on
one instance are not claimed thread-safe.

## Lehmer decomposition and proof obligation

For a query `x`, let

- `a = pi(floor(x^(1/4)))`,
- `b = pi(floor(sqrt(x)))`, and
- `c = pi(floor(cuberoot(x)))`.

Production evaluates the classical Lehmer decomposition

`pi(x) = phi(x,a) + ((b+a-2)(b-a+1))/2 - corrections`,

where the outer correction subtracts `pi(x/p_i)` for `a <= i < b` and the
nested correction handles products of two primes for `i < c`. `phi(x,a)` counts
positive integers at most `x` that are not divisible by the first `a` primes and
uses the exact recurrence

`phi(x,a) = phi(x,a-1) - phi(floor(x/p_a),a-1)`.

The table is only a memoized base region for that recurrence; it is not an
independent prime-counting oracle. Correctness relies on the classical Lehmer
partition of integers by their least prime factors plus the `phi` recurrence.
Finite tests are implementation evidence, not a proof of those identities.

## Verification

The exact candidate passed before upload under:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan: 4/4.

Deterministic known answers include every power of ten from `10` through
`10^11`, in particular

`pi(10^11) = 4,118,054,813`.

Boundary tests cover `0`, `1`, `2`, the exact public maximum, rejection of
`max+1`, and repeated-query stability.

The primary randomized oracle is structurally independent from Lehmer's
recurrence: tests build a direct Eratosthenes prime list through `10^7` and use
`std::upper_bound` only in test code. Five hundred fixed-seed queries are drawn
from `400,001..10^7`, deliberately above the production direct-sieve threshold,
and the complete counts must match.

## Complexity and non-claims

With configurable sieve bound `S`, phi-table width `X`, and stored phi-prime
count `A`, construction costs `O(S log log S + A*X)` time and `O(S + A*X)`
storage. The public implementation fixes `S=400001`, `X=25000`, and `A=100`, so
those resident tables are bounded by the API contract.

Query execution follows the direct Lehmer/phi recurrences with memoized Lehmer
subproblems. This educational bounded baseline deliberately does **not** claim a
best-known analytic prime-counting complexity, arbitrary-`uint64_t` support,
segmented low-memory operation, parallelism, or benchmark speedup. Wall-clock
focused timings were used only to choose a practical public bound and are not
correctness evidence.

## Scope

Exactly four paths change:

- `include/algorithms/number_theory/prime_counting.hpp`;
- `tests/test_prime_counting_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- `docs/scope_recovery_prime_counting.md`.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, minimum-cycle-basis, exact-treewidth, or temporary-file
surface is changed.
