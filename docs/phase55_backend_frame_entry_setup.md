# Phase 55 — target-neutral stack-directed frame-entry setup

## Executable boundary

Phase 53 sealed a logical byte anchor plus signed frame-base displacements, and
Phase 54 gave that logical base a dedicated physical register. Phase 55 closes
the remaining entry-time coordinate gap for a fixed-size frame without choosing
an ISA, ABI, or concrete stack-pointer register.

`derive_stack_directed_backend_frame_entry` consumes a sealed Phase-54 plan and
one explicit stack-growth direction. It preserves the complete Phase-54 result
as replay provenance and, for a feasible addressed frame, derives:

- the signed stack-pointer adjustment from function entry to the allocated frame;
- the dedicated frame-base register's signed offset from the adjusted stack pointer;
- the same base register's signed offset from the incoming stack pointer; and
- one direct incoming-stack-pointer displacement for every sealed frame slot.

Phase-54 register shortage and the zero-register hardware case remain normal
results: no entry setup is produced and no new exception is introduced.

## Coordinate invariant

Let `F` be the sealed frame size, `A` the Phase-53 logical base anchor, `O` one
slot byte offset, and `d = O - A` the sealed Phase-53 frame-base displacement.

For storage growing toward lower addresses:

- adjusted stack pointer: `entry_SP - F`;
- base from adjusted SP: `+A`;
- base from entry SP: `A - F`;
- slot from entry SP: `O - F`.

For storage growing toward higher addresses:

- adjusted stack pointer: `entry_SP + F`;
- base from adjusted SP: `A - F`;
- base from entry SP: `+A`;
- slot from entry SP: `+O`.

Every result verifies both coordinate compositions directly:

`stack_adjustment + base_from_adjusted_SP == base_from_entry_SP`

and, for every frame slot,

`base_from_entry_SP + d == direct_slot_from_entry_SP`.

Thus stack direction changes the entry/adjusted-SP coordinate system without
changing any sealed Phase-53 slot location.

## Validation and checked arithmetic

The transformation validates the Phase-54 ownership id, non-base register
count, byte/addressed-frame presence, frame size/layout correspondence, frame
anchor, and each slot's byte geometry plus signed Phase-53 displacement before
using them. Malformed sealed-plan shape is rejected as an internal logic error.

All size-to-signed conversions and additions are checked. A negative magnitude
of exactly `INT64_MAX + 1` is accepted as `INT64_MIN`; larger negative
magnitudes and positive magnitudes above `INT64_MAX` are rejected. Consequently
a downward frame allocation of exactly `2^63` bytes is representable as
`INT64_MIN` on a 64-bit `size_t` platform, while the corresponding upward
positive adjustment is not.

## Verification

Deterministic tests cover zero-register and register-shortage pass-through,
nonzero spill frames in both growth directions, repeated deterministic replay,
malformed Phase-54 witnesses, unknown direction rejection, and signed boundary
behavior.

A fixed-seed 300-program corpus is generated through the real sealed Phase-54
planner. An independent coordinate oracle recomputes the stack adjustment,
base offsets, and each direct slot displacement from `(F, A, O)` and checks both
composition identities for lower- and higher-growing storage.

Strict GCC/Clang and sanitizer-focused arithmetic checks are precursor evidence;
exact-head full-repository CI is the authoritative integration gate.

## Complexity and non-claims

For `S` frame slots the new transformation is `O(S)` time and `O(S)` additional
result storage, on top of the already-computed sealed Phase-54 plan.

This phase deliberately does not choose a concrete stack-pointer register,
stack-pointer/base-register ISA instructions, encoded addressing modes,
calling convention, callee/caller-save policy, red zone, prologue/epilogue
encoding, unwind metadata, variable-size objects, or stack-slot reuse. It is a
target-neutral fixed-frame coordinate contract, not an ABI implementation.

## Phase boundary

Phase 55 is sealed at `main@1e10f30cfffd347f74dea8220bf98e225bda50c6` after
merged-main CI run `34420412273` completed successfully on GCC release, Clang
release, and GCC ASan+UBSan. The coordinate layer is therefore integrated and
no further stack-direction or signed-coordinate variant is required in this
phase.

The next coherent ownership gap is the still-symbolic stack pointer. Phase 56
should reserve one concrete physical-register id for the stack pointer from the
same caller-supplied finite register file, rerun the sealed Phase-54 frame-base
reservation over the remaining prefix, and derive the sealed Phase-55 entry
coordinates from that nested plan. A successful result must prove that stack
pointer, frame base, persistent assignments, and spill-scratch registers are
pairwise disjoint while retaining complete replay provenance. Concrete ISA
instructions, ABI/calling-convention rules, save/restore policy, unwind data,
and prologue/epilogue encoding remain outside that slice.

`ROADMAP.md` remains presentation-only historical state for this late backend
sequence; the phase proof and sealing audits are authoritative for promotion.