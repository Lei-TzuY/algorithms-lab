# Scope recovery: persistent Hood-Melville real-time queue

## Coverage decision

A fresh live-state audit after online bridge connectivity merged as
`main@bc2b7d814c1d2359006c33b570843a20977bd4ba` found zero open pull
requests and zero open issues. Default-branch code, branch names, commit search,
and pull-request history contained no Hood-Melville / real-time queue
implementation.

The repository already contains ordinary queue use inside algorithms, a
partially-retroactive FIFO recovery surface, a persistent finger tree, and
several balanced ordered structures. This slice changes proof model instead of
adding another ordered-set variant: it recovers a purely functional FIFO whose
rear reversal is deamortized into an explicit schedule, while every update also
creates a branching-persistent version.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; the frozen compiler/backend history and
stale historical ROADMAP are untouched.

## Production contract

`algorithms::data_structures::RealTimeQueue` stores signed 64-bit values.

- the empty queue supports `empty()` and `size()`;
- `front()` returns the oldest resident value and rejects an empty queue;
- `push_back(value)` returns a new queue version;
- `pop_front()` returns a new queue version and rejects an empty queue;
- source versions remain immutable and may be reused to create arbitrary
  branching histories;
- `last_rotation_work()` reports the number of nontrivial Hood-Melville
  schedule transitions executed by the update that produced that version;
- every public update performs at most two such schedule transitions;
- `rotation_active()` and `valid_invariants()` expose diagnostic state.

The structure deliberately does not provide random access, concatenation, deque
operations, destructive mutation, lock-free concurrency, serialization, or a
custom allocator.

## Functional representation

Persistent list nodes are immutable and shared through `shared_ptr<const
Node>`. A queue version contains:

- a logical front length and current front list;
- a logical rear length and rear list stored in reverse insertion order;
- one explicit rotation state;
- the diagnostic work count for the update that produced the version.

Copying a queue version therefore copies only scalar counters, the rotation
variant, and shared list handles. Existing list nodes are never modified.

## Hood-Melville rotation state

A rear reversal is not performed in one linear burst.

`Reversing(ok, f, f_rev, r, r_rev)` incrementally consumes one front node and
one rear node per schedule step, building reversed partial lists.

When the old front is exhausted, exactly one old rear node remains. The state
moves to `Appending(ok, f_rev, r_rev)`; each schedule step then copies one
still-live old-front value onto the completed candidate front.

`Done(new_front)` publishes the completed front list, after which the public
queue state returns to `Idle`.

The `ok` counter records copied old-front entries that have not been invalidated
by intervening `pop_front` operations. It is stored as `size_t`; invalidating
a reversing state with zero live copied entries is treated as an internal
invariant violation rather than permitting signed underflow.

## Scheduling invariant

Before an update returns, `check()` executes exactly two calls to the rotation
executor. Idle/done states may consume no work; every nontrivial executor
transition performs only a constant number of immutable-list head/tail/cons
operations.

When the rear grows larger than the logical front, the rear is necessarily only
one element larger because the previous public state satisfied
`rear_size <= front_size`. A new rotation therefore starts with old rear length
equal to old front length plus one, which is exactly the Hood-Melville reversing
shape.

A pop invalidates one scheduled old-front element before its two executor steps.
The schedule rate is sufficient to finish and install the new front before the
old public front can be exhausted.

`last_rotation_work() <= 2` is checked continuously by committed tests. The
claimed real-time bound concerns the algorithmic number of list/schedule
operations; it is not a wall-clock, allocator-latency, cache, or operating-system
latency guarantee.

## Persistence / exception boundary

`push_back` and `pop_front` begin from a value copy of the queue metadata.
All newly allocated list nodes belong only to the candidate returned version.
If node allocation throws, the source queue and every other historical version
remain unchanged.

Because resident nodes are immutable, publishing a successful new version
requires no mutation of old versions. This gives a natural strong semantic
boundary for allocation failure.

## Complexity / non-claims

Under the standard persistent-list cost model where head, tail, shared-handle
copy, and cons are constant-time primitives:

- `front`, `empty`, and `size`: `O(1)`;
- `push_back` and `pop_front`: worst-case `O(1)` algorithmic queue work,
  including at most two nontrivial rotation transitions;
- one update allocates only a constant number of new list nodes;
- each queue version occupies `O(1)` metadata in addition to structurally
  shared immutable nodes.

No claim is made that `shared_ptr` allocation/deallocation itself has a
hard real-time wall-clock bound. No benchmark, lock-free, wait-free, custom
memory-reclamation, or universally-faster-than-`std::deque` claim is made.

## Independent verification

The committed primary oracle stores one ordinary `std::deque<int64_t>` for
every generated queue version.

Deterministic coverage includes:

- empty-query/update rejection;
- thousands of FIFO pushes and pops that repeatedly overlap rotations;
- direct branching from old versions while newer descendants already exist;
- replay of complete queue contents by draining a copied production version
  against its independent deque state.

The randomized corpus creates 6,000 branching versions. Every step chooses an
arbitrary historical version, performs either a push or pop, stores a new
production version and an independently updated `std::deque`, and checks:

- size / empty / front agreement;
- structural diagnostics;
- `last_rotation_work() <= 2`;
- periodic complete sequence replay for both the newest and randomly selected
  historical versions.

The deque oracle does not use Hood-Melville rotation state, immutable lists,
schedule counters, or production invariants.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

## Scope

Exactly three new recovery paths are intended:

- `include/algorithms/data_structures/real_time_queue.hpp`;
- `tests/test_real_time_queue_cases.hpp`;
- `docs/scope_recovery_real_time_queue.md`.

The isolated recovery-test architecture auto-enrolls the case header after CMake
reconfiguration. No CMake, test-main, README, historical ROADMAP, workflow,
benchmark, frozen compiler/backend, or temporary-file change is required.

Base: `bc2b7d814c1d2359006c33b570843a20977bd4ba`.
