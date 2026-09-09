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
