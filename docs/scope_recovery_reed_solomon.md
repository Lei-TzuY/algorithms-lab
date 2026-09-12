# Scope recovery — Reed-Solomon unique decoding

## Coverage decision

After the bounded-exact Bareiss determinant reached merged `main`, a fresh live
code, commit, pull-request, and branch audit found no Reed-Solomon,
Berlekamp-Welch, or equivalent error-correcting-code capability in
`algorithms-lab`. Several superficially attractive frontiers were rejected as
already present, including directed arborescence, simplex linear programming,
matroid intersection, minimum-mean cycle, planarity, and SMAWK.

This slice changes proof model again rather than extending the recent integer
linear-algebra or compression work. It composes the existing prime-field modular
arithmetic, polynomial evaluation, and finite-field Gaussian elimination into an
error-correction contract: encode a low-degree polynomial, tolerate bounded
symbol corruption, solve the Berlekamp-Welch equations, and replay the recovered
codeword against every received symbol.

The frozen Phase-45–69 compiler/backend surface remains untouched under
`docs/scope_recovery_after_phase69.md`.

## Production contract

The public API provides evaluation-form Reed-Solomon encoding and unique
decoding over a prime field `F_p`.

- a message is a non-empty coefficient vector defining a polynomial `P` of
  degree `< k`, low degree first;
- evaluation points are canonicalized modulo `p` and must be pairwise distinct;
- encoding requires `n >= k` and returns `P(x_i)` for every evaluation point;
- decoding accepts a received word of the same length, a message dimension `k`,
  and radius `t`, with the explicit unique-decoding precondition
  `n >= k + 2t`;
- input symbols are interpreted modulo `p`;
- success returns exactly `k` canonical message coefficients, the corrected
  codeword, and the replayed error positions;
- if no codeword lies within Hamming distance `t`, decoding returns `nullopt`.

The decoder does **not** claim that every transmission containing more than `t`
corruptions is detectable. Such a received word can, in principle, fall within
radius `t` of a different codeword. The contract is instead the standard unique
nearest-codeword statement under `n >= k + 2t`.

No erasures, extension-field symbols, list decoding, systematic/FFT codec,
cryptographic integrity, burst-error model, or arbitrary soft-decision claim is
made.

## Berlekamp-Welch invariant

Let `E(x)` be a monic error-locator polynomial of degree `t`, and let
`Q(x) = P(x) E(x)` have degree `< k+t`. For every received sample `(x_i,y_i)`,
Berlekamp-Welch imposes

`Q(x_i) = y_i E(x_i)`.

With the leading coefficient of `E` fixed to one, the remaining coefficients of
`Q` and `E` form a linear system with `k+2t` unknowns. The implementation reuses
the repository's prime-field RREF solver instead of implementing another
elimination engine.

For any transmitted codeword corrupted in at most `t` positions, every solution
of those equations satisfies `Q = P E`: the difference `Q-PE` has degree
`< k+t` and vanishes at at least `n-t >= k+t` distinct field points. Therefore it
is the zero polynomial. This remains true when fewer than `t` actual errors make
the linear system underdetermined; the deterministic particular solution is
still valid.

A consistent linear system alone is not accepted as a certificate. Production
also:

1. divides `Q` by the monic `E` in `F_p` and requires zero remainder;
2. requires the quotient shape to be exactly the requested message dimension;
3. re-encodes the quotient at every evaluation point;
4. counts the exact Hamming disagreement positions against the received word;
5. returns success only when that replayed distance is at most `t`.

This fail-closed replay prevents an algebraically consistent but non-codeword
candidate from being exposed as a successful decode.

## Verification

Focused candidate passes:

- GCC C++20 strict warnings-as-errors;
- Clang C++20 strict warnings-as-errors;
- actual GCC ASan+UBSan with leak/UB halting.

Deterministic evidence covers exact decoding, zero-radius decoding, two-error
recovery, composite-modulus rejection, duplicate evaluation points modulo the
field, mismatched shapes, insufficient redundancy, and a received word that is
outside every permitted decoding ball.

Two independent randomized evidence layers are used:

- 600 fixed-seed `F_257` messages with `n=9`, `k=5`, `t=2`; each trial injects
  zero, one, or two distinct non-zero symbol corruptions and requires exact
  recovery of the original message, corrected codeword, and error positions;
- 500 arbitrary received words over `F_5` with `n=5`, `k=3`, `t=1`; the primary
  oracle exhaustively enumerates all `5^3 = 125` messages, re-encodes every
  codeword, and independently decides whether a unique radius-one codeword
  exists. Decoder success/failure and returned message must match that exhaustive
  oracle exactly.

The exhaustive layer is intentionally independent of Berlekamp-Welch and
Gaussian elimination.

## Complexity / non-claims

For `n` received symbols and `u = k + 2t` Berlekamp-Welch unknowns, the direct
RREF backend uses dense polynomial-time field arithmetic (bounded conservatively
by `O(n u^2 + u^3)` arithmetic work in this educational implementation), followed
by `O(k t + n k)` polynomial division/replay work. Repository modular
multiplication itself is overflow-safe repeated doubling, so no unit-cost
full-width multiplication performance claim is implied.

This slice prioritizes a replayable algebraic correctness boundary rather than
fast Reed-Solomon coding, SIMD tables, NTT/FFT acceleration, or arbitrary-length
production throughput.
