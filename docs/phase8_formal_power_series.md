# Phase 8 — formal power series inverse

This slice builds a higher-level polynomial operation on top of the existing `998244353` NTT convolution rather than introducing another multiplication engine. It computes a truncated multiplicative inverse in the formal power series ring over the existing transform field.

## API and algebraic precondition

`formal_power_series_inverse(series, terms)` returns coefficients `B[0..terms)` such that

`A(x) B(x) = 1 (mod x^terms, 998244353)`.

`terms == 0` returns an empty vector. For a non-empty result, the input must contain a constant coefficient and `series[0] mod 998244353` must be non-zero; otherwise the series is not invertible in this field and construction throws `std::invalid_argument`.

The first inverse coefficient is `A(0)^(p-2) mod p`, computed with the existing Phase-7 `number_theory::power_mod` implementation.

## Newton doubling

Suppose the current approximation `B` satisfies `AB = 1 mod x^m`. The next approximation uses

`B' = B * (2 - AB) mod x^k`, where `k = min(2m, terms)`.

Writing the current error as `E = 1 - AB`, the new error is

`1 - AB' = 1 - AB(2 - AB) = (1 - AB)^2 = E^2`.

Because `E` is divisible by `x^m`, `E^2` is divisible by `x^(2m)`, so each iteration doubles the number of correct low-order coefficients. Both polynomial products delegate to `convolution_mod_998244353`; the implementation truncates after each stage and never duplicates NTT logic.

## Transform-size boundary

The public limit is `2^22` requested coefficients. At a Newton step targeting `k`, the previous inverse has at most `k/2` coefficients. Both convolution shapes are therefore at most `k + k/2 - 1 < 3k/2` coefficients long. For the maximum `k = 2^22`, the next power-of-two padding is at most `2^23`, exactly the existing primary NTT limit. Requests above `2^22` are rejected with `std::length_error` rather than relying on an unsupported transform order.

With NTT multiplication, the geometric sequence of doubling stages gives `O(n log n)` time in the requested prefix length and `O(n)` peak polynomial storage, under the fixed-field transform model already documented for Phase 8.

## Independent verification

Deterministic tests cover zero requested terms, missing/non-invertible constant terms, the explicit `2^22` boundary rejection, the first six coefficients of `1/(1+x)`, and a non-unit constant series.

Four hundred fixed-seed random series use source lengths 1–30, requested inverse lengths 1–64, and full-width `uint64_t` coefficients with a forced non-zero constant residue. Production Newton inversion is compared exactly with an independent `O(n^2)` coefficient recurrence:

`b0 = a0^-1`,

`bk = -a0^-1 * sum(i=1..k) ai * b(k-i)`.

The oracle uses its own test-only modular multiplication and binary exponentiation; reduced factors are below `998244353`, so its direct products are below `10^18`. It does not call Newton iteration, NTT convolution, production `power_mod`, or production inverse code.

Focused GCC, Clang, and GCC ASan+UBSan builds under the repository strict-warning policy pass both deterministic and randomized test groups before upload. Full repository CI remains the integration gate.

## Frontier

This formal-power-series slice is sealed as part of the integrated Phase-8 checkpoint. Its Newton invariant, transform-size boundary, and oracle independence are reviewed in [`phase8_sealing_audit.md`](phase8_sealing_audit.md). The promoted frontier is Phase 9 weighted combinatorial optimization.
