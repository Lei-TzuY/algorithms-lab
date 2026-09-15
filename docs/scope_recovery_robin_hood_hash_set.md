# Scope recovery: Robin Hood open-addressing hash set

## Coverage decision

A fresh live audit after the exact single-word Myers checkpoint found no Robin
Hood hash-table implementation in default-branch code or pull-request history,
and no branch occupying a Robin-Hood recovery surface. The sealed cuckoo hash
set solves a different displacement/rebuild problem; quotient filters provide
approximate membership rather than an exact mutable set. Live HyperLogLog,
partially-persistent DSU, minimum-cycle-basis, ROBDD, and LZ77/LZ78 surfaces are
left untouched.

Prospective work remains governed by `docs/scope_recovery_after_phase69.md`;
the frozen compiler/backend ROADMAP tail is not resumed.

## Production contract

`RobinHoodHashSet64` is a unique signed-64 exact set with deterministic replay:

- power-of-two open-addressing table, minimum capacity eight;
- deterministic SplitMix-style fixed-width mixer parameterized by an explicit
  replay seed;
- insertion uses Robin Hood swaps when the incoming probe distance exceeds the
  resident distance;
- lookup uses the Robin Hood early-termination rule;
- erasure uses backward shifting and stores no tombstones;
- growth occurs before crossing a 7/8 resident-load boundary and rebuilds into a
  temporary doubled table before committing;
- exact sorted-value and complete physical-slot diagnostics;
- `valid_structure()` replays probe distances, home-to-slot reachability,
  uniqueness, size accounting, and public lookup reachability.

The mixer is deterministic and non-cryptographic. Same seed plus the same
successful mutation trace replays the same physical layout. No adversarial-hash
or universal-hash guarantee is implied.

## Structural proof obligation

For a resident key at physical slot `i`, the stored probe distance equals the
circular distance from its home bucket to `i`, and every earlier slot on that
probe path is occupied. During insertion, swapping a lower-distance resident
with a higher-distance incoming key preserves both keys' legal linear-probe
paths while giving priority to the key that has travelled farther.

For lookup at current probe distance `d`, encountering an empty slot proves the
key was never inserted past that hole. Encountering a resident with distance
less than `d` also proves absence: an insertion of the searched key reaching
that position would have displaced that resident under the Robin Hood rule.

On erase, shifting consecutive residents backward while their distance is
positive closes exactly the deleted hole and decrements each moved resident's
home distance by one. Stopping at an empty slot or distance-zero resident cannot
break any later key's probe path. Thus tombstones are unnecessary.

Growth uses a temporary table and commits only after all resident keys have been
reinserted, so allocation failure before the table swap leaves the live table
unchanged. No general strong exception-safety claim is made for process-wide
resource failure after successful allocation because stored keys themselves are
plain `int64_t` and the remaining relocation operations are non-throwing.

## Verification

Focused exact bytes use the repository test framework and passed:

- GCC C++20 strict warnings-as-errors;
- Clang C++20 strict warnings-as-errors;
- actual GCC ASan+UBSan.

Evidence covers signed-64 boundary keys, duplicate/absent operations,
deterministic same-seed layout replay, a six-key same-home collision cluster,
backward-shift deletion in the middle and at the head of that cluster, repeated
capacity growth, and a 30,000-operation fixed-seed insert/erase/contains trace
against an independent `std::set<int64_t>` oracle. Structural diagnostics and
exact sorted contents are replayed throughout the randomized trace.

## Complexity and non-claims

For table capacity `C`, lookup, insertion, and deletion perform `O(C)` direct
work in the worst case; one growth rebuild performs `O(C)` resident-table work
plus probe work for reinsertion. Resident storage is `O(C)`. The concrete
SplitMix-style mixer is deterministic but is not claimed to be a universal hash
family, so this baseline deliberately makes no adversarial or expected-O(1)
operation claim and no benchmark-speedup claim.

This slice does not add heterogeneous keys, generic hashing, stable references,
iterators, shrinking, concurrency, persistence, hopscotch hashing, or a library
replacement abstraction.
