# Scope recovery: fixed-horizon AMS second-frequency-moment sketch

## Coverage decision

A fresh default-branch, pull-request, code, and branch audit at
`main@c2d77fb9716469cef7c70c0f74e599278aebfac3` found Count-Min,
Misra-Gries, and Greenwald-Khanna streaming summaries but no Alon-Matias-Szegedy
second-frequency-moment sketch or equivalent sampled-position `F2` estimator.
The prospective authority remains `docs/scope_recovery_after_phase69.md`; the
stale late-backend ROADMAP heading is not an implementation frontier.

The immediately preceding recovery work already covered orthogonal geometry,
y-fast predecessor search, DAG path cover, maximum-weight closure, matching,
numerical linear algebra, transforms, and shortest paths. This slice therefore
moves to a different proof model: a one-pass randomized streaming estimator with
an exact expectation identity and explicit replay semantics.

## Production contract

`AmsF2FixedHorizonSketch` receives a fixed stream horizon `n` and one or more
explicit sample positions in `[0,n)`. Each row stores only its sampled position,
the item observed at that position, and that item's suffix occurrence count.
Updates are one-pass and never retain the complete stream or a frequency table.

For a row sampled at position `J`, let `R` be the number of occurrences of the
sampled item from `J` through the end of the completed stream. The row estimator
is

`X = n * (2R - 1)`.

The final estimate is the arithmetic mean of the row estimators. Estimates are
unavailable before the fixed horizon has been fully consumed; feeding beyond the
horizon is rejected. Duplicate sample positions are permitted and remain
independent row state in the API.

The companion `ams_replayable_sample_positions` helper is deliberately a replay
facility, not a probability API. It specifies SplitMix64 plus rejection sampling
so a `(horizon,row_count,seed)` triple reproduces the same positions without
relying on implementation-defined bounded-distribution mapping. It is not
cryptographic, and this slice does not claim that one deterministic seed or any
particular finite row count supplies exactness or a caller-specified confidence
level.

## Expectation proof obligation

Fix an item with final frequency `f`. If each of its occurrences is considered
as the sampled position, the corresponding suffix counts are
`f, f-1, ..., 1`. Their odd factors sum exactly:

`(2f-1) + (2(f-1)-1) + ... + 1 = f^2`.

Therefore, when `J` is uniform over the `n` stream positions,

`E[X] = sum_i f_i^2 = F2`.

The implementation does not infer this theorem from randomized tests. Instead,
the test suite also makes the finite identity executable: for every small stream
it instantiates one row at every position and requires the mean of all row
estimators to equal the independently computed exact `F2` exactly.

## Verification

The repo-native streaming test translation unit covers:

- empty-horizon and invalid-row/sample-position validation;
- estimate-before-completion and fixed-horizon overrun rejection;
- a hand-checkable stream whose five row estimators are
  `25,15,15,5,5` and average to exact `F2=13`;
- duplicate sample-position replay and row-state equality;
- deterministic seeded position replay and in-range positions;
- exhaustive every-position verification for every binary stream of lengths
  one through seven;
- 500 fixed-seed random streams of lengths one through thirty over a nine-item
  alphabet, again using every position and comparing with an independent exact
  frequency-map oracle; and
- an anti-overclaim regression where a fixed seeded three-row replay returns a
  deterministic estimate different from exact `F2`, demonstrating that replay
  does not imply exactness.

Focused strict GCC, strict Clang, and actual ASan+UBSan builds pass the same
production/test surface before upload. Repository CI remains the authoritative
integration gate.

## Complexity and non-claims

With `r` rows, each stream update scans the row states in `O(r)` time and the
resident sketch uses `O(r)` state. Producing all row estimates is `O(r)`.

This baseline does not claim the sharper update bounds available from other AMS
formulations, unknown-horizon reservoir sampling, mergeability, turnstile
updates, cryptographic randomness, four-wise-independent sign projections,
finite-sample confidence bounds, or exact answers from a finite randomized run.
Those would require separate contracts and proof obligations rather than being
silently inferred from this sampled-position baseline.

## Scope

Exactly three repository files change: a new header-only production sketch, the
already-registered streaming test translation unit, and this proof document. No
CMake, ROADMAP, README, frozen compiler/backend, workflow, benchmark, or
placeholder surface changes.
