# Scope recovery: prime-field polynomial Euclid / Bézout GCD

## Coverage decision

Fresh live-state, PR-history, code, and branch searches at `main@a678a6c5b634213fabf052a64212c2f0442f50fc` found no polynomial-GCD / polynomial-Euclid capability and no occupied recovery branch on this surface. Existing NTT/convolution/FPS inversion, finite-field Gaussian elimination, Berlekamp-Massey, and scalar modular arithmetic cover different algebraic state and proof obligations. The stale historical compiler/backend ROADMAP is not used as prospective authority; this slice follows `docs/scope_recovery_after_phase69.md` plus the live coverage audit.

## Production contract

`algorithms::polynomials` exposes:

- `polynomial_divide_mod(dividend, divisor, prime)` returning exact quotient and remainder in `F_p[x]`;
- `polynomial_gcd_mod(a,b,prime)` returning the unique monic GCD;
- `polynomial_extended_gcd_mod(a,b,prime)` additionally returning `s,t` with `s*a + t*b = gcd`.

Polynomials use low-to-high `uint64_t` coefficients. Every coefficient must already be a canonical residue `< prime`; trailing zero coefficients are accepted and normalized away. The zero polynomial is the empty vector. The modulus must be prime, and division by the zero polynomial is rejected.

The implementation reuses the sealed full-width deterministic primality test plus overflow-safe `multiply_mod` / `power_mod`; it does not introduce a second modular multiplication or inverse engine.

## Invariants and correctness boundary

For nonzero divisor `d`, long division repeatedly cancels the current highest remainder term using `lead(d)^(-1)`. After every cancellation the remainder degree strictly decreases; termination therefore gives the unique `q,r` satisfying

`dividend = q*d + r`, with `r = 0` or `deg(r) < deg(d)`.

Extended Euclid maintains, at every iteration,

`old_r = old_s*a + old_t*b`

and

`r = s*a + t*b`.

Replacing `(old_r,r)` with `(r, old_r - q*r)` preserves both identities while strictly decreasing the nonzero remainder degree. When the remainder reaches zero, `old_r` generates the ideal `(a,b)`. Scaling it and both Bézout coefficients by the inverse leading coefficient makes the public nonzero GCD monic without changing the identity. `gcd(0,0)` is the zero polynomial; the deterministic returned coefficients are `s=1,t=0`.

These are the polynomial Euclidean-domain / Bézout proof obligations. Random testing is implementation evidence, not a proof of the theorem.

## Verification

Focused repo-native verification passed before upload under:

- GCC C++20 repository strict warnings-as-errors;
- Clang C++20 repository strict warnings-as-errors;
- actual GCC ASan+UBSan with leak detection.

The four focused groups cover:

- composite-modulus, noncanonical-residue, and zero-divisor rejection;
- trailing-zero normalization and known exact division/GCD/Bézout examples;
- a full-width prime `18446744073709551557` regression exercising overflow-safe field multiplication;
- 300 fixed-seed `F_5` divisions generated independently as `d*q+r`, requiring exact recovery of `q,r` and the remainder-degree bound;
- 350 fixed-seed `F_3` polynomial pairs of degree at most four, compared with an independent exhaustive oracle.

The primary GCD oracle does **not** use Euclid or production division: it enumerates monic candidate divisors from highest degree downward and independently enumerates every possible quotient to decide divisibility by direct small-field convolution. Every extended-GCD result is also replayed through an independent small-field polynomial multiply/add implementation.

## Complexity and non-claims

Let `d` be the maximum input degree. Direct long division uses quadratic coefficient work in the degree scale, and extended Euclid can perform `O(d)` divisions plus direct Bézout polynomial products. With the repository's bit-doubling `multiply_mod`, this baseline conservatively claims `O(d^3 log p)` time and `O(d^2)` transient polynomial storage across the live vectors. The exact practical bound depends on the degree sequence.

This slice does not claim fast half-GCD, subquadratic division, polynomial factorization, irreducibility testing, multipoint evaluation/interpolation, arbitrary coefficient rings, extension fields, or arbitrary-precision coefficients.

## Scope

The intended repository change is exactly four files: one new header-only production implementation, one dedicated native test translation unit, one CMake test-registration line, and this proof document. README, stale ROADMAP, recovery authority, workflows, benchmarks, frozen compiler/backend code, minimum-cycle-basis, graph-isomorphism, and temporary files remain untouched.
