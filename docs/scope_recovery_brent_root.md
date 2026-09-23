# Scope recovery: safeguarded Brent-Dekker root solving

## Coverage decision

After exact Minimum Linear Arrangement merged as
`main@ce9582cc03b44bf3482e5a193ba0c991dc5ed1bd`, a fresh live audit found
zero open pull requests and zero open issues.

The repository already contains substantial dense numerical linear algebra,
iterative Krylov solvers, FFT, and matrix factorization surfaces, but no
one-dimensional bracketed root solver, bisection/Newton/secant root API, Brent
implementation, branch, or implementation PR.

A branch named `scope-recovery-minimum-bisection` exists, but it is a graph
minimum-bisection surface and is unrelated to numerical root finding.

This slice therefore introduces a new numerical-analysis capability rather than
an alternate implementation of an existing solver.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; frozen historical compiler/backend
work remains untouched.

## Public contract

`brent_root(function, lower, upper, absolute_tolerance,
relative_tolerance, max_iterations)` solves `f(x)=0` inside a finite
strictly ordered bracket.

The semantic precondition is that `f` is continuous on the bracket.
Continuity cannot be established from finitely many evaluations and is not
claimed to be validated.

Input requirements:

- `lower < upper`;
- both endpoints are finite;
- `absolute_tolerance > 0` and finite;
- `relative_tolerance >= 0` and finite;
- `max_iterations > 0`;
- either endpoint is an exact root, or endpoint function values have opposite
  signs.

Callback evaluations must be finite. A non-finite callback result is rejected
with `std::overflow_error`.

Invalid static input contracts are rejected with `std::invalid_argument`.

## Result model

`BrentRootResult` reports:

- `converged` or `iteration_limit`;
- the current best root estimate;
- its function value;
- the current sign-bracket bounds;
- completed iteration count;
- total function-evaluation count.

An endpoint root converges immediately with zero iterations.

Reaching the iteration budget does not silently claim convergence: the status is
`iteration_limit`, while the returned bracket remains a valid sign bracket.

## Bracket invariant

After endpoint validation, production keeps two points `a` and `b` whose
function values have opposite signs.

The endpoint `b` is normalized to have no larger residual magnitude than
`a`.

For a candidate `s`:

- if `f(s)=0`, the solver terminates exactly;
- if `f(a)` and `f(s)` have opposite signs, the new bracket is `[a,s]`;
- otherwise the new bracket is `[s,b]`.

After the bracket update, endpoints may be swapped only to restore the
smaller-residual-at-`b` invariant. Swapping does not change the represented
interval or sign-bracket property.

No multiplication of endpoint function values is used to test sign changes, so
overflow-prone `f(a)*f(b)` sign tests are avoided.

## Interpolation hierarchy

At each nonterminal iteration production attempts:

1. inverse quadratic interpolation when `f(a)`, `f(b)`, and the historical
   value `f(c)` are pairwise distinct;
2. otherwise a secant interpolation when the current endpoint function values
   differ;
3. otherwise midpoint fallback.

Interpolation arithmetic is evaluated in `long double` where available. A
candidate is accepted only if it converts to a finite representable
`double`.

This wider intermediate representation is a robustness measure, not an
extended-precision accuracy claim. On implementations where `long double`
has the same range as `double`, an overflowed interpolation simply fails the
acceptance test and falls back to bisection.

## Brent safeguards

Even a finite interpolation candidate is rejected when any standard Brent
progress guard fails.

The candidate must lie strictly between the current best endpoint `b` and the
point one quarter of the way from `a` toward `b`.

It is also rejected when:

- after a midpoint step, its displacement from `b` is not less than half the
  previous `|b-c|` displacement;
- after an interpolation step, its displacement is not less than half the
  previous `|c-d|` displacement;
- the relevant historical displacement is already below the active tolerance.

A rejected interpolation candidate is replaced by
`std::midpoint(a,b)`, avoiding overflow-prone `(a+b)/2`.

Thus interpolation is opportunistic; maintaining the bracket takes priority.

## Tolerance arithmetic

The active stopping tolerance is

`absolute_tolerance + relative_tolerance * |b|`.

If that arithmetic becomes non-finite, production throws
`std::overflow_error` rather than treating an infinite tolerance as evidence
of convergence.

This boundary is regression-tested with finite extreme endpoints and a relative
tolerance large enough to overflow the tolerance expression.

## Termination

Convergence is reported when:

- the current best endpoint is an exact zero; or
- current bracket width is no greater than the active tolerance.

The finite iteration budget is an explicit execution bound. Exhausting it
returns `iteration_limit`; it does not throw and does not claim a converged
root.

The implementation makes at most two initial endpoint evaluations plus one new
function evaluation per executed iteration.

## Independent verification

The committed differential oracle is plain bisection. It does not use:

- inverse quadratic interpolation;
- secant interpolation;
- Brent history points;
- Brent acceptance guards;
- the production state machine.

For each reference problem, the oracle repeatedly uses
`std::midpoint` and direct sign tests to retain the half-bracket containing the
root.

Committed deterministic coverage includes:

- `sqrt(2)`;
- the fixed point of `cos(x)-x`;
- an exact lower-endpoint root;
- an exact upper-endpoint root;
- a flat odd-multiplicity root `(x-r)^3`;
- invalid/reversed/degenerate brackets;
- non-finite endpoints;
- invalid tolerances and iteration limits;
- same-sign endpoints;
- non-finite callback values;
- tolerance-arithmetic overflow;
- an intentionally one-iteration run that must return
  `iteration_limit` while preserving a valid sign bracket;
- 1,000 fixed-seed monotone cubic problems compared against the independent
  bisection reference.

The randomized family is

`f(x) = (x-r) ((x-r)^2 + q)`, with `q>0`.

It has exactly one real root `r` and a guaranteed sign-changing bracket.

A supplementary model-level stress pass exercised 100,000 additional instances
from the same family plus fixed known roots and found no convergence, residual,
or bracket mismatch. This is supporting evidence only; repository compiler and
sanitizer CI remain the integration gate.

## Complexity boundary

For an iteration budget `K`:

- function evaluations are at most `K+2`;
- auxiliary storage is `O(1)`;
- arithmetic work outside the callback is `O(K)`.

No guaranteed superlinear convergence rate is claimed. Brent interpolation can
accelerate smooth problems, but the hard safety property of this slice is the
maintained bracket plus midpoint fallback.

## Non-claims

This slice does not claim:

- validation of function continuity;
- detection of even-multiplicity roots without a sign change;
- derivative use or Newton convergence;
- multi-root isolation;
- complex roots;
- arbitrary precision;
- interval-arithmetic certification;
- a bound on callback conditioning;
- benchmark-backed superiority over bisection, Newton, or library solvers.

The method is a first-principles safeguarded bracketed solver with explicit
finite-precision failure semantics.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/numerical/brent_root.hpp`;
- `tests/test_brent_root_cases.hpp`;
- `docs/scope_recovery_brent_root.md`.

Pre-PR hardening includes an explicit regression that tolerance arithmetic
overflow must throw instead of falsely converging.

No CMake, test-main, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `ce9582cc03b44bf3482e5a193ba0c991dc5ed1bd`.
