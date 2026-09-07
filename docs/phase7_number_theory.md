# Phase 7 — number-theory foundations

This slice adds a coherent unsigned-64-bit number-theory core: Euclidean GCD, overflow-safe modular multiplication/exponentiation, and deterministic Miller-Rabin primality testing.

## Euclidean GCD

`gcd(a,b)` uses the standard remainder recurrence until the second operand is zero. It defines `gcd(0,0) = 0` and otherwise returns the non-negative common divisor in `uint64_t`. Each iteration strictly decreases the non-zero remainder, giving logarithmic Euclidean complexity.

## Overflow-safe modular arithmetic

A direct `uint64_t` product may overflow before `% modulus` can reduce it, so `multiply_mod` never evaluates that product. After reducing both operands below a non-zero modulus, it performs binary doubling and conditional accumulation.

The internal `add_mod(a,b,m)` assumes `a,b < m` and computes `(a+b) mod m` as either `a + b` when representable below `m`, or `a - (m-b)` when the mathematical sum crosses `m`. Both branches avoid unsigned wraparound. `multiply_mod` therefore computes the exact modular product in `O(log b)` modular additions without `__int128` or an external big-integer dependency.

`power_mod` uses ordinary binary exponentiation but routes every multiplication through `multiply_mod`, so it remains correct even when the unreduced product would exceed `UINT64_MAX`. A zero modulus is rejected; modulus one canonically returns zero.

## Deterministic uint64 primality

`is_prime` first rejects values below two and handles divisibility by small primes. It then writes `n-1 = d * 2^s` with odd `d` and performs Miller-Rabin rounds using the fixed bases

`{2, 325, 9375, 28178, 450775, 9780504, 1795265022}`.

The implementation relies on the known theorem that this seven-base set, after reducing each base modulo `n`, is sufficient for deterministic Miller-Rabin classification across the full unsigned 64-bit domain. The repository's tests validate the implementation; they do not pretend to re-prove that external number-theoretic witness theorem.

Each modular exponentiation uses the overflow-safe arithmetic above. This implementation favors first-principles portability under the repository's strict `-Wpedantic` build over non-standard wider integer extensions. Consequently it does not claim the constant factors of an implementation using native 128-bit multiplication.

## Verification

Deterministic tests cover zero/one moduli behavior, zero GCD inputs, known Carmichael and strong-pseudoprime composites, a prime near `UINT64_MAX`, `UINT64_MAX` itself, and large modular multiplication/exponentiation known-answer vectors computed independently with arbitrary-precision arithmetic.

Fixed-seed randomized verification includes:

- 5,000 full-width GCD pairs compared with `std::gcd`,
- 5,000 small-modulus modular multiply/power cases where ordinary test-only multiplication is provably safe,
- 20,000 values below one million compared with independent trial-division primality.

The small-range oracles and known-answer vectors exercise the implementation independently from the production overflow-avoidance recurrence. Passing those tests is implementation evidence, not a proof of the fixed-witness theorem itself.

## Frontier

The unsigned-64-bit arithmetic/primality slice passed its candidate and merged-main gates and is sealed as part of Phase 7. The phase-wide architecture/integration decision and promotion rationale are recorded in [`phase7_sealing_audit.md`](phase7_sealing_audit.md).
