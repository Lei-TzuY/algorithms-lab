# Scope recovery — replayable FKS static perfect hashing

## Coverage decision

A fresh live-code, PR-history, and branch audit at `main@cc8fee83a329341fde2f3a1f4e03f08431f63031` found no FKS/perfect-hashing capability and no occupied recovery branch for that surface. Existing rolling hashes answer substring-fingerprint queries, Count-Min is an approximate frequency sketch, and the treap/B-tree/vEB families are dynamic ordered sets; none provides collision-free static membership through a two-level universal-hashing layout.

This slice follows `scope_recovery_after_phase69.md` and leaves the frozen compiler/backend surface untouched. It deliberately changes proof model after the recent SMAWK and prime-field root-extraction checkpoints.

## Production contract

`FksStaticSet32` is an immutable set over the complete `uint32_t` key universe.

- duplicate constructor keys are canonicalized away;
- membership is exact: a hash hit is accepted only when the stored key equals the query;
- construction uses the prime `P = 4294967311 > 2^32` and affine hashes
  `h(x) = ((a*x+b mod P) mod m)`, with `a != 0`;
- modular products reuse the repository's overflow-safe `multiply_mod` rather than relying on overflowing native multiplication;
- the primary table has one bucket per distinct key;
- only a primary hash with `sum_i bucket_size_i^2 <= 4n` is accepted;
- each nontrivial secondary bucket receives exactly `k^2` slots and accepts only a collision-free secondary hash;
- caller-visible seed plus explicit primary/secondary attempt budgets make construction replayable and bounded;
- exhausting either attempt budget throws instead of falling back to a collision-prone representation;
- successful construction exposes attempt counts, hash parameters, total secondary slots, and an expensive structural validator.

The deterministic SplitMix64 stream and rejection-sampled bounded mapping are replay infrastructure. They are not claimed to be cryptographic randomness or theorem-certified independent random samples.

## FKS proof boundary

For the classical independently/uniformly sampled universal affine family, two distinct keys collide in an `m`-slot table with probability at most `1/m`.

For `n` keys in `n` primary buckets,

`sum_i k_i^2 = n + 2 * (# colliding unordered key pairs)`.

Therefore the expected square sum is below `2n`; Markov's inequality gives constant probability that it is at most `4n`. Once such a primary hash is accepted, the complete second-level payload is deterministically bounded by `4n` slots.

For a bucket of size `k`, a `k^2`-slot secondary hash has expected colliding-pair count below `1/2`, so an independently sampled secondary hash is collision-free with constant probability. A collision-free second-level table plus stored-key equality makes membership exact.

Those classical expectation statements require the independent uniform sampling assumption. This implementation instead exposes a deterministic seeded parameter stream and finite attempt budgets for replay; it does not claim that every seed inherits the ideal-random expected construction time. Tests are implementation evidence, not a proof that SplitMix outputs are independent random variables.

## Complexity / non-claims

Let `n` be the number of distinct input keys, `A_p` the caller's primary-attempt budget, and `A_s` the per-bucket secondary-attempt budget.

- canonicalization: `O(n log n)` through sorting/unique;
- successful resident state: `O(n)` primary metadata plus at most `4n` secondary slots;
- membership: two affine hashes plus constant slot checks; with the repository's first-principles `multiply_mod`, this is conservatively `O(log P)` code-local work (constant under the fixed 32-bit domain);
- bounded construction is finite; a conservative direct bound is `O(n log n + (A_p + A_s) n log P + A_s n)` after the accepted `4n` square-sum cap.

No dynamic insert/erase, full-`uint64_t` universe, minimal-space hash table, cryptographic hash, adversarial-PRNG resistance, deterministic construction-success, or benchmark-backed speed claim is implied.

## Verification

Focused repo-native verification passed before upload under:

- GCC C++20 repository strict warnings-as-errors;
- Clang C++20 repository strict warnings-as-errors;
- actual GCC ASan+UBSan with leak detection.

Deterministic evidence covers empty input, duplicate canonicalization, `0`/`UINT32_MAX`, missing-key queries, invalid zero budgets, input-order-independent same-seed replay, and fixed seeds that exhaust exactly one primary or secondary attempt budget.

The primary randomized oracle is `std::set<uint32_t>` used only in tests: 300 fixed-seed input multisets of up to 128 generated keys, including duplicate noise, each run 256 independent membership probes. Every successful production instance must match the oracle exactly, pass full structural replay, and satisfy `secondary_slot_count <= 4 * distinct_key_count`. The same key multiset is shuffled and rebuilt with the same seed to verify identical public hash-parameter diagnostics.

## Scope

Exactly three repository files change: a header-only production structure, the already-registered data-structure test translation unit, and this proof document. No CMake, README, ROADMAP, recovery-authority, workflow, benchmark, or frozen compiler/backend files change.
