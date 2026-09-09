# Phase 52 — target-neutral byte-addressed spill-frame layout

## Executable boundary

Phase 50 deliberately represented spills as dense abstract `stack_slot` ids and
Phase 51 made every spill scratch temporary fit a finite physical-register suffix.
Phase 52 closes the remaining storage namespace gap without choosing a concrete
ISA or ABI. `layout_scratch_aware_backend_frame` consumes the selected Phase-51
plan and produces a separate byte-addressed view in which:

- persistent physical-register ids are unchanged;
- Phase-51 physicalized scratch-register ids are unchanged;
- every dense abstract stack slot receives one deterministic byte offset;
- class and original-location storage use the same slot-to-offset mapping;
- every reload/store operand in every physicalized operation uses the same
  byte offset;
- operation kind, origin kind/index, block reachability, and original-block
  provenance are preserved exactly;
- no abstract spill-scratch register or abstract stack-slot id remains in the
  new typed storage view.

The sealed Phase-50/51 objects are retained conceptually as replay evidence;
Phase 52 does not mutate or reinterpret their existing types.

## Layout contract and checked arithmetic

The caller supplies one uniform stack-slot size, one stack-slot alignment, and
one whole-frame alignment. Slot/frame alignments must be nonzero powers of two,
and frame alignment must be at least slot alignment. Slot `i` is laid out in
increasing dense slot-id order:

1. align the current cursor upward to slot alignment;
2. assign that aligned cursor as slot `i`'s byte offset;
3. advance by the explicit slot size with checked `size_t` arithmetic.

After the last slot, frame size is aligned upward to frame alignment. Empty
spill state therefore has frame size zero. Every overflow in alignment or size
arithmetic is rejected rather than wrapped.

Because slot ids are already dense and deterministic from Phase 50, this is a
stable layout for fixed `(selection, config)`; it is not a frame-size optimizer.

## Verification

Deterministic regressions cover nontrivial padding, zero-slot frames, invalid
size/alignment contracts, arithmetic overflow, and malformed Phase-51 scratch
witnesses. A fixed-seed 350-program corpus reuses the real sealed Phase-49/50/51
pipeline. For every feasible Phase-51 selection it chooses bounded layout
parameters, lays the frame out twice, and independently replays every class,
location, block, operation, input, and output storage mapping while checking
provenance and alignment/non-overlap invariants.

The full repository GCC/Clang/ASan+UBSan matrix remains the authoritative exact
candidate integration gate.

## Complexity and non-claims

For `S` stack slots and `N` total class/location/operation storage references,
layout construction is `O(S + N)` time and `O(S + N)` result storage.

This phase does **not** choose stack-growth direction, concrete load/store
opcodes, addressing modes, a frame/base pointer, callee/caller-saved policy,
calling convention, prologue/epilogue, red zones, variable-size objects, slot
coloring/reuse, or an ABI-minimal frame. It is a target-neutral deterministic
byte-layout boundary over the sealed backend pipeline.

## Seal and promotion

Phase 52 implementation PR #121 used exact head
`83f58c66d9c3ca154cb8b68eb1dd8db910a9c245` over sealed Phase-51
`main@326c01b248d8fe856557b35c8eb8387b3845cb4f` and was squash-merged as
`main@af316ff58300f51be00aa58844f52b5d55c32369`. The exact merged-main CI run
`34410865957` completed successfully on GCC release, Clang release, and GCC
ASan+UBSan.

Phase 52 is therefore **SEALED**. The next coherent backend boundary is Phase 53:
translate the sealed positive frame-byte offsets into a target-neutral signed
base-relative displacement view using an explicit logical frame-base byte anchor
and caller-supplied representable displacement interval. Concrete ISA opcode
selection, encoded addressing modes, stack/base-register assignment, calling
conventions, prologue/epilogue, and ABI policy remain outside that next slice.
