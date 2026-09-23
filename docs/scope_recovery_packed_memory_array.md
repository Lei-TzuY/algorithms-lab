# Scope recovery: hierarchical packed-memory sequence

## Coverage decision

After a fresh live-state audit at
`main@a6bc1adbfb483cb20b59bc65ad16babfb1a83173`, the repository had zero
open pull requests and zero open issues.

The same audit found no packed memory array / packed-memory sequence
implementation, branch, or prior implementation pull request. Nearby structures
such as B-trees, treaps, ropes, persistent sequences, predecessor structures,
and hash tables use different representation and update invariants.

This slice therefore adds a distinct dynamic-sequence proof model: a
power-of-two gapped array, aligned density windows, local order-preserving
redistribution, and whole-root growth/shrink.

Prospective work remains governed by
`docs/scope_recovery_after_phase69.md`; the frozen historical
compiler/backend frontier is not resumed.

## Public contract

`PackedMemoryArraySequence` stores an ordered sequence of signed 64-bit values.

The public operations are:

- default construction of an empty sequence;
- construction from an existing ordered span/vector;
- `size()`, `empty()`, and physical `capacity()`;
- exact rank lookup `at(rank)`;
- `insert(rank,value)`, where `rank` may equal the current size;
- `erase(rank)`, returning the removed value;
- `to_vector()`, replaying the logical sequence in order;
- `valid_structure()`, an intentionally expensive structural diagnostic.

Values need not be sorted or unique. Duplicate values and the complete
`int64_t` value range are ordinary payloads.

Invalid ranks are rejected with `std::out_of_range`.

## Representation

The physical slot array has:

- minimum capacity 16;
- power-of-two capacity;
- leaf width 8;
- optional/gap slots between logical values.

Every occupied slot encountered from left to right is one logical sequence
element. Rebalancing changes physical positions only; it never changes logical
relative order.

Aligned hierarchy windows have widths

`8, 16, 32, ... , capacity`.

For a window of width `w`, the structural upper occupancy limit is

`floor(3w/4)`.

The implementation also uses

`ceil(w/4)`

as a deletion compaction / root-underfill trigger. The lower trigger is not
presented as a universal density invariant for every window.

## Insertion invariant

Before insertion, the root is enlarged if adding one element would exceed the
root's 75% upper limit.

For the physical path containing the insertion boundary, production examines
every aligned hierarchy window after hypothetically adding one element.

Let `H` be the highest window on that path whose post-insert occupancy would
exceed its upper limit.

- if no path window would overflow, the leaf window is redistributed;
- otherwise the parent of `H` is redistributed;
- root overflow is impossible after the root-capacity precheck and is treated as
  an internal invariant failure.

Why is the parent sufficient?

- windows strictly above that parent are unchanged in occupancy and were not
  over-dense;
- the selected parent itself is not over-dense, by maximality of `H`;
- every selected-parent descendant is rebuilt by the even-placement routine;
- windows outside the selected parent are untouched.

The inserted logical rank is converted to a local rank by counting occupied
slots before the selected physical segment. The segment's existing logical
values are collected, the new value is inserted at that local rank, and the
complete segment is redistributed without reordering.

## Even placement

A segment of width `w` containing `k < w` logical values is redistributed
with an integer digital-differential schedule:

- base step `floor(w/k)`;
- residual `w mod k`;
- residual extra slots spread across the sequence rather than accumulated at
  one end.

This keeps the occupied positions monotone and distributes them as evenly as the
integer slot grid permits.

For every segment population used by production, `k <= floor(3w/4)`.
The balanced redistribution therefore keeps aligned descendants at or below the
same 75% occupancy ceiling.

Production retains a defensive full hierarchy check after local mutation; if an
unexpected rounding/invariant violation were ever detected, it performs a
whole-root redistribution rather than silently weakening the density contract.

## Erase / compaction

Erasing a logical rank clears exactly its physical occupied slot, so deletion
cannot directly violate an upper-density bound.

After removal:

