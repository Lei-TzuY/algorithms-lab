# Scope recovery: exact prime-field polynomial interpolation

## Coverage decision

A fresh live-state and code-search audit at `main@7a041398caacb97ded186def3cb283060adbb861` found no polynomial-interpolation API, implementation, recovery branch, or matching commit. Existing finite-field coverage includes overflow-safe modular arithmetic, deterministic full-`uint64_t` primality, Gauss-Jordan elimination, polynomial Euclid/Bézout GCD, square-free factorization, Berlekamp-Massey recurrences, and NTT-based polynomial operations, but none reconstructs a polynomial from point samples.

This slice deliberately stays away from the simultaneously active graph/string recovery surface. It adds exact algebraic reconstruction with a Newton divided-difference invariant rather than another graph cycle, string index, or frozen backend continuation. Prospective authority remains `scope_recovery_after_phase69.md`; the historically stale ROADMAP backend heading is intentionally untouched.

## Production contract

`interpolate_prime_field(samples, p)` accepts point samples over the prime field `F_p` and returns the unique polynomial of degree strictly below `samples.size()` in ordinary monomial form, with coefficients ordered from constant term upward and canonicalized to `[0,p)`.

- `p` must be prime and is checked by the sealed deterministic full-`uint64_t` primality implementation;
- sample coordinates and values are canonicalized modulo `p`;
- x coordinates must be distinct after canonicalization; collisions are rejected;
- an empty sample set returns the empty coefficient vector, representing the zero polynomial;
- more than `p` samples are rejected before interpolation because distinct field x coordinates are then impossible;
- all full-width multiplication uses the sealed overflow-safe `multiply_mod` primitive;
- field inverses use Fermat inversion `a^(p-2)` through the sealed `power_mod` primitive.

`evaluate_polynomial_mod` provides exact Horner evaluation under the same prime-field validation and canonicalization rules.

## Newton invariant

Let the normalized samples be `(x_i,y_i)`. Production computes Newton divided differences in place. After divided-difference order `d`, the updated entry at index `i` is the coefficient for the Newton basis term associated with samples `i-d..i`. Because every normalized x coordinate is distinct and `p` is prime, every denominator `x_i-x_{i-d}` is non-zero in `F_p` and therefore invertible.

The resulting Newton representation is converted to ordinary monomial coefficients by repeatedly applying

`P_k(x) = a_k + (x - x_k) P_{k+1}(x)`.

The conversion maintains one exact field coefficient vector after each step; modular addition/subtraction avoid unsigned overflow and multiplication delegates to the sealed full-width modular primitive.

Correctness relies on uniqueness of degree-`< n` interpolation through `n` distinct field abscissas and the Newton divided-difference identity. Those are proof obligations; randomized tests are implementation evidence rather than theorem substitutes.

## Independent verification

The primary randomized oracle does not implement divided differences. For 1,500 fixed-seed trials it:

1. selects a small prime field;
2. generates an ordinary coefficient vector directly;
3. evaluates it with a separate straightforward power-series loop whose small modulus guarantees ordinary `uint64_t` products cannot overflow;
4. presents the resulting samples in shuffled, non-canonical x representations; and
5. requires interpolation to recover the exact original coefficient vector.

Deterministic evidence additionally covers the empty polynomial, a known quadratic, composite/invalid moduli, modulo-x collisions, impossible sample cardinality, and a known-answer vector under the largest prime below `2^64`. That full-width case uses externally precomputed exact values rather than deriving expected coefficients through the production evaluator.

The focused candidate passed GCC C++20 strict warnings-as-errors, Clang C++20 strict warnings-as-errors, and an actual GCC ASan+UBSan build, all 4/4. Full-repository Actions on the uploaded candidate remain the integration authority.

## Complexity and non-claims

For `n` samples, this straightforward baseline fills `n(n-1)/2` divided-difference cells and therefore performs `Theta(n^2)` field inversions in addition to `O(n^2)` field additions/multiplications for divided differences and Newton-to-monomial conversion. Each inverse is implemented as `a^(p-2)` through `power_mod`; with the repository's current bit-by-bit `multiply_mod`, a conservative primitive modular-add/doubling bound is `O(n^2 log^2 p)`. Storage is `O(n)` apart from the returned coefficient vector and normalized/sorted x temporaries. No constant-time field-operation model is being smuggled into the implementation claim.

This slice does **not** implement composite-modulus interpolation, Hermite/repeated-abscissa interpolation, batch inversion, fast product-tree interpolation, FFT/NTT-accelerated interpolation, multipoint evaluation, error-correcting interpolation, rational reconstruction, or polynomial fitting over non-fields.
