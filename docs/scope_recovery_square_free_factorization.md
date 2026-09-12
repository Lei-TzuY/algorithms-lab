# Scope recovery: prime-field polynomial square-free factorization

## Coverage decision

A fresh live-state/code/branch audit at `main@e61fb3678dd84aec877e76690763dafe030c51a7` found no square-free factorization capability and no occupied `scope-recovery-square-free-factorization` branch. The immediately preceding recovery slice added exact prime-field polynomial division, monic GCD, and extended Bézout witnesses, so square-free decomposition is a direct algebraic continuation of that sealed surface rather than an unrelated breadth expansion.

## Production contract

`algorithms::polynomials` adds:

- `polynomial_derivative_mod(f, p)` for formal differentiation in `F_p[x]`;
- `polynomial_square_free_factorization_mod(f, p)`, returning the nonzero leading unit plus monic square-free factors grouped by exact positive multiplicity.

Polynomials keep the existing low-to-high canonical `uint64_t` coefficient representation. The modulus must be prime and every coefficient must already be `< p`. Trailing zero coefficients are normalized. The zero polynomial is rejected because its square-free factorization is not defined. A nonzero constant returns only its scalar unit and no nonconstant factors.

The result is **not** an irreducible factorization. One returned factor may itself be a product of several distinct irreducibles that occur with the same multiplicity.

## Invariants and characteristic-p boundary

After extracting the leading coefficient, production works with a monic polynomial `f`. For nonzero derivative `f'`, the Yun-style loop maintains

`f = w * c`, where `c = gcd(f, f')` initially,

and repeatedly computes `y = gcd(w,c)` and `z = w/y`. The nontrivial `z` layer contains exactly the irreducible factors whose current multiplicity is the loop index. Exact polynomial division is reused from the preceding prime-field Euclid slice; any nonzero remainder is treated as an internal invariant failure rather than silently accepted.

Characteristic `p` requires an additional branch: a nonconstant polynomial can have zero derivative when every nonzero exponent is divisible by `p`. The leftover repeated part is then an exact `p`-th power. Production takes its exact `p`-th root by dividing exponents by `p`; coefficients are unchanged because Frobenius is the identity on `F_p`. The root is decomposed recursively and every nested multiplicity is multiplied by `p` with overflow checking.

Final layers are sorted by multiplicity. Layers with the same multiplicity are multiplied together, yielding one monic square-free product per multiplicity. Therefore the public result satisfies

`input = unit * product_i factor_i ^ multiplicity_i`,

with strictly increasing multiplicities, monic nonconstant square-free factors, and pairwise-coprime factor layers.

These are finite-field algebra proof obligations. Random/exhaustive tests are implementation evidence, not a proof of Yun's theorem or Frobenius theory.

## Verification

Focused repo-native verification passed before upload under:

- GCC C++20 repository strict warnings-as-errors;
- Clang C++20 repository strict warnings-as-errors;
- actual GCC ASan+UBSan with leak detection / halt-on-error.

The four focused groups cover:

- formal-derivative semantics plus zero-polynomial, composite-modulus, and noncanonical-residue rejection;
- a non-monic `F_5` example `3 (x+1)^2 (x+2)^3`, checking unit and exact multiplicity layers;
- characteristic-two and characteristic-three regressions requiring recursive `p`-th-root handling, including multiplicities `4`, `3`, and `6`;
- nonzero constants;
- 500 fixed-seed nonzero `F_3` polynomials of degree at most seven.

The randomized verifier is independent of production GCD/decomposition recurrence. It reconstructs the input with a separate small-field polynomial multiply/power implementation, exhaustively enumerates monic candidates `q` to reject any returned factor divisible by `q^2`, and separately enumerates common monic divisors to verify pairwise coprimality between multiplicity layers.

## Complexity and non-claims

This first-principles baseline reuses direct polynomial long division/GCD from the preceding slice. With degree `d`, the implementation intentionally keeps the same conservative polynomial-Euclid complexity regime; recursive characteristic-`p` roots strictly reduce degree by a factor of `p`. No subquadratic polynomial arithmetic bound is claimed.

This slice does not claim irreducible factorization, distinct-degree factorization, equal-degree factorization, Berlekamp/Cantor-Zassenhaus, irreducibility testing, extension-field factorization, arbitrary coefficient rings, or fast half-GCD.

## Scope

The intended repository change is exactly four files: one new header-only production implementation, one dedicated native test translation unit, one CMake test-registration line, and this proof document. README, historical ROADMAP, workflows, benchmarks, frozen compiler/backend code, unrelated recovery branches, and temporary files remain untouched.
