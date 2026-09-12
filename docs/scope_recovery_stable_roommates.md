# Scope recovery: exact stable roommates via Irving rotations

## Capability

This recovery slice adds the classical stable-roommates problem for one set of
participants with complete strict preferences over every other participant. It
is intentionally distinct from the sealed bipartite Gale-Shapley capability:
a stable perfect roommate pairing need not exist, and the exact solver uses
Irving's two-phase stable-table reduction plus exposed-rotation elimination.

The public result is `std::optional<StableRoommatesResult>`. A present result
contains the symmetric partner map, the number of Phase-1 proposals, and the
number of exposed rotations eliminated. `nullopt` means no stable perfect
matching exists under the complete strict preferences. Empty input has the
empty stable matching; non-empty odd cardinality has no perfect matching.

Preference validation is strict: each row must list every *other* participant
exactly once, with no self entry, duplicate, or out-of-range id.

## Invariants and proof obligations

Phase 1 maintains directed semi-engagements. A free participant proposes to the
first remaining participant on its reduced list. A recipient keeps the best
proposal seen so far and immediately deletes every worse successor symmetrically.
If such a deletion breaks the recipient's own outgoing semi-engagement, that
participant becomes free and proposes again. Thus accepted proposals only
improve at a recipient, and every deleted pair is excluded from every stable
matching represented by the resulting stable table.

If Phase 1 leaves a stable table with lists longer than one, Phase 2 repeatedly
finds an exposed rotation. Starting from a participant `x_i`, let the previous
second choice be `y_{i+1}` and let `x_{i+1}` be the last remaining participant
on `y_{i+1}`'s list. Repetition exposes a cycle. Eliminating it removes each
`x_i`'s previous first choice, promotes the previous second choice, and removes
all successors of `x_i` from that new recipient's list. If any list empties, no
stable matching exists. If every list reaches length one, symmetric singleton
lists define a stable perfect matching.

Correctness relies on Irving's stable-table and rotation-elimination theorems:
Phase 1 preserves existence, every non-singleton stable table exposes a rotation,
and eliminating an exposed rotation preserves at least one stable matching when
one exists. Tests provide implementation evidence rather than replacing these
theorems. Production additionally replays the final partner witness and fails
closed if it contains a blocking pair.

## Verification

Deterministic cases cover empty and two-person solutions, odd cardinality,
malformed preference tables, a six-person instance that requires an exposed
rotation, repeated determinism, and a six-person instance independently proved
to have no stable matching.

The primary randomized oracle is structurally independent of Irving's algorithm.
For 1,200 fixed-seed complete strict instances with 2, 4, 6, or 8 participants,
it recursively enumerates every perfect pairing and directly scans all unmatched
pairs for a blocking pair. Production existence must equal exhaustive existence;
every returned witness is independently replayed for symmetry and stability.
A larger 10,000-instance focused stress corpus was also run before upload.

The first focused implementation was rejected by this oracle on an eight-person
counterexample. That failure exposed two real reduction bugs: Phase-1 successor
deletions were deferred incorrectly, and Phase-2 rotation elimination removed
only the immediate rejected pair instead of restoring the full successor
invariant. The corrected candidate passed GCC and Clang strict warnings-as-errors
plus actual GCC ASan+UBSan with leak detection.

## Complexity and non-claims

The classical Irving algorithm admits an `O(n^2)` implementation with carefully
maintained list pointers. This educational baseline deliberately stores an
`n x n` active-pair table and rescans preference rows for first/second/last and
list-size queries. Because there can be `O(n^2)` pair deletions/rotations and an
individual scan costs `O(n)`, the implementation claims the conservative direct
bound `O(n^4)` time and `O(n^2)` auxiliary/result storage rather than importing
the optimized textbook bound.

Ties, incomplete lists, maximum-cardinality stable roommates with unmatched
participants, all-stable-matching enumeration, rotation-poset construction, and
weighted objectives are not claimed. This slice does not modify the historical
ROADMAP, recovery authority, compiler/backend surface, or the sealed bipartite
stable-matching implementation.
