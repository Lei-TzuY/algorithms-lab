# Scope recovery: bounded-universe van Emde Boas ordered set

## Capability

This recovery slice adds a first-principles ordered set over an explicitly bounded
power-of-two universe. `VanEmdeBoasSet(bits)` accepts `bits` in `[1,16]`, so the
configured universe is exactly `[0, 2^bits)`. The public contract provides set
semantics (`insert`, `erase`, `contains`), constant-time minimum/maximum access,
and strict predecessor/successor queries.

Duplicate insertion and erasure of an absent key are deterministic no-ops that
return `false`. Every key-taking operation rejects values outside the configured
universe. The implementation is intentionally a bounded-universe baseline: it
does not claim arbitrary-width keys, multiset multiplicities, sparse allocation,
y-fast/x-fast trie behavior, or dynamic universe resizing.

## Representation and invariants

Every recursive node represents a power-of-two local universe. A non-empty node
stores its exact local minimum and maximum directly. For universes larger than
two elements, the remaining values are split into a high cluster index and a low
offset. The high/low mapping and `combine(high, low)` are inverse on the local
universe.

The key structural invariant is:

- `minimum` / `maximum` are exactly the extrema of the represented set;
- except for the node minimum, every stored value is represented recursively in
  exactly one cluster;
- the summary contains exactly the indices of non-empty clusters;
- an empty cluster is absent from the summary, and a non-empty cluster is present
  exactly once.

Insertion swaps a new smaller value into `minimum`, then recursively stores the
previous minimum remainder. If an empty cluster receives its first value, its
index is inserted into the summary before the local value becomes that cluster's
minimum/maximum.

Erasure preserves the same invariant in reverse. When deleting the node minimum,
the replacement is reconstructed from the smallest non-empty summary cluster and
that cluster's minimum. When a cluster becomes empty, its index is erased from the
summary; maximum is then reconstructed either from the final non-empty cluster or
from the node minimum when no cluster remains.

For successor/predecessor, the algorithm first checks whether the answer can lie
inside the key's current cluster. Otherwise it asks the summary for the next or
previous non-empty cluster and combines that cluster id with the corresponding
cluster extremum. If no such cluster exists, the node minimum/maximum boundary
handles the remaining case. This gives strict successor/predecessor semantics.

## Complexity and space boundary

Let the universe size be `U = 2^bits`, with `1 <= bits <= 16`.

This implementation deliberately prebuilds the complete recursive cluster tree.
Construction time and resident storage are therefore `O(U)`. That choice removes
allocation from normal set operations but is not the sparse-space variant often
associated with practical predecessor indexes.

Each recursive operation reduces the universe bit width roughly by half, so
`insert`, `erase`, `contains`, `predecessor`, and `successor` use `O(log log U)`
recursive levels. `minimum` and `maximum` are `O(1)`.

No y-fast-trie `O(n)`-space claim, hashing bound, word-RAM predecessor lower/upper
bound, or larger-than-16-bit universe claim is implied.

## Verification

Focused repo-style verification passed before upload under:

- GCC C++20 with repository strict warnings-as-errors: 4/4 tests;
- Clang C++20 with the same strict warnings: 4/4;
- actual GCC ASan+UBSan (`-fsanitize=address,undefined`): 4/4.

Deterministic tests cover invalid universe widths, key-bound rejection, the base
`U=2` recursion case, duplicate insertion, absent erasure, exact minimum/maximum,
strict predecessor/successor, the full supported `U=65536` boundary, and repeated
minimum/maximum replacement during deletion.

The primary randomized oracle is independent `std::set<uint32_t>` behavior. A
fixed-seed 20,000-operation trace over `U=256` mixes insert, erase, membership,
predecessor, successor, and extrema queries. After every operation it compares
size/emptiness/minimum/maximum, and periodically scans additional query keys for
membership plus predecessor/successor equality.

## Scope recovery context

A fresh live-history audit after proposer-optimal stable matching found no van
Emde Boas, x-fast/y-fast, or other bounded-universe predecessor/successor index in
the repository. This deliberately changes proof model again after stable matching,
maximum clique, matroid intersection, suffix automata, Mo ordering, and Li Chao
line envelopes. The Phase-45–69 compiler/backend surface remains frozen.

This slice follows the post-Phase-69 recovery rule: add one coherent missing
algorithm/data-structure capability with an explicit proof boundary and independent
verification. It intentionally leaves the historically stale `ROADMAP.md` and
README untouched.
