# Scope recovery: hard-decision Viterbi convolutional decoding

## Coverage decision

After the Frequent Directions streaming-covariance checkpoint, a fresh live-code,
PR-history, and branch audit found no Viterbi, trellis-decoding, or convolutional-
code implementation in the repository. Reed-Solomon / Berlekamp-Welch is already
sealed and solves a different block-code problem. This slice therefore changes
proof model again: shortest-path-style dynamic programming over a finite-state
trellis rather than another streaming sketch, numerical routine, or adjacent
covariance variant.

Prospective governance remains `docs/scope_recovery_after_phase69.md`; the frozen
compiler/backend sequence and historically stale ROADMAP are untouched.

## Production contract

`algorithms::coding::RateHalfConvolutionalCode` is a bounded binary rate-1/2
convolutional-code baseline.

- constraint length `K` is in `[2,16]`;
- two nonzero `K`-bit generator masks define the emitted pair for each input bit;
- the encoder begins in all-zero state and emits exactly two hard bits per input
  bit;
- `decode_hard(received)` requires an even-length binary received word and returns
  the exact minimum-Hamming-distance trellis path from the zero initial state;
- the final state is unconstrained: this slice does not add zero-tail termination,
  tail biting, or puncturing;
- equal-metric paths use lexicographically smallest message bits as the canonical
  deterministic witness;
- the result replays message bits, corrected codeword, exact Hamming metric, and
  final trellis state.

Generator masks may define a degenerate code. Production still solves the exact
hard-decision path problem for that trellis; it does not claim a minimum free
distance or a universal error-correction radius.

## Trellis invariant and optimality obligation

A state stores the previous `K-1` input bits. For each time step and each state,
production retains the minimum Hamming metric among all message prefixes ending
there. Every longer path has exactly one predecessor state plus one new input bit,
so Bellman's optimality principle makes the two-predecessor relaxation complete.
The final minimum over states is therefore the exact minimum-Hamming path over all
equal-length messages from the documented zero initial state.

Deterministic tie breaking is itself stateful. Each retained prefix receives its
lexicographic rank. A one-bit extension has key `(previous_rank,input_bit)`, so
scanning keys in `2*rank+bit` order gives the exact lexicographic order of retained
prefixes without storing or comparing whole prefixes during the forward pass.
Predecessor/input tables then reconstruct the selected optimum exactly.

The dynamic-programming / lexicographic-ranking arguments are proof obligations;
finite tests are implementation evidence rather than the source of the theorem.

## Verification

Focused final candidate passes:

- GCC C++20 under repository strict warnings-as-errors: 4/4;
- Clang C++20 under the same strict warnings: 4/4;
- actual GCC ASan+UBSan with fail-fast/leak detection: 4/4.

Deterministic evidence covers constructor/bit/shape validation, the classical
`K=3` generator pair `(0b111,0b101)`, empty input, degenerate-code ties, repeated
determinism, and a two-hard-bit corruption whose exact optimum recovers the known
message with metric two.

The primary randomized oracle is structurally independent. For 900 fixed-seed
cases with `K=2..5` and message length `0..9`, tests enumerate every possible
message, encode it with a separate direct shift-register/parity recurrence, and
select the global minimum Hamming distance with lexicographic tie breaking.
Production must match the exact metric and message. The same test separately
requires production `encode()` to equal the independent encoder, then replays the
corrected codeword and final state. The oracle never calls production encoding
while choosing the optimum, so a shared encoder/decoder defect cannot self-verify.

## Complexity and non-claims

For `T` received symbol pairs and `S=2^(K-1)` trellis states, the direct decoder
performs `O(T*S)` transition/rank work and stores `O(T*S)` predecessor/input state
plus `O(S)` rolling metrics/ranks. Encoding is `O(T)`. `K<=16` keeps state width
and generator masks inside the documented bounded machine-word domain.

This slice does not claim soft-decision metrics, puncturing, recursive systematic
convolutional codes, tail-biting/terminated decoding, free-distance computation,
BCJR/MAP decoding, turbo codes, SIMD acceleration, communications-standard
compatibility, cryptographic properties, or benchmark speedup.
