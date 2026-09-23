# Scope recovery: bounded-exact integer characteristic polynomial

## Coverage decision

After minimum strong-connectivity augmentation merged as
`main@ed2e735da31c8f58c1a61a32a47fa8c3f16d4139`, a fresh live audit found
zero open pull requests and zero open issues.

No default-branch implementation, branch, or prior pull request was found for a
matrix characteristic-polynomial routine or Faddeev-LeVerrier recurrence.

The repository already contains exact bounded integer determinant machinery,
prime-field polynomial algorithms, dense floating-point factorizations, and
iterative linear solvers. This slice is different: it constructs the complete
monic polynomial `det(lambda I - A)` for a signed-64-bit integer matrix.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; frozen historical compiler/backend
work is untouched.

## Public contract

`integer_characteristic_polynomial(matrix)` accepts a square
`vector<vector<int64_t>>`.

For an `n x n` matrix `A`, it returns `n+1` coefficients in descending
degree order:

`[1, c1, ..., cn]`

representing

`lambda^n + c1 lambda^(n-1) + ... + cn = det(lambda I - A)`.

The empty `0 x 0` matrix returns `[1]`, consistent with the empty
determinant.

A nonsquare matrix is rejected with `std::invalid_argument`.

## Faddeev-LeVerrier recurrence

Production uses the classical recurrence with `B0 = I`.

For `k = 1..n`:

1. compute `P_k = A B_(k-1)`;
2. compute `c_k = -trace(P_k) / k`;
3. when another iteration remains, set
   `B_k = P_k + c_k I`.

The returned polynomial is

`lambda^n + c1 lambda^(n-1) + ... + cn`.

For an integer matrix, every `c_k` is an integer coefficient of the
characteristic polynomial, so the recurrence division by `k` is exact
mathematically. Production still checks the remainder explicitly; a non-exact
division is treated as an internal invariant failure rather than silently
truncating.

The final diagonal update after `c_n` is intentionally skipped because
`B_n` is never consumed.

## Bounded-exact arithmetic

This slice deliberately follows the repository's bounded-exact integer style
rather than introducing arbitrary-precision dependencies.

Every production operation that can change a recurrence value is checked:

- signed-64 multiplication in matrix products;
- signed-64 addition in matrix-product accumulation;
- signed-64 trace accumulation;
- signed-64 coefficient negation;
- signed-64 diagonal update;
- exact positive division by the recurrence index.

If any required intermediate cannot be represented by `int64_t`,
`std::overflow_error` is thrown instead of wrapping.

The matrix dimension is also checked before `n+1` coefficient allocation and
before conversion of the recurrence index to `int64_t`.

This contract is stricter than "the final coefficients fit in int64_t": a
matrix may be rejected if a Faddeev-LeVerrier intermediate overflows even when
some alternative algorithm could still produce representable final
coefficients.

## Independent determinant oracle

Committed randomized verification does not reuse matrix traces, key
coefficients, or the production recurrence.

For small matrices, the test oracle evaluates the determinant directly from the
Leibniz permutation formula.

For each tested integer `lambda`, it constructs `lambda I - A` and checks:

`evaluate(returned_polynomial, lambda) == det_Leibniz(lambda I - A)`.

The random committed cases use matrices of order `0..4` with entries in
`[-3,3]` and test every integer `lambda` from `-4` through `4`. These
bounds keep the independent oracle safely inside signed-64 arithmetic.

Because both sides are polynomials of degree at most `n <= 4`, agreement at
nine distinct points is stronger than the minimum number of interpolation
points needed to identify the polynomial.

A supplementary model-level differential pass checked 5,000 additional random
matrices at the same nine lambda values, for 45,000 determinant-identity
comparisons, and found zero mismatch. This is supporting evidence only;
repository compiler/sanitizer CI remains the integration gate.

## Deterministic coverage

Committed tests include:

- empty matrix;
- singleton;
- known dense `2 x 2` matrix;
- diagonal `3 x 3` matrix;
- nilpotent upper-shift matrix;
- several structural examples verified by the determinant oracle;
- nonsquare rejection;
- coefficient-negation overflow via `INT64_MIN`;
- recurrence arithmetic overflow via extreme matrix values;
- 900 fixed-seed random matrices, each checked at nine determinant points;
- the first coefficient identity `c1 = -trace(A)` on every nonempty random
  case.

The lambda-zero oracle point also checks the constant-term determinant identity
`cn = (-1)^n det(A)` implicitly.

## Complexity boundary

There are `n` recurrence steps. Each step performs one dense `n x n`
matrix multiplication in `O(n^3)` time.

Therefore the conservative production bound is:

- time: `O(n^4)`;
- auxiliary matrix storage: `O(n^2)`;
- returned coefficient storage: `O(n)`.

No faster characteristic-polynomial complexity is claimed.

The test-only Leibniz oracle is factorial-time and intentionally restricted to
small matrices.

## Non-claims

This slice does not claim:

- arbitrary-precision integer coefficients;
- acceptance whenever only the final coefficients fit in int64;
- floating-point eigenvalue computation;
- eigenvectors;
- minimal polynomial;
- Hessenberg/Berkowitz/Keller-Gehrig asymptotics;
- prime-field or composite-modulus characteristic polynomials;
- benchmark-backed performance.

No cryptographic, probabilistic, or numerical-conditioning claim is involved.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/linear_algebra/characteristic_polynomial.hpp`;
- `tests/test_characteristic_polynomial_cases.hpp`;
- `docs/scope_recovery_characteristic_polynomial.md`.

Pre-PR hardening added an explicit matrix-dimension guard before coefficient
index arithmetic.

No Bareiss implementation, CMake, test-main, README, ROADMAP, workflow,
benchmark, frozen compiler/backend, or temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base at slice creation:
`ed2e735da31c8f58c1a61a32a47fa8c3f16d4139`.
