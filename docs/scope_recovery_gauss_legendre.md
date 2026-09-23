# Scope recovery: bounded Gauss-Legendre quadrature

## Coverage decision

After the hierarchical packed-memory-array slice merged as
`main@a086c84b4d8bd15594d1d08e0b5ca3c1b031b049`, a fresh live audit found
zero open pull requests and zero open issues.

The repository contains several numerical linear-algebra and root-finding
surfaces, but the live code, branch, and pull-request audit found no numerical
quadrature, Gauss-Legendre, Gauss-Kronrod, Simpson, or Legendre-rule
implementation.

A parallel `scope-recovery-rans-byte-coding` branch was explicitly detected
during frontier selection, so entropy-coding work was not touched.

This slice adds a different proof model: roots of orthogonal polynomials,
closed-form quadrature weights, and polynomial moment exactness.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; the frozen historical
compiler/backend frontier is not resumed.

## Public contract

`gauss_legendre_rule(order)` constructs a deterministic Gauss-Legendre rule
on `[-1,1]`.

The supported order is bounded:

- `order == 0` is rejected;
- `1 <= order <= 256` is supported;
- larger orders are rejected rather than silently extending the numerical
  guarantee beyond the tested contract.

The returned rule contains:

- strictly increasing nodes in `(-1,1)`;
- one positive weight per node;
- symmetric nodes and weights.

`gauss_legendre_integrate(rule, lower, upper, f)` applies a supplied rule to
a finite interval.

- finite reversed bounds are supported and reverse the sign;
- equal bounds return zero without invoking the integrand;
- malformed rule shape, non-finite/out-of-range nodes, non-positive/non-finite
  weights, or unsorted nodes are rejected;
- non-finite bounds are rejected;
- a non-finite integrand value is rejected;
- a non-finite final integral is rejected.

A convenience overload accepts an order and constructs the rule internally.

## Legendre recurrence

Production evaluates `P_n(x)` and `P_{n-1}(x)` using the three-term
Legendre recurrence:

`P_0(x)=1`

`P_1(x)=x`

`P_k(x)=((2k-1)xP_{k-1}(x)-(k-1)P_{k-2}(x))/k`.

At an interior point, production obtains the derivative from

`P'_n(x)=n(P_{n-1}(x)-xP_n(x))/(1-x^2)`.

No symbolic polynomial coefficient expansion is required.

## Root construction

Only half of the roots are solved explicitly because Legendre roots are
symmetric.

For root index `i`, production starts Newton iteration from the classical
cosine approximation

`cos(pi(4i+3)/(4n+2))`.

Each iteration evaluates `P_n`, `P_{n-1}`, and `P'_n`, then performs the
Newton update.

Safety boundaries:

- Newton is capped at 64 iterations;
- a zero/non-finite derivative is rejected;
- an iterate leaving the open unit interval is rejected;
- non-convergence is rejected;
- odd-order center roots are set exactly to zero from symmetry.

The stopping rule is scaled by `long double` machine epsilon.

## Weight construction

For a converged root `x_i`, production uses

`w_i = 2 / ((1-x_i^2)(P'_n(x_i))^2)`.

The mirrored root receives the identical weight.

Every generated weight is required to be finite and positive, and the final
node vector is checked to be strictly increasing.

## Polynomial exactness boundary

In exact arithmetic, the order-`n` Gauss-Legendre rule integrates every
polynomial of degree at most `2n-1` exactly on `[-1,1]`.

This implementation does **not** claim bit-exact floating-point integration.
Nodes and weights are approximated with `long double`, so tests verify the
theoretical moment identities within explicit numerical tolerances.

The interval transform uses

`x = midpoint + half_width * node`

and multiplies the weighted sum by `half_width`.

Midpoint and half-width are formed from halved endpoints to avoid the avoidable
overflow surfaces in direct `(a+b)/2` and `(b-a)/2` formulas. Node mapping
uses `std::fma`.

## Verification

Committed tests use several independent evidence surfaces.

### Closed-form low orders

Order 1 is checked against:

- node `0`;
- weight `2`.

Order 2 is checked against:

- nodes `±1/sqrt(3)`;
- weights `1,1`.

Order 3 is checked against:

- nodes `0, ±sqrt(3/5)`;
- weights `8/9, 5/9, 5/9`.

These checks do not derive expectations from the production Newton loop.

### Moment identities

For orders `1..16`, every monomial degree `0..2n-1` is integrated directly
from the returned nodes and weights.

The expected `[-1,1]` moment is independently known:

- zero for odd degree;
- `2/(d+1)` for even degree.

### Interval transform

For orders `1..10`, monomials through degree `2n-1` are also integrated on
`[-2,3]` and compared against the closed-form antiderivative.

### Structural and boundary evidence

Tests additionally cover:

- node ordering;
- node/weight symmetry;
- positive weights;
- total weight `2`;
- malformed external rules;
- invalid orders and infinite bounds;
- reversed bounds;
- equal bounds without integrand invocation;
- non-finite integrand output;
- the maximum supported order `256`.

### Non-polynomial evidence

The smooth integral

`integral_-1^1 exp(x) dx = e - e^{-1}`

is evaluated at orders 4, 8, and 16. Tests require decreasing error and a tight
final approximation.

A supplementary model-level calculation also exercised the same bounded
root/weight construction through order 256 and observed stable normalization.
That supporting calculation is not repository compiler/sanitizer evidence.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head remain the integration gate.

## Complexity boundary

Let `n` be the requested quadrature order, with `n <= 256`.

Production solves approximately half the roots.

Each Newton step evaluates the order-`n` recurrence in `O(n)`, and Newton is
explicitly capped at 64 iterations. Therefore rule construction has a
conservative `O(n^2)` bound under the fixed iteration cap.

Storage for a rule is `O(n)`.

Applying a precomputed rule to one interval performs exactly one integrand
evaluation per node and uses `O(n)` time with `O(1)` additional working
storage beyond the rule.

## Non-claims

This slice does not claim:

- arbitrary-precision quadrature;
- bit-exact floating-point polynomial integration;
- adaptive integration;
- Gauss-Kronrod error estimation;
- endpoint singularity handling;
- infinite-interval transforms;
- oscillatory-specialized rules;
- optimal root-finding iteration counts;
- benchmark-backed speedups;
- support beyond order 256.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/numerical/gauss_legendre.hpp`;
- `tests/test_gauss_legendre_cases.hpp`;
- `docs/scope_recovery_gauss_legendre.md`.

Pre-PR hardening includes:

- explicit `<utility>` support for `std::pair` and `std::forward`;
- validation of externally supplied nodes and weights;
- malformed-rule regression coverage;
- closed-form order-3 verification;
- maximum-supported-order structural/normalization coverage.

No CMake, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Base: `a086c84b4d8bd15594d1d08e0b5ca3c1b031b049`.
