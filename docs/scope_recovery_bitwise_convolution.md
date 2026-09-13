# Scope recovery: modular bitwise convolution transforms

## Coverage decision

A fresh post-Phase-69 live audit after dense Cholesky recovery found no OR/AND/XOR bitwise-convolution or Walsh-Hadamard production surface, PR, or matching branch. The sealed ranked-zeta subset-convolution slice explicitly left OR/AND/XOR convolution outside its contract. This slice fills that named algebraic-transform gap rather than extending the immediately preceding numerical-linear-algebra work or resuming the frozen compiler/backend frontier.

## Contract

`algorithms::combinatorial::bitwise_convolution_mod(first, second, modulus, kind)` accepts equal, non-empty, power-of-two `uint64_t` tables in bit-mask order and returns one of:

- OR convolution: `h[s] = sum_{a | b = s} f[a] g[b] (mod m)`;
- AND convolution: `h[s] = sum_{a & b = s} f[a] g[b] (mod m)`;
- XOR convolution: `h[s] = sum_{a ^ b = s} f[a] g[b] (mod m)`.

OR and AND accept every modulus `m >= 2`, including composite and full-width moduli. XOR additionally requires odd `m`: for table length `N = 2^k`, inverse Walsh-Hadamard scaling needs `N` to be a unit in `Z/mZ`. Input coefficients are reduced modulo `m`. Full-width pointwise multiplication reuses the sealed overflow-safe `number_theory::multiply_mod`.

## Transform obligations

For OR, the subset zeta transform maps `F[s]` to the sum over masks contained in `s`; pointwise multiplication therefore combines every pair whose bitwise union is contained in `s`. Subset Mobius inversion isolates the exact union.

For AND, the dual superset zeta/Mobius pair applies the same argument to masks containing `s`, isolating exact intersections.

For XOR, each butterfly applies `(x,y) -> (x+y,x-y)`. Over a commutative ring the Walsh matrix squares to `N I`. Because `N` is a power of two, odd modulus is exactly the condition needed here for inverse scaling. Production computes `inv(2) = (m>>1)+1` without overflowing `UINT64_MAX` and obtains `inv(N)` through sealed modular multiplication.

These algebraic identities are proof obligations. Random tests are implementation evidence rather than proofs of zeta/Mobius or Walsh inversion.

## Verification

Focused repo-native candidate passes:

- GCC C++20 strict warnings-as-errors;
- Clang C++20 strict warnings-as-errors;
- actual GCC ASan+UBSan.

Deterministic evidence covers shape/modulus validation, XOR even-modulus rejection, singleton and composite-modulus behavior, and independent arbitrary-precision known answers at modulus `UINT64_MAX` for all three operations.

Primary randomized evidence uses 600 fixed-seed table pairs with `N` from 1 through 128. For six small moduli, an independent direct `O(N^2)` oracle enumerates every pair `(a,b)`, routes its product to `a|b`, `a&b`, or `a^b`, and accumulates modulo `m`. The randomized oracle deliberately uses moduli at most 1,000,003 so direct `uint64_t` multiplication cannot overflow and does not reuse production `multiply_mod`. RNG values consume raw `mt19937_64` output; replay does not depend on implementation-specific distribution mappings.

## Complexity and non-claims

For `N=2^k`, each transform uses `O(N log N)` modular additions/subtractions and `O(N)` pointwise overflow-safe modular multiplications. XOR inverse scaling adds `O(N)` modular multiplications plus `O(log N)` multiplications to build `N^{-1}`. Storage is `O(N)` beyond the returned vector.

The `multiply_mod` helper has its own code-local logarithmic cost; no single-machine-operation multiplication or benchmark speedup is claimed. This slice does not add ordinary subset convolution, NTT/FFT replacement, generic semiring transforms, non-power-of-two tables, or XOR convolution for even moduli.

## Scope

Exactly four paths change:

- `include/algorithms/combinatorial/bitwise_convolution.hpp`;
- `tests/test_bitwise_convolution_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this proof document.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark, frozen compiler/backend, or occupied recovery-surface churn is included.
