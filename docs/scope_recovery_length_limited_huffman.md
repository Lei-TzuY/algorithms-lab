# Scope recovery: optimal length-limited Huffman coding

## Coverage decision

A fresh live-state audit at `main@2a88f4d413fce689a45931727853fcbff0ecc3f0` found no Package-Merge, length-limited Huffman, or equivalent bounded-code-length production capability in default-branch code or commit history, and no occupied `package-merge` branch. The repository already contains the sealed deterministic ordinary `HuffmanByteCodebook`; this slice leaves that implementation unchanged and studies the distinct global optimization problem introduced by a caller-supplied maximum code length.

The historical compiler/backend roadmap remains prospectively frozen by `docs/scope_recovery_after_phase69.md`. This recovery slice changes proof model again after the exact pathwidth / BK-tree / Karatsuba checkpoints rather than continuing those adjacent surfaces.

## Production contract

`algorithms::coding::LengthLimitedHuffmanByteCodebook` accepts the same 256-entry non-negative byte-frequency table as the sealed ordinary Huffman codebook plus `maximum_code_length`.

- zero-frequency symbols are absent;
- an empty alphabet is valid even with limit zero;
- a non-empty alphabet requires a positive limit;
- a one-symbol alphabet uses the conventional one-bit code `0`;
- infeasible bounds are rejected rather than silently exceeded;
- returned code lengths never exceed the caller's bound;
- returned bit strings form a deterministic canonical prefix code sorted by `(length, byte)`;
- `weighted_bit_count()` is the exact sum of `frequency * code_length` when representable as `uint64_t`, otherwise construction throws `std::overflow_error`;
- `valid_codebook()` replays absence/presence, lengths, binary code strings, prefix-freeness, and weighted cost.

For `n > 1`, the implementation may internally clamp a caller bound above `n-1`: every full binary prefix tree with `n` positive-frequency leaves has an optimal representative with maximum depth at most `n-1`, so larger limits cannot improve the optimum.

## Package-Merge / coin-collector proof obligation

For a positive-frequency symbol `i` with weight `w_i`, create one conceptual coin at every permitted depth `d = 1..L`. The coin has cost `w_i` and width `2^{-d}`. Selecting the first `l_i` depth coins for a symbol costs `w_i l_i` and has total width `1 - 2^{-l_i}`.

For more than one symbol, an optimal binary prefix code can be taken as a full tree, hence Kraft equality holds:

`sum_i 2^{-l_i} = 1`.

Therefore the selected coins must have total width

`sum_i (1 - 2^{-l_i}) = n - 1`.

Package-Merge solves this binary coin-collector instance bottom-up. At each width it pairs the cheapest available items into packages of the next width and merges those packages with the original symbol coins at that width. At the final `1/2` width, selecting the cheapest `2n-2` items gives total width `n-1`; recursively unpacking the selected packages counts how many depth coins were selected for each symbol, which is its code length.

The correctness of the Package-Merge coin-collector reduction and the Kraft/full-tree correspondence are proof obligations. The finite tests below are implementation evidence, not a proof of those theorems.

## Determinism and representability

Items are ordered first by exact representable cost, then deterministic leaf/package metadata. Package costs that exceed `uint64_t` are tracked as explicit overflow sentinels instead of wrapping. If an overflowing package is required in the selected solution, unpacking it necessarily contributes more than `UINT64_MAX` to the exact weighted path length, so the final checked cost rejects the result. Relative ordering among already-unrepresentable packages cannot turn an unrepresentable selected total into a representable optimum.

After optimal lengths are recovered, canonical codes are generated without assuming the code fits in a machine integer: the current bit string is incremented directly and extended with zeroes as lengths increase.

## Verification

Focused verification before upload passes:

- GCC C++20 under repository strict warnings-as-errors;
- Clang C++20 under the same strict warnings;
- actual GCC ASan+UBSan with leak detection.

Deterministic cases cover empty/singleton semantics, infeasible length bounds, a skewed known optimum, equal-frequency canonical replay, huge non-binding caller bounds, and exact `uint64_t` weighted-cost boundaries.

The primary randomized oracle uses 500 fixed-seed instances with 2..6 positive-frequency symbols and feasible maximum lengths at most five. It exhaustively enumerates every length vector in `[1,L]^n`, filters by exact Kraft equality, and computes the minimum weighted path length directly. Production cost must match that independent optimum exactly.

As secondary integration evidence only, the same random frequency tables are solved with a non-binding limit and compared with the sealed ordinary `HuffmanByteCodebook` weighted optimum. Ordinary Huffman is not the primary oracle for the constrained problem.

## Complexity and non-claims

With `n <= 256` positive-frequency byte symbols and effective maximum length `L <= n-1`, this direct implementation explicitly sorts each package level for deterministic tie metadata. Its conservative bound is `O(L n log n)` time and `O(L n + output_bits)` auxiliary/result storage. This intentionally does not claim the tighter complexity of optimized Package-Merge variants.

This slice does not claim alphabetic coding, adaptive/dynamic Huffman coding, unequal letter costs, D-ary coding, entropy-coding bitstream framing, canonicality independent of the caller's byte ids, or a universal compression-ratio improvement over ordinary Huffman coding.

## Scope

The intended candidate changes exactly four paths:

- `include/algorithms/coding/length_limited_huffman.hpp`;
- `tests/test_length_limited_huffman_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this focused recovery proof document.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark, frozen compiler/backend, or temporary-file churn belongs in this slice.
