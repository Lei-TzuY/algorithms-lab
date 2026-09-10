# Phase 56 — physical stack-pointer ownership

## Capability

Phase 56 closes the register-ownership gap left by the sealed Phase-55 stack-directed frame-entry coordinates. `plan_stack_pointer_owned_backend_frame` reserves the highest physical-register id from one finite register file as the stack pointer, runs the sealed Phase-54 frame-base planner only over the remaining prefix, and then derives the sealed Phase-55 lower-/higher-growing stack coordinates from that nested plan.

For a total physical-register budget `R`:

- `R == 0`: no stack-pointer register is owned; the nested plan receives zero registers and no frame is produced;
- `R > 0`: stack pointer is exactly `R-1` and the nested Phase-54 planner receives exactly `R-1` registers;
- when `R > 1`, Phase 54 reserves `R-2` as frame base before attempting allocation in the lower prefix;
- nested register shortage remains a normal infeasible result with no addressed frame/entry setup.

No machine instruction, ABI register name, calling convention, save/restore policy, red zone, unwind rule, or variable-sized frame behavior is introduced.

## Ownership invariant

Whenever an addressed frame exists, physical-register ownership is strictly stratified:

`persistent/scratch ids < frame-base id < stack-pointer id < R`.

Production replays the nested Phase-54/55 witness and validates every physical-register occurrence exposed by the base-relative addressed frame. Scratch registers, class storage, location storage, and operation input/output storage must all lie below the frame base. The Phase-55 entry setup must name that same frame-base register.

This makes stack pointer, frame base, persistent assignments, and spill-scratch storage pairwise disjoint without changing the sealed allocation algorithms.

## Verification

Deterministic tests cover:

- zero, one, and two total physical registers;
- successful spill/scratch ownership with stack pointer and frame base both reserved;
- nested non-base register shortage;
- both stack-growth directions and replay determinism;
- propagation of nested frame-configuration validation;
- rejection of an unknown stack-growth direction.

A fixed-seed 320-program corpus compares Phase-56 output against an explicit manual composition of sealed Phase 54 followed by sealed Phase 55. Tests separately replay the ownership invariant over every physical-register reference in each successful addressed frame.

The repository GCC/Clang/ASan+UBSan matrix is the integration authority.

## Complexity and boundary

Beyond the sealed Phase-54/55 work, Phase 56 adds linear validation in the number of exposed addressed-frame physical-register references. It stores the complete nested provenance plus one optional stack-pointer id.

The phase does not claim target ISA lowering, stack-pointer mutation instructions, frame-base materialization instructions, ABI save/restore classification, prologue/epilogue ordering, unwind metadata, stack probing, red zones, dynamic alloca, or variable-sized frames.

## Sealed checkpoint and next architecture frontier

Phase 56 is sealed after PR #129 reached `main@b099110688330f0f623767f4fe624ddfad0f5de9` and exact merged-main CI run `34423035411` completed successfully on GCC release, Clang release, and GCC ASan+UBSan.

The next coherent frontier is Phase 57: an abstract fixed-frame prologue/epilogue action plan. It should consume the sealed ownership/coordinate witness and emit explicit target-neutral stack adjustment and frame-base materialization/restoration actions while still avoiding concrete ISA encodings, ABI register naming, caller/callee-save policy, unwind metadata, or variable-sized frames.
