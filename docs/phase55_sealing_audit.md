# Phase 55 sealing audit

## Live checkpoint

Phase 55 reached `main@1e10f30cfffd347f74dea8220bf98e225bda50c6` through PR #127. The exact merged-main CI run `34420412273` completed successfully on all repository gates: GCC release, Clang release, and GCC ASan+UBSan.

No implementation pull request, issue, or competing Phase-56 branch was open when this audit was prepared.

## Capability sealed

Phase 55 converts the sealed Phase-54 finite-register/frame-base plan into a target-neutral fixed-frame coordinate relation for either lower- or higher-growing stacks. It preserves the complete Phase-54 plan, leaves zero-register and register-shortage infeasibility as normal results, and produces entry setup only when the sealed addressed frame exists.

For lower-growing storage it derives `SP += -frame_size`, frame base from adjusted SP as `+anchor`, and direct slot coordinates `slot_offset - frame_size`. For higher-growing storage it derives `SP += +frame_size`, frame base from adjusted SP as `anchor - frame_size`, and direct slot coordinates `slot_offset`.

Every successful plan checks both coordinate identities:

- `stack_adjustment + base_from_adjusted_SP == base_from_entry_SP`;
- `base_from_entry_SP + Phase53_slot_displacement == direct_slot_from_entry_SP`.

The transformation validates the Phase-54 base-register id, non-base register count, frame-witness correspondence, frame anchor, slot geometry, and every sealed Phase-53 displacement before deriving coordinates.

## Evidence and claim boundary

Deterministic verification covers zero-register/register-shortage pass-through, both stack directions, repeated replay, malformed Phase-54 witnesses, unknown direction rejection, and exact signed boundaries including representable `INT64_MIN` negative magnitude.

A fixed-seed 300-program corpus is generated through the real sealed Phase-54 planner. An independent oracle recomputes every stack/base/slot coordinate from frame size, anchor, and byte offset rather than invoking the production recurrence.

The exact integrated repository matrix is the authoritative evidence. Phase 55 claims `O(S)` added time/storage for `S` frame slots and no ISA, ABI, encoded-addressing, save/restore, unwind, variable-frame, or slot-reuse capability.

## Architecture audit and Phase 56 promotion

The coordinate gap is closed, but the stack pointer is still a symbolic coordinate rather than an owned physical register. Phase 54 already proved that persistent assignments and spill-scratch registers are disjoint from one reserved frame-base register. The next coherent cross-layer step is therefore to reserve the stack pointer from the same finite physical-register file rather than inventing machine instructions prematurely.

Phase 56 should reserve the highest physical register as the stack pointer, run the sealed Phase-54 planner over the remaining prefix, and then derive Phase-55 stack-directed coordinates from that nested plan. For a total register count `R`, the baseline ownership should be:

- `R == 0`: no stack-pointer ownership and no feasible frame plan;
- `R > 0`: stack pointer is `R-1`;
- the nested Phase-54 plan receives exactly `R-1` registers, so when a frame base is feasible it is below the stack pointer and all persistent/scratch registers are below the frame base.

Successful results must replay every physical-register reference and prove pairwise disjointness among stack pointer, frame base, persistent assignments, and spill scratch. Nested register shortage remains a normal infeasible result.

Concrete stack/base instructions, ABI calling convention, callee/caller-save classification, red zones, prologue/epilogue ordering/encoding, unwind metadata, and variable-sized frames remain out of scope.

## ROADMAP presentation note

`ROADMAP.md` still carries presentation-only historical state for the late backend sequence because the connected write workflow deliberately avoids rewriting the large historical file merely for status cosmetics. This proof and audit are authoritative for the Phase-55 seal and Phase-56 promotion.
