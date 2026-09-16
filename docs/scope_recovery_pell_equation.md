# Scope recovery: bounded-exact Pell equation via periodic continued fractions

## Coverage decision

A fresh live audit at `main@601a4f1c88a505d90a517d9003fa773b8c7bbf8a`
found no Pell-equation, quadratic-surd, Chakravala, or square-root
continued-fraction production surface in default-branch code, pull-request
history, or branch names. The current prospective authority remains
`docs/scope_recovery_after_phase69.md`; the frozen Phase-45--69 compiler/backend
surface and historically stale ROADMAP are intentionally untouched.

This slice deliberately changes proof model after the recent deterministic
rake-compress tree contraction and verification-infrastructure hardening. It
adds an exact number-theoretic continued-fraction recurrence rather than an
adjacent tree, kinetic, retroactive, or test-infrastructure variant.

## Production contract

`fundamental_pell_solution(D)` returns the fundamental positive solution
`(x,y)`, with `y>0`, of

`x^2 - D*y^2 = 1`.

The bounded public domain is:

- `2 <= D <= 1,000,000,000`;
- `D` must be nonsquare;
- the mathematical fundamental `x` and `y` must both fit in `uint64_t`.

Invalid/square `D` throws `std::invalid_argument`. If the fundamental solution
cannot be represented by the public `uint64_t` result, construction fails
closed with `std::overflow_error`; no truncation, modular wrap, or arbitrary-
precision claim is made.

The result also exposes the period length of the simple continued fraction of
`sqrt(D)` and the zero-based convergent index used for the fundamental solution.

## Algorithm and proof boundary

For nonsquare `D`, the standard square-root continued-fraction recurrence is

`m' = d*a - m`

`d' = (D - m'^2)/d`

`a' = floor((a0 + m')/d')`, where `a0=floor(sqrt(D))`.

The period ends at the first coefficient `2*a0`. If the period length is `L`,
Lagrange's theorem and the classical Pell/continued-fraction theorem give the
fundamental solution at convergent

- `L-1` when `L` is even;
- `2L-1` when `L` is odd.

Convergents are evaluated with the ordinary positive recurrence
`p_i=a_i p_{i-1}+p_{i-2}`, `q_i=a_i q_{i-1}+q_{i-2}` and checked before every
multiply-add. Because all coefficients and convergent terms are non-negative,
an overflow before the target convergent implies the target itself is outside
the public `uint64_t` representation.

The universal Pell/continued-fraction theorem is the mathematical proof
obligation. Finite tests are implementation evidence, not a replacement proof.

## Exact arithmetic boundary

Production uses no floating point and no non-standard wider integer. The input
bound `D <= 1e9` keeps every square-root continued-fraction state comfortably
inside `uint64_t`: `a0 <= 31622`, the periodic coefficients are at most
`2*a0`, and the recurrence products are far below the unsigned 64-bit limit.
Convergent growth is the only intentionally unbounded quantity, and every
multiply-add is checked before evaluation.

## Verification

Focused exact candidate bytes passed before upload under:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with leak detection/fail-fast: 4/4.

Deterministic evidence covers invalid/square inputs, even/odd period parity,
known fundamental solutions for `D=2,3,5,6,7,13,61`, and the famous `D=661`
case whose fundamental solution exceeds `uint64_t` and must throw rather than
wrap.

The primary independent oracle covers every nonsquare `D` in `[2,50]`. It does
not generate continued fractions: it scans positive `y`, evaluates
`D*y^2+1`, performs an integer square-root test, and takes the first exact
square. Production `x,y` must match that brute-force minimum exactly. The
bounded oracle arithmetic is independently representable in `uint64_t`.

## Complexity and non-claims

Let `L` be the period length of `sqrt(D)`. Period discovery plus the required
convergent evaluation uses `O(L)` fixed-word recurrence steps and `O(L)` storage
for the explicit period vector. The implementation does not claim a stronger
bit-complexity bound, arbitrary-precision Pell solving, generalized Pell
`x^2-Dy^2=N`, Chakravala, class-group computation, regulator computation, or
cryptographic relevance.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/number_theory/pell_equation.hpp`;
- `tests/test_pell_equation_cases.hpp` (automatically isolated into its own test
  translation unit by the sealed #351 recovery-test harness);
- `docs/scope_recovery_pell_equation.md`.

No CMake, `tests/test_main.cpp`, README, historical ROADMAP, recovery authority,
workflow, benchmark, frozen compiler/backend, or temporary-file churn is
required.
