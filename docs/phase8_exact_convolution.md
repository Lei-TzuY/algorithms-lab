# Phase 8 — exact bounded integer convolution

This slice lifts the fixed-field NTT from modular polynomial arithmetic into an exact signed-integer convolution surface, but only where exact reconstruction is provable before execution.

## Two transform fields

The implementation reuses one private parameterized radix-2 NTT engine with two prime fields:

- `p1 = 998244353 = 119 * 2^23 + 1`, primitive root `3`;
- `p2 = 1004535809 = 479 * 2^21 + 1`, primitive root `3`.

The public `998244353` transform API remains unchanged. The second field is used only by `convolution_exact_int64`. Because the second modulus supplies roots only through order `2^21`, exact convolution is limited to padded transform length `2^21` even though the first field supports `2^23`.

Stage roots and inverse-length factors for both fields continue to use the Phase-7 `number_theory::power_mod` implementation. The prime/primitive-root facts are mathematical parameter assumptions; implementation tests do not claim to prove those external facts.

## Exactness and representability boundary

The combined CRT modulus is

`M = 998244353 * 1004535809 = 1002772198720536577`.

Since `M` is odd, centered reconstruction is unique in

`[-floor(M/2), floor(M/2)] = [-501386099360268288, 501386099360268288]`.

Before running either transform, the implementation computes a conservative coefficient bound from the inputs:

`min(n, m) * max(abs(left)) * max(abs(right))`.

Every output coefficient contains at most `min(n,m)` products, so if that bound is at most `floor(M/2)`, every true coefficient lies inside the unique centered CRT interval. The bound comparison uses division before multiplication so it cannot overflow `uint64_t`; `INT64_MIN` magnitude is handled without signed negation overflow.

If the conservative bound cannot prove exact reconstruction, the call throws `std::overflow_error`. This intentionally rejects some cancellation-heavy inputs whose actual result might fit. Rejecting an unproven case is preferable to returning a residue alias and calling it exact.

The centered range is far below `INT64_MAX`, so every accepted reconstructed coefficient is representable by the public `int64_t` result type.

## CRT reconstruction

Each signed coefficient is reduced independently into both fields, transformed, multiplied pointwise, and inverse-transformed. For residues `r1 mod p1` and `r2 mod p2`, the implementation solves the two-congruence system using the inverse of `p1 mod p2`, producing one canonical value in `[0,M)`. Values above `floor(M/2)` are mapped to their negative centered representative.

All products in the transform and CRT combine path are bounded below about `1.01e18`, well inside `uint64_t`; no non-standard wider integer extension is required.

## Verification

Deterministic tests cover signed coefficients, empty operands, both centered reconstruction boundaries, rejection immediately beyond the bound, and `INT64_MIN * 0` to exercise magnitude handling without overflow.

A separate adversarial case deliberately has cancellation in the true result while the conservative product bound exceeds the CRT range; rejection verifies the documented fail-closed policy rather than silently relaxing it.

Five hundred fixed-seed random polynomial pairs use lengths 0–32 and coefficients in `[-1,000,000, 1,000,000]`. Production two-prime NTT/CRT output is compared exactly with an independent test-only `O(nm)` signed-integer convolution. Under these generator bounds the naïve oracle arithmetic is provably representable in `int64_t`, and it does not reuse NTT residues, CRT reconstruction, or the production coefficient-bound calculation.

Focused GCC, Clang, and GCC ASan+UBSan builds use the repository strict-warning policy and pass all three exact-convolution test groups before upload. Full repository CI remains the integration gate.

## Frontier

This exact-convolution slice is sealed as part of the integrated Phase-8 checkpoint. Its fail-closed exactness boundary and relationship to the fixed-field transform are reviewed in [`phase8_sealing_audit.md`](phase8_sealing_audit.md). The promoted frontier is Phase 9 weighted combinatorial optimization.
