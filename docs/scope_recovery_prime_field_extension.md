# Scope recovery: prime-field extension arithmetic

## Coverage decision

Fresh live coverage after Earley general-CFG recognition found no extension-field,
Galois-field, or quotient-field arithmetic surface in code, pull-request history,
or active branches. The sealed prime-field polynomial stack repeatedly lists
extension fields as an explicit non-claim. This slice therefore composes existing
prime-field polynomial capabilities into a genuinely new algebraic object rather
than adding another factorization variant.

## Production contract

`PrimeFieldExtension(p, m)` represents the quotient field `F_p[x] / (m)`.

- `p` must be prime.
- `m` must be a canonical, monic, positive-degree irreducible polynomial.
- field elements are low-to-high coefficient vectors for the unique representative
  of degree strictly less than `deg(m)`; trailing zeros are accepted on input and
  every returned value is trimmed.
- coefficients must already be canonical residues in `[0,p)`.
- zero is the empty vector and one is `{1}`.
- addition, subtraction, multiplication, division, inverse, and nonnegative
  integer power return canonical representatives.
- inverse/division by zero throws.
- arbitrary polynomial representatives of degree at least `deg(m)` are rejected
  rather than silently reduced at the public boundary.

The constructor reuses the sealed deterministic distinct-degree factorization
(DDF) implementation. A monic degree-`k` modulus is accepted only when DDF returns
exactly one factor group, that group has irreducible degree `k`, and its factor is
exactly the modulus. Repeated/reducible inputs therefore fail closed.

## Correctness obligation

For irreducible `m`, the ideal `(m)` is maximal in `F_p[x]`, so
`F_p[x]/(m)` is a field and every residue class has one representative of degree
less than `deg(m)`.

Addition/subtraction operate coefficient-wise. Multiplication computes an ordinary
prime-field polynomial product and takes the exact remainder modulo `m`, which is
the unique quotient-field representative. For every nonzero element `a`,
irreducibility implies `gcd(a,m)=1`; the sealed polynomial extended Euclidean
algorithm returns `s*a + t*m = 1`, so `s mod m` is the multiplicative inverse.
Binary exponentiation composes the same field multiplication.

These quotient-ring / maximal-ideal facts and the existing DDF/Euclid theorems are
proof obligations. Finite tests are implementation evidence, not proofs of the
field theorem.

## Independent verification

Focused final-candidate execution passed under strict GCC, strict Clang, and an
actual GCC ASan+UBSan build.

The primary small-field oracle is deliberately independent of production DDF and
polynomial Euclid:

- 480 generated monic polynomials over `F_2` / `F_3`, degrees 1..4, are classified
  for irreducibility by exhaustive enumeration of every monic divisor through half
  the degree; constructor acceptance must match exactly.
- all elements of `GF(2^3)`, `GF(3^2)`, and `GF(5^2)` are exhaustively compared
  pairwise for addition, subtraction, and multiplication against direct test-only
  coefficient convolution plus monic long reduction.
- every nonzero element's inverse is found independently by exhaustive enumeration
  of the finite field and compared exactly with production Bézout inversion.
- 1,000 fixed-seed field-law / power checks compare production exponentiation with
  repeated independent multiplication.

Deterministic regressions cover `x^3+x+1` over `F_2`, `x^2+2` over `F_5`, malformed
prime/modulus/element contracts, zero inversion/division, trailing-zero canonical
inputs, and degree-one arithmetic at the full-width prime
`18446744073709551557`.

## Complexity and non-claims

Let `k = deg(m)`. Construction inherits the sealed deterministic DDF baseline.
Addition/subtraction are linear in `k`; direct multiply-and-reduce uses quadratic
coefficient work and overflow-safe scalar modular multiplication. Inversion
inherits the direct polynomial extended-Euclid baseline, and exponentiation uses
`O(log e)` field multiplications for exponent `e`. No tighter asymptotic claim is
imported from a different polynomial-arithmetic implementation.

This slice does not claim extension-field polynomial factorization, primitive-element discovery, discrete logarithms, normal-basis conversion, trace/norm APIs,
cryptographic constant-time behavior, subquadratic polynomial arithmetic, or
arbitrary-precision coefficients.

## Scope

This recovery slice changes exactly four paths: one header-only production API,
one repo-native test header, one include line in `tests/test_main.cpp`, and this
proof/boundary document. It does not touch CMake, README, historical ROADMAP,
recovery authority, workflows, benchmarks, frozen compiler/backend surfaces, or
occupied recovery branches.