1. an empty sequence resets to minimum capacity;
2. while the root is below its 25% underfill trigger and the next smaller root
   can still store the sequence below 75%, capacity is halved;
3. otherwise the mutation path is examined for sparse aligned windows;
4. the parent of the highest sparse path window is redistributed, or the root
   itself at minimum-capacity underfill.

This local compaction is deterministic and order preserving.

The 25% threshold is a compaction policy, not a claimed textbook PMA amortized
threshold schedule.

## Structural diagnostic

`valid_structure()` checks:

- capacity is a power of two;
- capacity is at least 16 and divisible by the leaf width;
- physical occupied-slot count equals public logical size;
- root occupancy does not exceed 75%;
- every aligned hierarchy window satisfies the 75% upper-density invariant.

It deliberately does not claim that every aligned window has a lower-density
bound.

## Complexity boundary

Let `C` be the current physical capacity and `N <= C` the logical size.

This implementation prioritizes an explicit, replayable density proof over the
classical optimized PMA performance envelope.

- `at(rank)`: `O(C)` worst case because rank-to-physical lookup scans gaps;
- `to_vector()`: `O(C)`;
- local collection/redistribution: at most `O(C)`;
- hierarchy-path occupancy inspection: `O(C)` total scanned slots across the
  geometrically increasing path windows;
- defensive full hierarchy validation: `O(C log C)`;
- therefore `insert` and `erase` are conservatively
  `O(C log C)` worst case in this baseline;
- resident storage is `O(C)`.

No classical `O(log^2 N)` amortized PMA insertion claim is made. Achieving
that bound would require a more specialized rank/search directory and
level-varying density schedule than this exact baseline implements.

## Independent verification

The committed primary oracle is `std::vector<int64_t>`.

For each tested mutation, the same rank insertion or erase is applied to the
vector and the packed sequence. Verification compares:

- exact logical size;
- exact full `to_vector()` replay;
- structural validity;
- selected rank lookups;
- periodic all-rank lookup replay.

Committed deterministic coverage includes:

- empty sequence and invalid-rank behavior;
- construction from an existing sequence;
- front/middle/back insertion;
- duplicates and `INT64_MIN` / `INT64_MAX`;
- exact returned erase values;
- growth through 1,500 updates and shrink back to minimum capacity;
- 600 adversarial front/middle updates with periodic removals;
- 3,500 fixed-seed randomized insert/erase steps against `std::vector`;
- every randomized step checks exact full logical replay and structural
  invariants, while every 113th step checks every logical rank individually.

Supporting model-level verification additionally:

- exhaustively checked the integer even-placement schedule for power-of-two
  windows through 512 slots and every legal occupancy up to 75%, finding zero
  aligned-child upper-density violation;
- replayed 36,000 additional randomized insert/erase mutations against a
  vector model with zero order or density mismatch.

Those model checks are supplementary evidence only; repository GCC, Clang, and
sanitizer CI remain the integration gate.

## Non-claims

This slice does not claim:

- sorted-set search semantics;
- uniqueness;
- stable physical addresses;
- lock-free or concurrent updates;
- persistence;
- cache-miss benchmarks;
- cache-oblivious optimality;
- textbook level-varying PMA thresholds;
- textbook optimal PMA amortized update bounds;
- benchmark-backed speedups over `std::vector` or tree structures.

## Scope

Exactly three recovery paths are intended:

- `include/algorithms/data_structures/packed_memory_array.hpp`;
- `tests/test_packed_memory_array_cases.hpp`;
- `docs/scope_recovery_packed_memory_array.md`.

Pre-PR hardening includes:

- bounding randomized all-rank replay frequency so sanitizer runtime is not
  dominated by redundant linear scans;
- removing an unreachable second root-growth path after the insertion
  precondition already guarantees root capacity.

No CMake, README, historical ROADMAP, workflow, benchmark, frozen
compiler/backend, or temporary-file change is required.

Exact repository GCC release, Clang release, and GCC ASan+UBSan CI on the final
pull-request head are required before integration.

Base: `a6bc1adbfb483cb20b59bc65ad16babfb1a83173`.
