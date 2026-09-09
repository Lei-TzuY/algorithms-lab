# Phase 52 sealing audit

## Live integration evidence

Phase 52 implementation PR #121 used exact head
`83f58c66d9c3ca154cb8b68eb1dd8db910a9c245` over sealed Phase-51
`main@326c01b248d8fe856557b35c8eb8387b3845cb4f`.

PR CI run `34410335777` completed successfully on GCC release, Clang release,
and GCC ASan+UBSan. The PR was squash-merged as
`main@af316ff58300f51be00aa58844f52b5d55c32369`. Exact merged-main CI run
`34410865957` also completed successfully on all three jobs.

## Capability audit

Phase 52 closes the abstract spill-slot namespace left by Phases 50 and 51
without changing those sealed representations:

- every dense abstract stack-slot id maps to one deterministic byte offset;
- explicit slot size/alignment and whole-frame alignment are validated;
- slot placement and final frame-size arithmetic are checked for overflow;
- class storage, original-location storage, and every operation input/output use
  the same stack-slot-to-offset mapping;
- persistent and spill-scratch physical-register ids are preserved exactly;
- block reachability, original-block identity, operation kind, phi-free origin
  kind/index, and operation order remain replayable;
- successful construction contains no abstract spill-scratch registers and no
  abstract stack-slot ids in the new typed byte-addressed view.

Deterministic regressions cover nontrivial padding, zero-slot frames, invalid
size/alignment contracts, arithmetic overflow, and malformed Phase-51 scratch
witnesses. A fixed-seed 350-program corpus runs through the real sealed
Phase-49/50/51 pipeline and independently replays storage/provenance mapping,
slot alignment/non-overlap, frame alignment, and deterministic repeated layout.

## Claim boundary

The phase provides a target-neutral deterministic byte layout, not a concrete
machine frame. It makes no claim about stack-growth direction, a chosen stack or
frame-pointer register, signed base-relative displacement representation,
concrete load/store opcodes, ISA addressing-mode legality or encoding, caller-
or callee-saved registers, calling conventions, prologue/epilogue, red zones,
variable-size objects, slot coloring/reuse, or ABI-minimal frame size.

The direct construction remains `O(S + N)` time and `O(S + N)` result storage
for `S` stack slots and `N` class/location/operation storage references. No
stronger frame-optimization bound is claimed.

## Seal and promotion

Phase 52 is **SEALED** at
`main@af316ff58300f51be00aa58844f52b5d55c32369` with exact merged-main CI run
`34410865957` green.

The next coherent backend gap is representation-level addressing rather than
frame-size optimization. Phase 53 should lower Phase-52 `frame_byte_offset`
storage into a target-neutral signed base-relative displacement view. The caller
supplies a logical frame-base byte anchor and an allowed signed displacement
interval; production derives every memory displacement exactly with checked
arithmetic while preserving physical-register ids and all storage/operation
provenance. Malformed frames, out-of-frame anchors, and nonrepresentable or
out-of-policy displacements must be rejected. Concrete ISA opcode selection,
addressing-mode encoding, stack/base-register assignment, calling conventions,
prologue/epilogue, red zones, and ABI policy remain outside that slice.
