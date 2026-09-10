# Phase 56 sealing audit

## Live checkpoint

Phase 56 reached `main@b099110688330f0f623767f4fe624ddfad0f5de9` through PR #129. The exact merged-main CI run `34423035411` completed successfully on all repository gates: GCC release, Clang release, and GCC ASan+UBSan.

No implementation pull request, issue, or competing Phase-57 branch was open when this audit was prepared.

## Capability sealed

Phase 56 assigns ownership of the highest physical-register id to a symbolic stack pointer while preserving the sealed Phase-54 frame-base reservation and Phase-55 stack-directed coordinate pipeline beneath it.

For a total physical-register budget `R`, the sealed baseline is:

- `R == 0`: no stack-pointer owner and no addressed frame;
- `R > 0`: stack pointer is exactly `R-1`;
- the nested Phase-54 planner receives exactly `R-1` registers;
- when a frame base is present it is exactly below the stack pointer in the reserved suffix;
- every persistent or spill-scratch physical register exposed by the successful addressed frame lies strictly below the frame base;
- nested register shortage remains a normal infeasible result rather than an exception.

The result preserves the complete nested Phase-54/55 provenance and both lower-/higher-growing stack-coordinate semantics.

## Evidence and invariant boundary

Deterministic verification covers zero/one/two-register budgets, a successful spill/scratch hierarchy, nested register shortage, both stack-growth directions, repeated replay, unknown stack-growth rejection, and propagation of nested configuration validation.

A fixed-seed 320-program corpus compares Phase-56 output against explicit manual composition of the sealed Phase-54 planner followed by sealed Phase 55. Independent ownership replay checks scratch registers, class/location storage, and operation input/output storage in every successful addressed frame.

The exact integrated repository matrix is authoritative. Phase 56 adds only target-neutral physical-register ownership and linear ownership validation. It does not claim machine opcodes, ABI register names, caller/callee-save rules, prologue/epilogue encoding, red zones, stack probing, unwind metadata, dynamic allocation, or variable-sized frames.

## Architecture audit and Phase 57 promotion

The finite-register ownership gap is closed: persistent allocation, spill scratch, frame base, and stack pointer now occupy pairwise-disjoint regions of one physical-register file. The remaining fixed-frame backend gap is action sequencing, not another register-allocation variant.

Phase 57 should therefore compile the sealed ownership/coordinate witness into a small target-neutral fixed-frame action plan. A successful frame should expose explicit entry/exit actions such as:

- adjust the owned stack pointer by the sealed Phase-55 fixed-frame displacement on entry;
- materialize the owned frame-base register from the adjusted stack pointer using the sealed coordinate relation;
- restore the stack pointer by the exact inverse fixed adjustment on exit.

An infeasible nested frame should continue to produce no frame actions. Action operands must replay the Phase-56 stack-pointer/frame-base ids and Phase-55 signed coordinate values exactly. Entry and exit stack adjustments must compose to zero without overflow.

Concrete instruction selection/encoding, target ABI names, save/restore of arbitrary callee-saved registers, return-address handling, red zones, unwind metadata, stack probing, and variable-sized frames remain out of scope.

## ROADMAP presentation note

`ROADMAP.md` may lag this late backend sequence as presentation history. This sealing audit and the Phase-56 proof document are authoritative for the Phase-56 seal and Phase-57 promotion.
