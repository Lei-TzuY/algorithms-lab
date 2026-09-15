# Scope recovery: meldable pairing heap

## Coverage decision

A fresh live-state audit after the exact slope-trick checkpoint found no pairing
heap / meldable pairing-heap implementation in default-branch code, pull-request
history, or branch names. Soft heap also remains uncovered, but implementing an
exact binary heap with a cosmetic corruption flag would not constitute a soft
heap and would be rejected as a false capability claim. This slice instead adds
a standard first-principles two-pass pairing heap: a genuine meldable priority
queue with a different structural and amortized proof model from the existing
array-backed binary heap.

Prospective authority remains `docs/scope_recovery_after_phase69.md`; historical
`ROADMAP.md` drift and the frozen compiler/backend sequence are untouched.

## Production contract

`algorithms::data_structures::PairingHeap<T, Compare>` uses the same comparator
semantics as the Phase-1 binary heap: `Compare(a,b)` means `a` has higher
priority than `b`. The public surface provides:

- `push`, `top`, and `pop`;
- destructive `meld(PairingHeap&&)`, leaving the donor empty;
- move construction / assignment with correct donor size state;
- explicit size/empty queries and a structural heap-order diagnostic;
- min-heap behavior by default and custom comparator support.

Copying is intentionally disabled. Meld requires equivalent ordering semantics
between the two comparator instances; equivalence of stateful comparators is a
caller precondition and is not dynamically checked. The structural operations
assume comparison and element-move operations do not throw; no strong exception
safety claim is made for user-defined throwing comparators or throwing `T` moves.
Handles and `decrease_key` are intentionally outside this baseline.

## Structural invariant and delete-min proof obligation

Each node stores its children as a left-child / next-sibling list. For every
parent-child edge, the child may not have higher priority than the parent.
Melding two roots compares only their keys: the lower-priority root becomes the
new first child of the higher-priority root, preserving heap order in O(1).

`pop` removes the root and performs the standard two-pass pairing discipline:
first pair adjacent root children from left to right and meld each pair; then
meld the resulting roots from right to left. Every meld preserves heap order, so
the rebuilt root is again globally minimal/maximal under `Compare`. The
implementation reserves first-pass storage before detaching the existing tree so
vector allocation failure cannot partially detach the heap.

Ownership uses `unique_ptr`, but whole-heap destruction is iterative rather than
recursive. This matters because monotone insertions can create an O(n)-deep
first-child chain; a recursive ownership teardown would turn an otherwise valid
heap into a stack-depth hazard.

## Verification

Deterministic tests cover empty-operation rejection, duplicates, sorted pop
order, destructive meld, self-meld, move construction/assignment, custom max
ordering, and a 20,001-node deep-chain destruction regression.

The primary differential oracle uses eight independent `std::multiset<int>`
instances. Eight thousand fixed-seed mixed operations perform inserts, pops, and
destructive melds between random heaps. After every operation, affected heaps
are checked for size/emptiness/top equality and the production structural
invariant; periodic full sweeps validate all heaps. The standard library is used
only as a testing oracle, not as the implementation under study.

Focused candidate bytes pass GCC and Clang under the repository strict warning
set and pass an actual GCC ASan+UBSan build. Exact remote-head full repository CI
remains the merge gate.

## Complexity and non-claims

This concrete two-pass pairing heap provides O(1) worst-case `top`, `push`, and
root meld. A single `pop` may touch O(n) roots; the standard two-pass pairing
heap has O(log n) amortized delete-min. `valid_structure` is O(n) and exists for
verification rather than the hot path. Storage is O(n).

This slice does not implement handles, decrease-key, persistence, arbitrary
comparator-equivalence checks, soft-heap key corruption, Chazelle soft-heap
bounds, or benchmark-backed performance claims. In particular, it does not
claim to close the separate soft-heap coverage gap.
