# Phase 51 sealing audit

## Live integration evidence

Phase 51 implementation PR #119 used exact head `4e2cf7bfbacd547887c68f0c99ba88af298eb682` over sealed Phase-50 `main@54514802b7cb1563ed39b4131de2c66e0446b575`.

PR CI run `34407480514` completed successfully on GCC release, Clang release, and GCC ASan+UBSan. The branch was squash-merged as `main@b9b7105766c160031c3b4a35f0603c3b2d4004b8`. Exact merged-main CI run `34408039983` also completed successfully on all three jobs.

## Capability audit

The phase closes the Phase-49/50 integration gap rather than introducing a parallel allocator:

- every reservation attempt reruns the sealed Phase-49 coalescer with `allocatable_registers = total_registers - reserved_scratch_registers`;
- every attempt then reruns the sealed Phase-50 backend spill materializer and records its measured scratch demand, stack-slot count, spill-location count, and feasibility;
- attempts are ordered by increasing reserved suffix size, so the first feasible result is the minimum reservation for this fixed deterministic pipeline;
- the selected result retains the exact abstract Phase-50 lowering as replayable provenance and separately exposes physicalized operation blocks;
- persistent assigned registers stay in the allocator-visible prefix while abstract scratch index `i` maps only to physical register `allocatable_registers + i` in the reserved suffix;
- an insufficient finite register file returns no selection while retaining all failed attempts instead of overlapping scratch and assigned registers.

The fixed-seed 350-program corpus replays every reported attempt through the real sealed Phase-49 and Phase-50 implementations. Deterministic cases cover zero reservation, a minimum feasible reservation of one, and a register file that remains infeasible even when fully reserved for scratch.

## Claim boundary

Phase 51 proves only minimum reserved scratch capacity for the chosen deterministic coalescing/materialization pipeline. It does not claim minimum spill count, optimal coloring, maximum coalescing, globally optimal register allocation, spill-cost optimality, or post-rewrite reallocation optimality.

The physicalized operation stream is still machine-independent. Stack slots remain abstract indices; there is no byte-level frame layout, stack-growth direction, ABI, concrete instruction selection, addressing-mode legality, calling convention, save/restore policy, or prologue/epilogue generation claim.

## Seal and promotion

Phase 51 is **SEALED** at `main@b9b7105766c160031c3b4a35f0603c3b2d4004b8` with merged-main CI run `34408039983` green.

The next coherent backend gap is the remaining abstract stack-slot namespace. Phase 52 should add a target-neutral byte-addressed spill-frame layout: accept explicit uniform slot-size/alignment and frame-alignment parameters, assign every sealed Phase-50 stack-slot index a deterministic non-overlapping byte offset with checked arithmetic, expose total frame size/alignment evidence, and rewrite the Phase-51 physicalized operation view so stack operands reference concrete frame offsets while physical-register ids and operation provenance remain unchanged. Concrete ISA opcodes/addressing modes, stack-growth direction, calling conventions, and prologue/epilogue behavior remain outside that slice.
