# Scope recovery: composite-modulus linear systems via Smith normal form

## Coverage decision

After bounded-exact Smith normal form reached merged `main`, a fresh live code / PR-history / branch audit found no general linear-congruence-system solver. The repository already has prime-field Gauss-Jordan elimination, but field elimination does not model zero divisors or non-invertible pivots over composite moduli. CRT reconstruction exists only as a specialized two-congruence step inside exact convolution.

This slice therefore uses the new Smith decomposition as a cross-layer exact-integer reduction rather than immediately adding another canonical-form variant. Occupied recovery surfaces and the prospectively frozen Phase-45--69 compiler/backend work remain untouched.

## Production contract

`solve_modular_linear_system(A, b, m)` solves

`A x == b (mod m)`

for a rectangular signed-64 integer matrix `A`, signed-64 right-hand side `b`, and any non-zero `uint64_t` modulus `m`, including composite and full-width moduli.

- malformed RHS length and modulus zero are rejected;
- ragged matrices are rejected by the sealed Smith surface;
- success returns one deterministic canonical witness `x` with every coordinate in `[0,m)`;
- the result also exposes Smith coordinates, transformed RHS residues, invariant factors, diagonal gcds, and rank so the reduction can be replayed;
- unsatisfiable systems return `std::nullopt`;
- free Smith coordinates are chosen as zero; each constrained Smith coordinate uses the least non-negative solution modulo its reduced modulus;
- the returned witness is deterministic for the sealed Smith decomposition, but is not claimed to be lexicographically minimum among all original-coordinate solutions.

The API deliberately inherits Smith normal form's bounded-exact `int64_t` transform contract. Constructing `U`, `D`, or `V` can throw `std::overflow_error` even when the congruence system has a mathematical solution. No arbitrary-precision completion claim is made.

## Reduction and proof obligation

For the sealed decomposition

`U A V = D`,

set `x = V y` and multiply the congruence by unimodular `U`:

`D y == U b (mod m)`.

For every non-zero Smith diagonal `d_i`, the scalar congruence

`d_i y_i == c_i (mod m)`

is solvable exactly when `g_i = gcd(d_i,m)` divides the canonical residue `c_i`. Dividing by `g_i` gives a coprime equation modulo `m/g_i`; a private overflow-safe Euclidean coefficient recurrence computes the inverse of `d_i/g_i` without signed Bézout multiplication. Zero rows require their transformed RHS residue to be zero. Remaining Smith coordinates are free and are set to zero.

Finally `x = V y (mod m)`. Because `U` and `V` are unimodular over the integers, these transformations preserve the modular solution set bijectively. This theorem/reduction is the correctness obligation; finite tests are implementation evidence rather than a substitute for it.

Signed matrix entries and RHS values are converted to residues without negating `INT64_MIN`. Matrix-vector products are accumulated modulo `m` through the sealed overflow-safe `multiply_mod` plus an overflow-free modular addition. The private inverse tracks its Bézout coefficient only modulo the reduced modulus, avoiding unrepresentable signed intermediate coefficients.

## Independent verification

Primary randomized verification is structurally independent of Smith reduction. For 700 fixed-seed systems with one to three equations, one to three variables, coefficients/RHS in `[-5,5]`, and moduli `1..7`, the test enumerates every assignment in `[0,m)^n` and decides existence by direct equation evaluation. Production existence must match exactly; every successful witness is replayed against the original system.

Additional evidence covers:

- solvable and unsatisfiable non-unit pivots over composite moduli (`2x == 2/1 mod 4`);
- coupled and underdetermined systems;
- inconsistent zero rows and zero-variable systems;
- modulus one;
- negative RHS semantics;
- full-`uint64_t` modulus, including the inverse of two modulo `2^64-1` whose canonical solution exceeds `INT64_MAX`;
- ragged/RHS/modulus validation and inherited Smith bounded-overflow rejection;
- repeated-call equality and replay of exposed Smith-coordinate certificates.

The exhaustive oracle does not call Smith normal form, diagonal reduction, or the production inverse recurrence. Sealed `multiply_mod` is reused only for full-width arithmetic replay where ordinary machine multiplication would overflow.

## Complexity and non-claims

The solver inherits the bounded-exact Smith-normal-form construction cost. After Smith decomposition, applying `U` to `b` and `V` to `y` uses `O(r^2 + c^2)` modular multiply/add slots for `r` rows and `c` columns. Each of at most `min(r,c)` scalar congruences performs a Euclidean inverse; with the repository's first-principles `multiply_mod`, the direct code-local bit-work includes the corresponding logarithmic modular-arithmetic factors.

No arbitrary-precision Smith completion, count/enumeration of all solutions, lexicographically minimum original-coordinate solution, Chinese-remainder decomposition, sparse solver, fast normal-form algorithm, or prime-field-only simplification is claimed.

## Scope

The intended recovery slice is header-only orchestration plus repo-native tests and this proof document. It reuses sealed Smith and modular arithmetic and does not change README, historical ROADMAP, the recovery-authority document, workflows, benchmarks, or frozen compiler/backend surfaces.
