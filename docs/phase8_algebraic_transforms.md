# Phase 8 — algebraic transforms and polynomial algorithms

Phase 8 begins with a fixed-modulus Number Theoretic Transform because it turns the Phase-7 modular-arithmetic substrate into an executable algebraic transform and polynomial-convolution pipeline rather than adding another unrelated routine.

## Fixed field and transform boundary

The transform works over the prime modulus `998244353 = 119 * 2^23 + 1` with primitive root `3`. Public transform input is normalized modulo the field. Empty input is a deliberate no-op; non-empty transform lengths must be powers of two and cannot exceed `2^23`, the largest supported power-of-two root-of-unity order under this modulus.

The implementation uses iterative bit-reversal followed by radix-2 Cooley-Tukey butterfly stages. Forward stage roots are powers of the fixed primitive root. Inverse roots and the final inverse-length factor are computed through the existing Phase-7 `number_theory::power_mod`, providing explicit cross-phase reuse instead of another modular-exponentiation implementation.

Butterfly multiplication deliberately does **not** call the generic overflow-safe multiplier. Every butterfly operand is reduced below `998244353`, so a product is below `(998244353 - 1)^2 < 10^18`, safely representable in `uint64_t`. Direct multiplication is therefore both portable and justified by a concrete representability bound.

For the fixed modulus, transform runtime is `O(n log n)` and auxiliary state beyond the input vector is `O(1)`; convolution allocates two transform buffers and is `O(N log N)` time / `O(N)` storage for the selected padded transform length `N`.

## Polynomial convolution

`convolution_mod_998244353(left, right)` returns the coefficient convolution reduced modulo `998244353`. If either polynomial is empty, the result is empty. Result-size arithmetic is checked before allocation, and a convolution requiring a transform longer than `2^23` is rejected rather than silently using an invalid root-of-unity order.

The implementation pads each input to the smallest power of two covering `left.size() + right.size() - 1`, transforms both, multiplies corresponding field elements, applies the inverse transform, and truncates to the exact result length.

## Independent verification

Deterministic tests cover a known polynomial product, empty operands, empty-transform behavior, singleton normalization/round-trip, and rejection of a non-power-of-two transform size.

Fixed-seed transform verification performs forward+inverse round trips on random full-width `uint64_t` vectors for every power-of-two size from 1 through 4096.

Six hundred fixed-seed random polynomial pairs use lengths 0–80 and full-width `uint64_t` coefficients. Production NTT convolution is compared exactly with a test-only `O(nm)` oracle. The oracle reduces each coefficient modulo the field and multiplies two reduced values directly; this is independently safe because each product is below `10^18`, and it reduces the accumulator after every term. The oracle does not call the NTT or reuse butterfly state.

The fixed modulus/primitive-root fact is a mathematical parameter assumption of this slice. Round-trip and differential tests validate our implementation but are not presented as a proof that `3` is a primitive root.

## Frontier

This completes the first ordered Phase-8 slice. Phase 8 remains active. The next roadmap frontier is exact bounded integer convolution using multiple NTT-friendly primes plus CRT, with an explicit coefficient/reconstruction bound rather than an unqualified exactness claim.
