# Scope recovery: exact `uint64_t` prime factorization

## Capability

`factorize_uint64(value, seed)` returns the complete prime-factor multiset of a
non-zero `uint64_t`, sorted in ascending order and retaining multiplicity. `1`
has the empty factorization; `0` is rejected because it has no finite prime
factorization. The result also records the seed and the number of Pollard-Rho
polynomials tried so a fixed-seed execution is replayable.

This slice reuses the sealed number-theory substrate rather than replacing it:
`gcd`, overflow-safe `multiply_mod`, and deterministic full-width `is_prime`.

## Pollard-Rho search invariant

Composite odd values not divisible by three are split with the polynomial
`f(x) = x^2 + c (mod n)`. The tortoise/hare walk repeatedly evaluates `f` using
`multiply_mod`, so squaring never relies on overflowing native multiplication.
At each step `gcd(|x-y|, n)` is either `1`, a non-trivial divisor, or `n`. Only a
strict divisor is accepted. A walk that cycles without finding one is discarded
and restarted with fresh polynomial parameters.

The pseudo-random stream is a repository-defined SplitMix64 sequence. Bounded
parameter selection uses explicit rejection sampling instead of
`std::uniform_int_distribution`, avoiding implementation-dependent mapping for a
fixed seed. This is a replay contract, not a cryptographic-randomness claim.

## Exactness obligation

Pollard-Rho is used only to discover a split. It does not decide whether a
returned leaf is prime. Recursive leaves are accepted only after the sealed
deterministic `is_prime` test succeeds. Every recursive split satisfies
`n = divisor * (n/divisor)` exactly, and final factors are sorted. Therefore the
random search path can change the decomposition order or number of restarts, but
not the exact factor multiset.

Tests additionally replay every factorization by dividing the original value by
each returned prime factor and requiring the final remainder to be `1`.

## Verification

Deterministic cases cover `0`, `1`, primes, a power of two, a repeated large
prime, `2^64-1`, a semiprime made from two primes near `2^32`, and a prime near
`UINT64_MAX`. A fixed seed must reproduce the complete result metadata, while
different seeds must still produce the same sorted prime factors.

A fixed-seed corpus of 2,500 values in `[1, 10^7]` is compared against an
independent trial-division oracle. That oracle shares neither Pollard-Rho nor the
Miller-Rabin control flow. Every returned factor is separately checked with the
sealed deterministic primality test and the complete product is replayed by
exact division.

Focused pre-upload builds passed the repository warning policy under GCC and
Clang, plus an actual GCC AddressSanitizer + UndefinedBehaviorSanitizer build:
4/4 focused test groups in all three configurations.

## Complexity and non-claims

Primality leaves use the sealed deterministic Miller-Rabin implementation.
Pollard-Rho has randomized/heuristic search cost; this implementation deliberately
does not advertise a deterministic worst-case factorization bound or claim that
a particular seed finishes within a fixed number of iterations. Each polynomial
walk is bounded and failed walks restart.

The implementation is not a cryptographic factorization service, does not claim
side-channel resistance, and does not use timing data as complexity evidence.
Correctness is exact even though divisor discovery is randomized.
