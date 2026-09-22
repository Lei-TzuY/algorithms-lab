# Scope recovery: first-principles min-max heap

## Coverage decision

A fresh live-state audit after exact LZW merged as
`main@1e38302bdf6bcf2e8f488e3c35a5bb217bf606de` found zero open pull
requests and zero open issues. Default-branch search found no min-max heap,
double-ended priority queue, or DEPQ implementation. Branch search found no
occupied min-max-heap surface, and commit search found no prior min-max-heap
implementation checkpoint.

The repository already contains an ordinary binary heap plus Fibonacci, pairing,
radix, and soft-heap recovery surfaces. Adding another single-ended meldable heap
would therefore provide weak architectural gain. This slice instead recovers a
different priority-queue contract: one complete-tree structure supports both
minimum and maximum endpoints and deletes either endpoint in logarithmic time.

Prospective scope remains governed by
`docs/scope_recovery_after_phase69.md`; the frozen compiler/backend history and
stale historical ROADMAP remain untouched.

## Production contract

`algorithms::data_structures::MinMaxHeap` stores signed 64-bit values with
multiset semantics.

- duplicate and full-width `int64_t` values are ordinary entries;
- `minimum()` and `maximum()` expose the two extrema;
- `push(value)` inserts one occurrence;
- `pop_minimum()` and `pop_maximum()` remove and return one endpoint;
- endpoint access/removal on an empty heap rejects explicitly;
- `size()`, `empty()`, and `valid_invariants()` expose state and an
  expensive structural diagnostic.

This surface deliberately does not add handles, arbitrary-key decrease/increase,
meld, stable duplicate identities, concurrency, persistence, or allocator
guarantees.

## Alternating-level invariant

The heap is one level-order complete binary tree in a contiguous vector.

- the root is on a **min level**;
- levels alternate min, max, min, max, ...;
- every min-level node is no greater than all of its descendants;
- every max-level node is no less than all of its descendants.

It is sufficient to maintain the local two-level form: a min-level node is no
greater than each child and grandchild, while a max-level node is no less than
each child and grandchild. Repeating that relation every two levels yields the
global descendant property.

Therefore the root is the global minimum. The global maximum is the larger of
the root's at most two children, so both endpoint lookups are constant time.

## Insertion

A new value is appended at the next complete-tree position. Its parent is on the
opposite level.

- if a min-level insertion exceeds its max-level parent, the pair is swapped and
  the promoted value bubbles through max-level grandparents;
- otherwise it bubbles through min-level grandparents;
- max-level insertion is symmetric.

Every bubble step moves exactly two tree levels, so insertion touches at most
`O(log n)` ancestors.

## Endpoint deletion

Deleting the minimum replaces the root with the last level-order element and
trickles it down on min levels. Deleting the maximum removes the larger root
child and similarly trickles on max levels.

At each step production chooses the most extreme candidate among the current
node's children and grandchildren.

- if the candidate is a child, one same-direction swap repairs the local
  boundary and terminates;
- if it is a grandchild, swapping moves the hole two levels while preserving the
  same min/max parity;
- after a grandchild swap, the displaced value is checked once against its
  opposite-level parent and repaired if necessary.

The max-level operation is the exact order-dual of the min-level operation.

## Complexity / non-claims

For `n` stored elements:

- `minimum`, `maximum`, `size`, and `empty`: `O(1)`;
- `push`, `pop_minimum`, and `pop_maximum`: `O(log n)`;
- resident storage: `O(n)`;
- `valid_invariants()`: `O(n)`, because each node checks at most six local
  descendants.

These are structural asymptotic claims, not benchmark claims. No cache-optimal,
branch-prediction, parallel, lock-free, meldable, or universally faster-than-two-
heap claim is made.

## Independent verification

Before upload, the candidate core was compiled under the repository strict
warning profile with both GCC C++20 and Clang C++20 and passed a 50,000-operation
standalone differential trace against `std::multiset`.

Committed repo-native evidence adds:

- empty-operation rejection;
- duplicate values and exact `INT64_MIN` / `INT64_MAX` boundaries;
- 4,096 ascending insertions followed by alternating deep minimum/maximum
  removals;
- a descending duplicate-heavy plateau;
- 20,000 fixed-seed mixed insert/min-delete/max-delete/query operations checked
  after every step against an independent `std::multiset` oracle;
- structural-invariant replay throughout the deterministic and randomized
  traces.

The multiset oracle verifies abstract double-ended-priority-queue semantics only;
it does not reuse alternating heap levels, grandparent bubbling, or
child/grandchild trickle-down logic.

## Scope

Exactly three new recovery paths are intended:

- `include/algorithms/data_structures/min_max_heap.hpp`;
- `tests/test_min_max_heap_cases.hpp`;
- `docs/scope_recovery_min_max_heap.md`.

The isolated recovery-test architecture auto-enrolls the case header after CMake
reconfiguration. No CMake, test-main, README, historical ROADMAP, workflow,
benchmark, frozen compiler/backend, or temporary-file change is required.

Base: `1e38302bdf6bcf2e8f488e3c35a5bb217bf606de`.
