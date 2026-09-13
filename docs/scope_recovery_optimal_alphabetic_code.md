# Scope recovery: optimal alphabetic prefix coding

## Coverage decision

A fresh post-Phase-69 coverage audit at `main@c5462ece364ba1009e23e6a8ed73304c874306b4` found the sealed deterministic Huffman byte codebook and the sealed optimal-BST / Knuth-optimization slice, but no optimal alphabetic prefix-code / Hu-Tucker / Garsia-Wachs production surface or recovery branch. The optimal-BST recovery explicitly lists Hu-Tucker/alphabetic coding as a non-claim. The genuinely occupied `scope-recovery-minimum-cycle-basis` and `scope-recovery-bfprt-selection` branches are left untouched.

This slice deliberately leaves the recent Robbins-orientation -> open-ear -> st-numbering -> densest-subgraph graph streak and changes proof model to compatibility-preserving greedy merges plus an alphabetic depth reconstruction.

## Production contract

`algorithms::coding::optimal_alphabetic_prefix_code(weights)` accepts an ordered sequence of non-negative `uint64_t` leaf weights and returns:

- one binary codeword per input position, in the same order as the weights;
- the exact leaf depth of every codeword;
- exact weighted external path length as a two-limb unsigned value;
- the number of Garsia-Wachs merges.

The empty input returns empty output. A one-leaf input uses the mathematical depth-zero / empty-codeword convention. Zero weights are accepted. The direct educational baseline accepts at most 4096 leaves; larger inputs fail with `std::length_error` before quadratic work/output is attempted.

## Garsia-Wachs obligation

The first phase augments the current sequence with conceptual `+infinity` sentinels. Repeatedly:

1. find the first consecutive triple `x,y,z` with `x <= z`;
2. replace `x,y` by a parent of weight `x+y`;
3. move that parent left to immediately after the rightmost earlier weight at least `x+y`.

The implementation stores the resulting merge tree and then reads the depth assigned to each original input leaf. Correctness relies on the generalized Garsia-Wachs theorem for non-negative weights: the merge sequence yields an optimal leaf-depth multiset for the alphabetic external-path-length problem, even though the temporary merge tree itself need not have leaves in the original order.

The ordered reconstruction does not reuse the greedy recurrence. It scans the target depths left-to-right. Whenever the two rightmost completed subtrees have the same root depth, it joins them under a parent one level higher. The Garsia-Wachs depth theorem guarantees that this process ends in one depth-zero root. Labeling left edges `0` and right edges `1` produces a full prefix tree whose leaf order is exactly the input order and whose leaf depths are unchanged; therefore its weighted cost equals the optimum delivered by the merge phase.

Tests are implementation evidence; they do not replace the Garsia-Wachs correctness theorem.

## Exact arithmetic boundary

For `n <= 4096`, every temporary subtree weight is at most `n * UINT64_MAX < 2^76`. Every leaf depth is at most `n-1 < 2^12`, so the complete weighted path length is less than `2^88`. The public two-limb 128-bit accumulator therefore covers every accepted input exactly. Addition and weight-times-depth accumulation are implemented without compiler-specific `__int128`; impossible internal overflow still fails closed.

## Independent verification

Focused pre-upload execution passed:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with halt-on-error: 4/4.

Evidence includes empty/singleton semantics, the 4096-leaf cap, all-zero and strongly skewed weights, deterministic replay, prefix-free/alphabetic codeword replay, and a full-width two-leaf vector whose optimum exceeds `uint64_t` but is exactly represented by the two-limb result.

The primary optimality oracle is structurally independent of Garsia-Wachs. For 900 fixed-seed random ordered vectors with up to nine leaves and weights in `[0,30]`, tests run the ordinary interval dynamic program

`C[i,j] = sum(weights[i..j)) + min_k(C[i,k] + C[k,j])`

with singleton cost zero, scanning every split. Production weighted cost must equal that exact optimum. A separate 300-case corpus through 128 leaves replays codeword depths and weighted cost without using the merge recurrence.

## Complexity / non-claims

This is intentionally the naive inspectable Garsia-Wachs baseline. Linear sequence scans, vector erase/insert, and explicit codeword materialization give `O(n^2 + B)` time, where `B` is the total number of returned code bits, and `O(n + B)` resident/result storage. The repository does **not** claim the balanced-tree `O(n log n)` acceleration, a Hu-Tucker implementation, length-limited coding, adaptive coding, byte-stream encoding/decoding, canonical-Huffman compatibility, or compression-ratio/entropy guarantees.

## Scope

Exactly four paths are intended to differ from live main:

- `include/algorithms/coding/alphabetic_code.hpp`;
- `tests/test_alphabetic_code_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- `docs/scope_recovery_optimal_alphabetic_code.md`.

No CMake, README, historical ROADMAP, recovery authority, workflow, benchmark, frozen compiler/backend, minimum-cycle-basis, BFPRT-selection, or temporary-file churn is required.
