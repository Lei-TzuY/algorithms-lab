# Scope recovery: KLL varying-capacity quantile sketch core

## Coverage decision

Fresh live-state audit at `main@9fcf58129e9044a5c8bf4853ca0d7df330175836`
found no KLL implementation in default-branch code, pull-request history, or the
branch namespace. The sealed deterministic Greenwald-Khanna summary explicitly
lists a KLL/randomized guarantee as out of scope. A Count-Sketch branch is already
occupied by another workflow and is deliberately not touched.

Prospective governance remains `docs/scope_recovery_after_phase69.md`; the stale
historical ROADMAP and frozen compiler/backend surface remain untouched.

## Production contract

`algorithms::streaming::KllQuantileSketch(k, seed)` is a direct educational
implementation of the varying-capacity compactor hierarchy from the KLL family.

- signed-64 stream values; arbitrary duplicates and full signed boundaries;
- explicit replay seed with repository-defined SplitMix64 random bits;
- level `h` stores equal-weight samples of weight `2^h`;
- top scale parameter `k >= 2`; lower level scales follow
  `q_{d+1}=ceil(2 q_d / 3)` and physical compaction capacity is `q_d + 1`;
- every compaction sorts one equal-weight level, retains one exact item when the
  level size is odd, randomly keeps either the even or odd positions of the
  remaining even-sized prefix, and promotes those survivors one level;
- weighted retained-sample replay, approximate rank, one-based rank-to-quantile
  query, compaction count, level capacities, retained count, and full structural
  validation are exposed;
- identical `(k, seed, stream)` inputs replay byte-for-byte identical resident
  levels.

The concrete RNG-to-parity mapping is part of this baseline's replay contract;
it does not rely on implementation-defined `std::uniform_int_distribution`
behavior.

## Correctness / probability boundary

For one compaction at level weight `w`, sorting an even item sequence and keeping
either alternating parity preserves total weight. For any rank query, the
compaction changes weighted rank by either `-w`, `0`, or `+w`; the two nonzero
directions are selected with equal probability. An odd leftover is retained at
its original weight and contributes no compaction error.

The capacity recurrence satisfies

`q_d >= k (2/3)^d`,

so it meets the geometric lower-capacity condition used by Karnin, Lang, and
Liberty for the varying-capacity KLL hierarchy with `c=2/3`. When the hierarchy
gains a new top level, all lower capacities are recomputed and rebalanced.

This implementation additionally exposes a deterministic, per-run rank-error
certificate equal to the sum of level weights of all compaction operations,
saturated at the stream length. Triangle inequality gives a valid absolute rank
error upper bound for every query regardless of the random choices. This
certificate is intentionally conservative and is **not** presented as the KLL
concentration bound.

The KLL paper's probabilistic concentration theorem and asymptotic parameter
choices remain proof obligations. This baseline does not expose calibrated
`epsilon/delta` confidence constants and does not import Apache DataSketches'
empirical normalized-rank-error tables.

## Verification

Focused exact candidate bytes passed before upload under:

- GCC C++20 repository strict warnings-as-errors: 6/6;
- Clang C++20 repository strict warnings-as-errors: 6/6;
- actual GCC ASan+UBSan with leak detection / fail-fast: 6/6.

Evidence includes validation, empty and no-compaction exact semantics, signed-64
boundaries, same-seed replay, different-seed divergence, real compression of a
5,000-item stream, retained-weight conservation, and capacity invariants.

The primary randomized correctness oracle is independent of compactor routing:
240 fixed-seed streams are retained in full by the test, exact ranks are computed
directly from the original values, and every tested production rank/quantile is
required to lie within the deterministic compaction-error certificate. A
separate weighted-sample oracle checks rank-to-quantile query mechanics.

A committed anti-exact regression deliberately finds a small-capacity seeded run
whose median estimate differs from the exact median while still satisfying the
certificate. Finite randomized execution is therefore executable evidence of an
approximate sketch, not disguised exactness.

## Complexity / non-claims

This direct hierarchy keeps the capacity-two lower levels explicitly instead of
folding them into the paper's constant-space sampler. With hierarchy height `H`,
the geometric capacities use `O(k + H)` resident sample slots. A worst-case
single insertion can cascade through levels and sort compacted buffers; a
conservative direct bound is `O(k log k + H)` comparison/bookkeeping work for one
cascade. Weighted-sample materialization / rank-to-quantile queries sort the
`R=O(k+H)` retained samples and therefore use `O(R log R)` work; direct rank
queries scan `O(R)` samples.

No sampler-folded `O(k)` space claim, top fixed-capacity / GK hybrid, summary
merge API, weighted updates, deletions, sliding windows, Apache binary format,
Apache `k=200` calibration, empirical 99% error constant, adversarial randomness,
or benchmark speedup is claimed.
