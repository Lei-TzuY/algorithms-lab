# Scope recovery: prime-field distinct-degree factorization

## Capability

`polynomial_distinct_degree_factorization_mod(f, p)` adds a deterministic
finite-field factorization layer that was explicitly outside the earlier
square-free decomposition scope.

The input is a nonzero square-free polynomial over `F_p`. It may be non-monic;
the result preserves its leading unit and returns monic factors grouped by the
common degree of their irreducible constituents. Groups are emitted in strictly
increasing degree order. Zero input, non-prime moduli, non-canonical
coefficients, and non-square-free input are rejected.

This is distinct-degree factorization (DDF), not equal-degree splitting and not
a complete irreducible factorization.

## Correctness invariant

For a square-free polynomial `f` over `F_p`,

`gcd(f, x^(p^d) - x)`

is exactly the product of the monic irreducible factors of `f` whose degrees
divide `d`. Production removes every lower-degree group as soon as it is found.
Therefore, at iteration `d`, all factors whose degrees properly divide `d` have
already left the remaining polynomial, so the new gcd is exactly the degree-`d`
group.

The Frobenius residue is maintained incrementally: after iteration `d` it is
`x^(p^d)` modulo the current remaining factor. When a group is removed, reducing
that residue modulo the exact quotient preserves the congruence. The next
`p`-th power therefore advances to `x^(p^(d+1))` without recomputing from
scratch.

Once `d > deg(remaining)/2`, the remaining nonconstant polynomial cannot contain
two irreducible factors: every still-unremoved irreducible has degree at least
`d`. The final remainder is consequently one irreducible factor and forms its
own last degree group.

The implementation reuses the sealed formal derivative, polynomial GCD,
division, field multiplication, and modular exponentiation surfaces rather than
duplicating them.

## Verification

Focused pre-upload verification passes under GCC C++20 strict
warnings-as-errors, Clang C++20 strict warnings-as-errors, and actual GCC
ASan+UBSan.

Deterministic coverage includes malformed moduli/residues, zero input,
non-square-free rejection, constants, mixed degree-1/2/3 factors, multiple
irreducibles sharing one degree group, and a full-width uint64 prime with two
linear factors.

The primary randomized oracle is structurally independent of Frobenius/GCD.
For `p in {2,3,5}`, tests enumerate monic candidate polynomials directly,
classify irreducibility by naive divisibility against every possible lower
monic degree, construct 450 square-free products per field, group those known
irreducibles by degree, and compare the complete production result. Small-field
oracle division is direct coefficient elimination and never calls production
GCD or DDF logic.

## Complexity / non-claims

Let `n = deg(f)`. The direct vector baseline performs at most `O(n)` Frobenius
steps. Each step raises one residue to the `p`-th power by binary exponentiation
and reduces polynomial products by long division. Accounting for the
repository's overflow-safe scalar modular multiplication, a conservative bound
for this implementation is `O(n^3 log^2 p)` time and `O(n^2)` transient/result
storage.

No equal-degree factorization, Berlekamp nullspace splitting,
Cantor-Zassenhaus, complete irreducible factorization, extension-field,
subquadratic polynomial arithmetic, or arbitrary-precision claim is made.
