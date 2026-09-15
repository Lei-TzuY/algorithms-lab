# Scope recovery: quotient filter approximate membership

## Coverage decision

Fresh live audit after the merged soft-heap checkpoint found no quotient filter,
quotient/remainder approximate-membership table, or matching recovery PR in the
default branch or pull-request history. The matching branch namespace was empty
before this slice was created. Nearby alternatives were deliberately rejected:
HyperLogLog and partially-persistent DSU already had occupied recovery branches,
and full SPQR decomposition remains too broad for one independently verifiable
checkpoint.

This slice follows `docs/scope_recovery_after_phase69.md`. It does not modify the
historical compiler/backend ROADMAP or the frozen compiler/runtime surface.

## Production contract

`algorithms::data_structures::QuotientFilter64(q, r, seed)` is an insertion-only
approximate-membership set over `uint64_t` values.

- `2 <= q <= 20`; the physical table contains `2^q` slots.
- `r >= 1` and `q+r <= 64`; the replayable mixer contributes a `q+r` bit
  fingerprint split into a canonical quotient and a stored remainder.
- one physical slot is intentionally kept empty, so public capacity is
  `2^q - 1`; insertion beyond that bound rejects rather than relying on a
  full-table circular sentinel ambiguity.
- every canonical quotient owns one sorted run of unique remainders.
- `occupied` marks canonical quotients, `continuation` marks non-first run
  members, and `shifted` records entries displaced from their canonical slot.
- insertion uses circular cluster shifting; it does not rebuild from a hidden
  exact set.
- `contains` can return false positives after fingerprint collisions, but an
  inserted/represented fingerprint cannot become a false negative.
- if a distinct value collides with an already represented fingerprint,
  `insert` returns false and does not increase `size()`; `size()` is therefore
  the number of represented fingerprints, not an exact distinct-input count.
- `debug_slots()` and `valid_structure()` expose/replay the representation for
  educational verification.

No deletion, resizing, merge, persistence, concurrency, or exact-set semantics
is implied.

## Representation invariant

For every quotient whose canonical slot has `occupied=1`, `find_run_start`
walks backward to the start of the enclosing cluster and then advances one run
for each occupied quotient until the target quotient is reached.

Within that run:

1. the first entry has `continuation=0`;
2. later entries have `continuation=1` and strictly increasing remainders;
3. `shifted` is true exactly when the physical slot differs from the run's
   quotient;
4. every non-empty physical slot belongs to exactly one run.

Insertion into an existing run chooses the sorted position. Insertion at a run
front turns the displaced former first entry into a continuation. Every shifted
entry keeps the `occupied` bit belonging to its *physical* canonical quotient;
only remainder/continuation/shifted payload moves. Keeping one empty slot makes
the circular shift terminate.

`valid_structure()` independently enumerates every occupied quotient, rebuilds
all runs, rejects duplicate/unsorted remainders or inconsistent metadata, and
requires exact one-to-one coverage of all physical non-empty slots.

## Approximation / hashing boundary

The implementation uses a fixed SplitMix-style 64-bit replay mixer parameterized
by `seed`. This mixer is deterministic and non-cryptographic. The code does
**not** claim it is a universal hash family or that adversarial caller keys obey
an ideal-hash distribution.

If the retained `p=q+r` fingerprint bits were uniformly distributed, a query
for a nonmember collides with any one represented fingerprint with probability
`2^-p`; the union bound gives at most `n/2^p` for `n` represented fingerprints.
That is the classical model boundary, not a numeric guarantee made for the
concrete deterministic mixer on arbitrary input distributions.

The committed anti-exact regression deliberately finds a noninserted value that
queries present in an 8-bit-fingerprint filter, then verifies inserting that
value is a no-op on fingerprint count. Finite randomized tests are evidence for
implementation correctness, not a proof of a false-positive probability.

## Verification

Focused final candidate passes:

- GCC C++20 with repository strict warnings-as-errors;
- Clang C++20 with the same strict warnings;
- actual GCC ASan+UBSan with leak/UB fail-fast.

Committed evidence covers parameter validation, duplicate fingerprints, the
reserved-empty-slot capacity boundary, deterministic replay, insertion-order
canonicalization, genuine false positives, and metadata diagnostics.

Randomized evidence includes:

- 300 fixed-seed 32-slot filters filled to the 31-fingerprint public capacity,
  stressing circular wraparound and checking `valid_structure()` after every
  insertion plus membership for every attempted key;
- 240 fixed-seed 256-slot filters with 180 distinct input values, checking the
  final structure and no-false-negative membership against an independent
  `std::set<uint64_t>` input oracle.

An additional pre-upload local stress corpus built 20,000 small filters with
quotient widths 2..6, filled each to its public capacity, replayed structure
after every insertion, and checked every attempted key for no false negatives.

## Complexity / non-claims

Let `m=2^q` and let `C` be the number of physical slots scanned/shifted in the
relevant cluster. `contains` is `O(C)` and insertion is `O(C)` direct work;
worst-case `C=O(m)`. Storage is `O(m)`. `valid_structure()` is an intentionally
expensive diagnostic and can take `O(m^2)` direct work in the worst case because
it independently locates every represented run.

No expected constant-time bound, target load-factor performance bound, deletion,
resizing, exact cardinality, universal/adversarial hash bound, cryptographic
property, or benchmark speedup is claimed.
