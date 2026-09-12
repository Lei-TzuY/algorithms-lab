# Scope recovery: prime-field equal-degree factorization

## Capability

This slice advances the sealed prime-field factorization chain by splitting one
validated distinct-degree group into its individual monic irreducible factors.
The public operation accepts a polynomial whose irreducible factors all have one
requested positive degree, an explicit replay seed, and a finite split-trial
budget. It preserves the original leading unit and returns canonical sorted
monic factors when the search completes.

Randomness affects only the search for a nontrivial divisor. Every accepted split
is an exact polynomial GCD followed by an exact quotient, so a completed result
is exact. Exhausting the caller's finite budget returns `complete=false` with no
partial factors; an unsplit composite group is never presented as irreducible.

## Split invariants

Input validation is delegated to the sealed distinct-degree factorization layer,
which also proves that the normalized input is square-free and consists of one
irreducible-degree group.

For odd prime `p`, a random residue `a` is split with the classical
Cantor-Zassenhaus quadratic-character test. The exponent
`(p^d-1)/2 = ((p-1)/2)(1+p+...+p^(d-1))` is evaluated through repeated Frobenius
powers, so `p^d` is never materialized in an integer type. A nontrivial
`gcd(f, a^((p^d-1)/2)-1)` partitions the irreducible factors.

For `p=2`, production uses the field trace
`a + a^2 + ... + a^(2^(d-1))`; a nontrivial GCD with that trace gives the split.
All worklist members remain products of distinct degree-`d` irreducibles, and a
member of total degree `d` is therefore irreducible.

The replay RNG is `std::mt19937_64`, but bounded residues use repository-defined
rejection sampling rather than `std::uniform_int_distribution`, avoiding an
implementation-defined bounded-mapping dependency.

## Verification

Focused candidate verification passes GCC and Clang C++20 strict warnings as
errors plus an actual GCC ASan+UBSan build. Deterministic cases cover malformed
field/degree/input contracts, non-square-free and mixed-degree rejection,
constant and already-irreducible zero-trial fast paths, explicit trial-budget
exhaustion, fixed-seed replay, characteristic-two trace splitting, and a
full-width uint64 prime linear-factor group.

The primary small-field oracle is independent of Cantor-Zassenhaus and Frobenius
splitting. It enumerates monic candidate polynomials, classifies irreducibility
by naive lower-degree divisibility, constructs 360 fixed-seed square-free
products over F2/F3/F5, and compares the complete returned factor multiset.

Finite randomized tests are implementation evidence, not a deterministic
termination theorem. `complete=false` remains a supported outcome for a finite
trial budget.

## Complexity and non-claims

For input degree `n`, target factor degree `d`, and `T` executed split trials, the
direct vector/arithmetic baseline conservatively claims
`O((T*d + 1) * n^3 * log^2(p))` time and `O(n^2)` transient/result storage. This
is intentionally loose and includes naive polynomial multiply/divide/GCD plus
the repository's overflow-safe scalar modular multiplication cost.

This slice does not claim deterministic polynomial-time EDF, Berlekamp nullspace
factorization, complete square-free-to-irreducible orchestration, extension
fields, subquadratic polynomial arithmetic, arbitrary precision, or guaranteed
success within any finite trial count.
