# Scope recovery: partially retroactive FIFO queue

## Coverage decision

A fresh live audit at `main@ef0783486cd99b8f6ca62288cdef178d1cbfbd42`
found no retroactive / partial-retroactive data-structure implementation in the
default branch, pull-request history, or branch namespace. The repository already
contains persistent/versioned structures, rollback DSU, offline dynamic
connectivity, dynamic forests, and many priority-queue families, but those expose
different time/state contracts. This slice therefore adds a new temporal data-
structure proof model instead of extending the immediately preceding isotonic-
regression or distance-transform surfaces.

Prospective scope remains governed by `docs/scope_recovery_after_phase69.md`;
the frozen Phase-45--69 compiler/backend history and stale historical ROADMAP are
untouched.

## Production contract

`PartialRetroactiveQueue` stores a FIFO operation history keyed by unique
`uint64_t` timestamps. It supports:

- retroactive insertion of `enqueue(value)` at an arbitrary unused timestamp;
- retroactive insertion of `dequeue` at an arbitrary unused timestamp;
- retroactive erasure of an existing operation;
- present-time `size`, `empty`, `front`, and `back` queries;
- timestamp-presence and expensive structural diagnostics.

Every history must be queue-valid: scanning events in timestamp order may never
execute a dequeue on an empty queue. An edit that would violate that condition is
rejected transactionally. Values use the full signed-64 domain. The default
history limit is 1,000,000 events, and callers may request a smaller limit for
bounded applications/tests; larger limits reject so all signed balance metadata
has an explicit representability domain.

This is **partial** retroactivity: past operations may be inserted/erased, while
queries ask only for the current queue state. It does not provide historical
state queries and makes no full-retroactivity claim.

## Augmented-AVL invariant

Production does not replay the operation sequence after an edit. Events live in
a first-principles AVL tree ordered by timestamp. Every subtree stores:

- height and node count;
- number of enqueue events;
- total queue-balance delta (`+1` enqueue, `-1` dequeue);
- the minimum non-empty prefix delta inside that subtree.

For concatenated chronological segments `A` then `B`, the summary composes as

`sum(A+B) = sum(A) + sum(B)`

and

`min_prefix(A+B) = min(min_prefix(A), sum(A) + min_prefix(B))`.

Therefore the root has a negative minimum prefix exactly when some historical
prefix dequeues from an empty queue. A newly inserted dequeue can be applied and,
if invalid, removed again without allocation. Before erasing an enqueue,
production combines the balance before its timestamp with the augmented suffix
summary; this proves whether every later prefix stays non-negative before any
mutation occurs.

AVL rotations recompute the same summaries, so a successful insert/erase keeps
height logarithmic and preserves the chronological monoid invariant.

## FIFO rank invariant

Let `E` be the total number of enqueue events and `D` the total number of dequeue
events in a valid history. FIFO semantics imply that exactly the first `D`
enqueues in chronological order have been consumed. The present queue is thus
the enqueue-rank interval `[D,E)`. Subtree enqueue counts support order-statistic
selection of rank `D` for `front` and rank `E-1` for `back`, without replaying
history.

## Complexity

For `m` stored events, AVL insertion, erasure, prefix/suffix aggregation, and
enqueue-rank selection are `O(log m)`. Therefore accepted past edits and current
`front/back` queries are `O(log m)`; current `size/empty/event_count` are `O(1)`.
The explicit `valid_structure()` diagnostic recursively replays AVL ordering,
height, metadata, and global prefix validity in `O(m)` and is intended for tests,
not normal operation.

No claim is made for full retroactivity, historical queue-state queries,
concurrency, duplicate timestamps, unbounded histories, or asymptotically
optimal retroactive lower/upper bounds.

## Independent verification

Focused final candidate passed repository-equivalent C++20 strict warnings under
GCC and Clang and an actual GCC ASan+UBSan build.

Deterministic cases cover edits in the past, FIFO resurrection after removing an
old dequeue, transactional rejection of an invalid retroactive dequeue and an
invalid enqueue erasure, duplicate/unknown timestamps, full signed-64 values,
full `uint64_t` timestamp endpoints, configurable event-limit rejection, and
4,096 monotonically inserted timestamps followed by adversarial erasures while
checking AVL height/metadata.

The primary randomized oracle is intentionally replay-based and exists only in
tests: 12,000 fixed-seed insert-dequeue/insert-enqueue/erase attempts over a
`std::map` timeline are validated by direct FIFO replay. After every attempted
edit, production must match the oracle's accept/reject decision, current size,
front/back values, event count, and structural invariant. The oracle therefore
does not reuse the production AVL augmentation or enqueue-rank recurrence.

Finite tests are implementation evidence. The universal `O(log m)` and
correctness claims come from the AVL balancing theorem plus the prefix-summary
and FIFO-rank arguments above.

## Scope

This recovery changes exactly four paths:

- `include/algorithms/data_structures/partial_retroactive_queue.hpp`;
- `tests/test_partial_retroactive_queue_cases.hpp`;
- one include registration in `tests/test_main.cpp`;
- this focused proof/coverage document.

No CMake, README, historical ROADMAP, recovery authority, workflow, benchmark,
frozen compiler/backend, or temporary-file surface is changed.
