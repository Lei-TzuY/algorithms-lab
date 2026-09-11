# Scope recovery: finite-field Gaussian elimination

## Coverage decision

After the exact-cover checkpoint reached merged-main green, a fresh coverage audit
moved away from another combinatorial-search variant. The repository already has
full-width modular arithmetic and deterministic primality, but no exact linear-
algebra surface: no matrix rank, consistency test, reduced-row-echelon witness,
or reconstructable affine solution space. This slice fills that gap over prime
fields and deliberately reuses the sealed number-theory arithmetic rather than
introducing a separate modular backend.

## Capability

`solve_linear_system_mod_prime(variable_count, coefficients, rhs, modulus)`
solves `A x = b` over the prime field `F_modulus` for rectangular systems. The
variable count is explicit so a zero-equation system can still describe a
non-zero-dimensional solution space. Input coefficients and right-hand sides are
normalized modulo the field modulus.

The result exposes:

- coefficient-matrix rank;
- deterministic ascending pivot columns;
- the complete reduced coefficient matrix and reduced right-hand side;
- an explicit consistency flag;
- for consistent systems, one canonical particular solution with every free
  variable set to zero;
- one canonical nullspace basis vector for each free column, in ascending order.

Composite, zero, and unit moduli are rejected. Row-count / right-hand-side
mismatches and rows whose width differs from `variable_count` are rejected before
elimination.

## RREF invariant

After pivot `r` is chosen in column `c`, the pivot row is scaled so entry `(r,c)`
is exactly one, and column `c` is eliminated from every other row. Previously
established pivot columns stay canonical because every later candidate row already
has zeros in those columns. Pivot columns are therefore strictly increasing and
the first `rank` rows carry their corresponding unit pivots.

All row operations are invertible over a field. A row whose reduced coefficient
part is all zero but whose reduced right-hand side is non-zero is therefore an
exact inconsistency witness. Otherwise, assigning every free variable zero and
reading pivot variables from the reduced right-hand side yields a valid
particular solution.

For a free column `f`, the canonical nullspace vector sets `x_f = 1`, every other
free variable to zero, and each pivot variable to the modular negation of the
RREF coefficient in column `f`. These vectors are linearly independent and span
the homogeneous nullspace. Thus every solution is exactly
`x_particular + sum(alpha_i * basis_i)`.

## Arithmetic boundary

The implementation requires a prime modulus and obtains pivot inverses as
`pivot^(p-2) mod p`, using the sealed deterministic primality test plus
overflow-safe `power_mod` / `multiply_mod`. Modular subtraction is implemented
without unsigned wraparound. The surface therefore supports the full prime
`uint64_t` modulus range accepted by the sealed number-theory layer rather than
only machine-multiplication-safe small fields.

This is exact finite-field algebra, not floating-point numerical linear algebra.
No conditioning, approximate solve, composite-modulus ring solve, Smith normal
form, or arbitrary-precision integer-matrix claim is implied.

## Verification

Deterministic regressions cover malformed shapes, non-prime moduli, zero-equation
systems with free variables, zero-variable consistent/inconsistent systems,
input normalization, a unique solution, an underdetermined affine space, an
inconsistent system, and arithmetic over the full-width prime
`18446744073709551557` including a non-trivial inverse.

A fixed-seed corpus of 300 random systems uses moduli 2, 3, or 5 with at most four
variables and four equations. The primary oracle enumerates every possible
assignment and independently checks `A x = b`; the entire affine solution set
reconstructed from the production particular solution and nullspace basis must
match that exhaustive set exactly. Rank is checked separately by enumerating all
linear combinations of the original coefficient rows, counting the distinct row
span, and recovering its field dimension. This avoids validating Gaussian
elimination with another Gaussian-elimination recurrence.

The focused candidate passes the repository strict-warning policy under GCC and
Clang and an actual GCC AddressSanitizer + UndefinedBehaviorSanitizer build: 6/6
focused test groups in all three configurations.

## Complexity and non-claims

For `m` equations, `n` variables, and rank `r`, Gauss-Jordan elimination performs
`O(m n r)` field-operation slots and stores `O(m n + n(n-r))` values including
the returned RREF and nullspace basis. In this repository's full-width modular
backend, multiplication itself is implemented by `O(log p)` safe additions and
pivot inversion by modular exponentiation, so the machine-operation cost includes
those arithmetic factors; this slice does not hide them behind a unit-cost
full-width multiplication claim.

The algorithm is deterministic once the input row order is fixed: each pivot is
the first available non-zero row in the current column, and free columns are
reported in ascending order. No sparse-matrix, fraction-free, cache-optimal,
parallel, or asymptotically faster matrix-multiplication claim is made.
