# Scope recovery: primitive binary BCH(15,7,5)

## Coverage decision

After exact Stirling transforms merged as
`main@42b742f798db3909a23f9b94e0007fa85518eb82`, a fresh live audit found
zero open pull requests and zero open issues.

The repository already contains:

- evaluation-form Reed-Solomon encoding and Berlekamp-Welch decoding;
- hard-decision convolutional decoding;
- exact prime-field extension arithmetic;
- prime-field polynomial factorization and Euclidean machinery.

It did not contain BCH coding, syndrome decoding, Berlekamp-Massey locator
reconstruction for a cyclic block code, or Chien search.

This slice deliberately chooses one canonical complete code rather than
pretending to provide an arbitrary BCH framework:

- primitive binary BCH(15,7,5);
- 7 message bits;
- 15 codeword bits;
- designed/minimum distance 5;
- bounded-distance correction radius 2.

That fixed contract keeps every algebraic boundary executable and exhaustively
verifiable.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; the frozen historical
compiler/backend frontier is untouched.

## Bit and polynomial convention

Bit vectors are polynomial coefficients in low-degree-first order.

For a 15-bit codeword:

- index 0 is the coefficient of `x^0`;
- index 14 is the coefficient of `x^14`.

The seven public message bits are also low-degree-first and are placed
systematically into codeword coefficients `x^8..x^14`.

Parity occupies coefficients `x^0..x^7`.

Every public bit must be exactly 0 or 1. Wrong-length or non-binary inputs are
rejected with `std::invalid_argument`.

## Field and generator

Decoding uses

`GF(16) = GF(2)[x] / (x^4 + x + 1)`.

The residue class of `x`, written `alpha`, has multiplicative order 15.
Production does not introduce a second finite-field implementation: it
constructs its 16x16 multiplication table and inverse table from the repository's
sealed `PrimeFieldExtension` implementation.

The nibble representation is the polynomial-basis coefficient vector over
`GF(2)`, so field addition is bitwise XOR.

The narrow-sense primitive BCH roots are

`alpha, alpha^2, alpha^3, alpha^4`.

The binary cyclotomic cosets modulo 15 are:

- `C1 = {1,2,4,8}`;
- `C3 = {3,6,12,9}`.

For primitive polynomial `x^4+x+1`, the corresponding binary minimal
polynomials are:

- `M1(x) = x^4 + x + 1`;
- `M3(x) = x^4 + x^3 + x^2 + x + 1`.

Their product is the degree-8 generator

`g(x) = x^8 + x^7 + x^6 + x^4 + 1`.

The production generator bit mask is therefore `0x01D1`.

## Systematic encoding

Let `m(x)` have degree below 7.

Production forms

`x^8 m(x)`

and computes its binary polynomial remainder `r(x)` modulo `g(x)`.

Because subtraction and addition are identical over `GF(2)`,

`c(x) = x^8 m(x) + r(x)`

is divisible by `g(x)`.

The upper seven coefficients remain exactly the supplied message, so encoding is
systematic.

## Syndrome contract

For received polynomial `r(x)`, production computes

- `S1 = r(alpha)`;
- `S2 = r(alpha^2)`;
- `S3 = r(alpha^3)`;
- `S4 = r(alpha^4)`.

A valid codeword has all four syndromes zero.

For up to two binary errors at coefficient positions `j1,j2`, define error
locators `Xk = alpha^jk`. The syndrome sequence is a linear recurrence whose
connection polynomial is the error-locator polynomial.

## Berlekamp-Massey

Production runs first-principles Berlekamp-Massey over the four GF(16)
syndromes.

The returned locator has the form

`Lambda(z) = product(1 + Xk z)`

because characteristic two makes minus equal plus.

For the designed radius, a valid correctable word must yield locator degree 1
or 2 unless every syndrome is already zero.

A locator degree above two is rejected.

## Chien search and correction

