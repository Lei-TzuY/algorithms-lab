# Phase 49 — Interference-Safe Copy Coalescing

Phase 49 consumes the sealed Phase-47 `OutOfSsaProgram` and the exact
Phase-48 may-liveness/interference substrate. It does not replace register
allocation and does not claim an optimal coalescing or coloring algorithm.

## Contract

`coalesce_phi_free_registers(program, register_budget)` first invokes the
sealed Phase-48 allocator to validate the phi-free program and reconstruct the
canonical location universe plus original interference graph. It then visits
scheduled entry/exit moves in deterministic block/kind/index order.

For each move preference, the current source and destination classes are:

- reported as `already_coalesced` when they are already one class;
- merged when no original interference edge crosses between the two classes;
- reported as `blocked_by_interference` otherwise.

The representative choice is deterministic. After all preferences are
processed, classes are numbered by the first canonical Phase-48 location they
contain. The quotient interference graph contains one edge for every pair of
classes connected by at least one original interference edge.

The quotient graph is recolored under the caller-supplied physical-register
budget with Phase 48's policy: descending quotient degree, deterministic tie
order, then the lowest available register. A class that cannot be colored is
spilled as a whole, and the class assignment is mapped back to every original
location.

Every scheduled move whose final endpoints belong to one class is returned as
a replayable redundant-copy witness with its block, entry/exit kind, and move
index.

## Safety invariant

Let `C(x)` be the final class containing original location `x`. The central
coalescing obligation is:

`(x, y) in original_interference => C(x) != C(y)`.

A merge is admitted only if no original interference edge currently crosses
between the two candidate classes. Merging can only union member sets, so once
an original edge crosses two classes those classes can never be legally
merged. Therefore no accepted sequence of preferences collapses an original
interference edge.

Quotient coloring then requires different physical registers on every assigned
quotient interference edge. Mapping that class color back to original
locations preserves Phase-48 interference safety. Spilled classes have no
physical register and every member is reported in the spill list.

## Determinism and witnesses

The result exposes:

- the original Phase-48 location/interference universe;
- every scheduled move preference and its merge/block decision;
- the final deterministic coalescing classes;
- quotient interference edges;
- mapped physical-register assignments and spills;
- every scheduled copy made redundant by the final classes.

This is enough for tests and callers to replay the structural safety argument
without inferring hidden allocator state.

## Verification

Deterministic regressions cover safe coalescing, a directly interfering move,
a transitive class whose later preference is blocked by interference, and a
zero-register budget that spills a whole merged class.

A fixed-seed randomized corpus generates 500 phi-free programs and verifies:

- classes partition the complete Phase-48 location universe;
- no original interference edge has both endpoints in one class;
- every quotient interference edge has a supporting original edge;
- assigned quotient neighbors never share a physical register;
- all members of one class share the same mapped assignment/spill state;
- every blocked scheduled move remains separated by class interference;
- every redundant-copy witness has coalesced endpoints;
- repeated execution is deterministic at the result-structure level.

Phase 48 remains the sealed source of liveness/interference truth. Phase 49 is
testing the quotient/coalescing layer on top of that substrate rather than
reimplementing liveness as a second production path.

## Complexity and non-claims

Let `L` be the number of original locations, `I` the number of Phase-48
interference edges, `P` the number of scheduled move preferences, and `C` the
number of final classes.

This first-principles baseline deliberately rescans the original interference
edge list for each attempted class merge, giving `O(P * I * alpha(L))` merge
checking with path-compressed class representatives. Quotient construction is
`O(I * alpha(L))`; deterministic coloring is bounded by the explicit quotient
adjacency work and ordered unavailable-register sets. Storage is `O(L + I + P)`
plus quotient adjacency/result witnesses.

No maximum-coalescing, George/Briggs optimality, minimum-register,
minimum-spill, spill-code insertion/rewrite, machine-opcode legality, MemorySSA,
or backend-wide optimality claim is made.

## Sealed boundary and next frontier

Phase 49 is sealed only after the implementation reached `main` and the exact
merged-main GCC release, Clang release, and GCC ASan+UBSan CI jobs all passed.
The sealed boundary is interference-safe coalescing plus quotient recoloring and
replayable redundant-copy witnesses; it does not silently add a backend machine
IR or executable spill code.

A fresh architecture audit promotes one distinct next frontier. Phase 50 should
introduce a machine-independent backend storage/rewrite representation: physical
register bindings for assigned classes, deterministic stack slots for spilled
classes, explicit load/store/register-move operations, and abstract spill-scratch
registers used only while materializing spilled operands. The lowering must
preserve phi-free block/operation order and eliminate only copies whose final
backend storage is identical.

Concrete ISA opcodes, addressing modes, calling conventions, byte-level frame
layout, mapping spill scratch registers onto real hardware registers, spill-cost
optimization, post-rewrite reallocation, MemorySSA, and backend-wide optimality
remain outside the sealed Phase-49 boundary and the promoted Phase-50 baseline.
