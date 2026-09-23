# Scope recovery: exact dyadic Knuth-Yao sampler

## Coverage decision

A fresh live audit on
`main@f7905981176c99c2a63c053bba80373661ef2f26` found zero open pull
requests and zero open issues.

The repository already contains replayable randomized algorithms and streaming
sketches, including Karger min-cut, Freivalds verification, AMS F2, KLL, and
replayable FKS construction. None provides exact discrete-distribution
generation from fair bits.

No Knuth-Yao, alias-method, or discrete sampler implementation, branch, or
implementation PR was present when this slice was selected.

A contemporaneous rANS candidate was rejected because
`scope-recovery-rans-byte-coding` was already occupied by another branch.

## Public contract

`DyadicKnuthYaoSampler` accepts a vector of non-negative `uint64_t`
weights.

Requirements:

- at least one symbol;
- positive total weight;
- the total weight must be an exact power of two;
- the sum must fit in `uint64_t`.

Zero-weight symbols are legal and must be unreachable.

`sample(bit_source)` consumes successive caller-supplied bits until a
discrete-distribution-generating-tree leaf is reached and returns:

- the selected symbol index;
- the exact number of bits consumed.

The class does not own an RNG. Fairness and independence of the supplied bits
are external assumptions. The production correctness claim is combinatorial:
if every length-k fair bit word is equally likely, output probabilities equal
the normalized input weights exactly.

## Power-of-two normalization

Weights may contain a common power-of-two factor.

For example:

- `{1,3}` and `{2,6}` describe the same probabilities;
- `{1}`, `{8}`, and `{0,8,0}` are deterministic distributions.

Construction computes the minimum trailing-zero count among all positive
weights, bounded by the denominator exponent, and divides every positive
weight and the total by that shared power of two conceptually.

This reduces the probability-matrix precision without changing any ratio.

The original weights and original total remain the public contract.
Normalization changes only the number of fair bits needed by the finite DDG
tree.

## Probability matrix and DDG construction

Let the normalized total be `2^k`.

The normalized weight of symbol `i` has a k-bit binary representation. Reading
those bits from most significant to least significant gives the finite binary
probability matrix columns for that symbol.

At depth `d=1..k` production:

1. expands every still-unassigned frontier node into two child slots;
2. scans symbols in increasing index order;
3. for every symbol whose probability-matrix bit in column `d` is one,
   converts one available slot into a leaf for that symbol;
4. carries all remaining slots to the next depth.

The sum of dyadic probabilities is exactly one, so the final frontier must be
empty.

Leaf placement is deterministic, but exact probabilities depend only on leaf
depths and labels, not on a statistical experiment.

## Exact mass invariant

A leaf at depth `d` represents exactly

`original_total / 2^d`

full-denominator bit words.

`valid_structure()` replays the entire tree and checks:

- every reachable node is visited exactly once;
- internal nodes have exactly two valid distinct children;
- leaves have no children;
- leaf symbols are in range;
- no path exceeds normalized precision;
- every leaf contributes non-zero dyadic mass;
- accumulated leaf mass for every symbol equals its original input weight
  exactly;
- every stored node is reachable from the root.

Thus structural validation independently reconstructs the input distribution.

## Independent verification

The committed oracle does not inspect production node layout.

For a sampler with k effective bits, tests enumerate bit words, feed bits
MSB-first through the public `sample` API, and count returned symbols.

When raw weights have a common power-of-two factor, the reduced k-bit patterns
repeat the corresponding number of times across the original total-word
enumeration. The final exact count must equal every original weight.

Committed coverage includes:

- empty input;
- all-zero input;
- non-power-of-two total;
- `uint64_t` sum overflow;
- deterministic normalized distributions such as `{0,8,0}` consuming zero
  bits;
- early termination for `{3,1}`;
- reducible weights `{2,6}` with normalized precision two;
- zero-weight symbols;
- several fixed dyadic distributions;
- 320 fixed-seed random small distributions with exact full-word enumeration.

A supplementary exhaustive model check enumerated every composition with:

- total weight in `{1,2,4,8,16,32}`;
- one through six symbols.

That is 530,607 distinct weight vectors. Every construction completed and every
exhaustive output count exactly matched the input weights after normalization.
This supporting model check is not compiler or sanitizer evidence.

## Bit-consumption boundary

The sampler may stop before k bits when a leaf occurs at a shallower depth.

A deterministic distribution has a root leaf and consumes zero bits.

`max_bits_per_sample()` reports the normalized dyadic precision, not the raw
denominator exponent.

This slice does not claim an empirical entropy benchmark. It exposes actual
bits consumed for each sample and relies only on the exact finite DDG
construction.

## Complexity

Let:

- `s` be the number of symbols;
- `k <= 63` be normalized dyadic precision;
- `N` be the number of constructed DDG nodes.

Construction scans all symbols at each probability-matrix column and builds
each tree node once, giving a conservative `O(sk + N)` bound.

Sampling follows one root-to-leaf path and therefore consumes and processes at
most k bits.

Structural replay is `O(N + s)`.

No claim is made that explicit node storage is asymptotically optimal.

## Non-claims

This slice does not claim:

- arbitrary non-dyadic rational or floating-point probabilities;
- cryptographic randomness;
- validation of caller bit-source fairness or independence;
- alias-table sampling;
- empirical goodness-of-fit evidence;
- entropy benchmark superiority;
- streaming distribution updates;
- thread-safe mutable updates.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/randomized/knuth_yao_sampler.hpp`;
- `tests/test_knuth_yao_sampler_cases.hpp`;
- `docs/scope_recovery_knuth_yao_sampler.md`.

Pre-PR hardening found and fixed:

- raw-weight probability-matrix construction failed for reducible dyadic
  weights;
- the zero-bit deterministic branch compared raw mass with one instead of
  normalized mass;
- explicit `<algorithm>` inclusion for `std::min`;
- a regression locking normalized precision for `{2,6}`.

No CMake, README, historical ROADMAP, workflow, benchmark, or temporary-file
change is required. The repository automatically discovers
`tests/test_*_cases.hpp`.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base at branch creation:
`f7905981176c99c2a63c053bba80373661ef2f26`.