An error at codeword coefficient position `j` satisfies

`Lambda(alpha^(-j)) = 0`.

Production therefore evaluates the locator at every
`alpha^(-j)`, `j=0..14`.

The candidate is accepted only when:

1. the number of Chien roots equals the locator degree;
2. at most two positions are found;
3. flipping those binary positions makes all four syndromes zero;
4. extracting the systematic seven message bits and re-encoding them reproduces
   the corrected codeword exactly.

The final re-encode check deliberately closes the loop between the syndrome
decoder and the public cyclic encoder.

## Bounded-distance boundary

The decoder guarantees correction for every received word at Hamming distance
at most two from a BCH(15,7,5) codeword.

It does **not** claim that corruption of three or more bits is always detected.

For a received word beyond the designed radius, two outcomes are allowed:

- `nullopt` when locator/root/codeword checks fail;
- a valid codeword when the received word happens to lie within distance two of
  a different codeword.

This is the standard bounded-distance-decoder boundary and is not treated as an
undetected implementation failure.

## Independent verification

The committed primary oracle does not use:

- systematic polynomial division;
- GF(16) syndromes;
- Berlekamp-Massey;
- Chien search.

Instead, it constructs the complete binary cyclic code independently by
enumerating every degree-below-7 multiplier `q(x)` and forming

`q(x) g(x)`

directly over `GF(2)`.

This produces exactly 128 distinct 15-bit words.

The test suite computes every pairwise Hamming distance and verifies that the
minimum distance is exactly 5.

It then verifies production systematic encoding across all 128 possible
messages and requires the resulting set of codewords to equal the independent
128-word cyclic codebook exactly.

For decoding, every production codeword is corrupted by every error pattern of
weight 0, 1, or 2:

- 1 zero-error pattern;
- 15 one-bit patterns;
- 105 two-bit patterns.

That is 121 received words per message and

`128 * 121 = 15,488`

exhaustive bounded-radius decoding cases.

For each case, the independent oracle scans the complete 128-word codebook by
Hamming distance and confirms that the unique codeword within radius two is the
original codeword. Production must return exactly that word, the original
systematic message, and the exact sorted error positions.

Additional fixed-seed tests inject 3..6 distinct errors. Production is not
required to reject them, but any returned result must independently satisfy:

- membership in the exhaustive codebook;
- Hamming distance at most two from the received word;
- exact systematic re-encoding equality.

## Complexity boundary

This is a fixed `n=15`, `t=2` code, so asymptotic claims are intentionally
secondary.

Structurally:

- systematic encoding performs binary polynomial division across 15 bits;
- syndrome evaluation uses four roots across 15 coefficients;
- Berlekamp-Massey processes four syndrome symbols;
- Chien search evaluates a degree-at-most-2 locator at 15 field points.

The GF(16) multiplication/inverse tables are initialized once from the existing
extension-field implementation.

No benchmark-backed latency or throughput claim is made.

## Non-claims

This slice does not claim:

- arbitrary BCH lengths, dimensions, primitive polynomials, or designed
  distances;
- shortened or punctured BCH codes;
- soft-decision decoding;
- errors-and-erasures decoding;
- guaranteed detection beyond two errors;
- generic Berlekamp-Massey APIs for unrelated sequences;
- cryptographic properties;
- constant-time side-channel behavior;
- benchmark-backed performance.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/coding/bch_15_7_5.hpp`;
- `tests/test_bch_15_7_5_cases.hpp`;
- `docs/scope_recovery_bch_15_7_5.md`.

Pre-PR hardening includes:

- explicit `<algorithm>` / `<utility>` production includes;
- a valid deterministic randomized-test seed;
- removal of an unused production mask helper.

The repository's existing `tests/test_*_cases.hpp` auto-discovery path requires
no CMake or test-main edit.

No README, ROADMAP, recovery-authority, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `42b742f798db3909a23f9b94e0007fa85518eb82`.
