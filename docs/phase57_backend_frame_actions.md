# Phase 57 — target-neutral fixed-frame entry/exit actions

## Capability

Phase 57 compiles the sealed Phase-56 stack-pointer ownership and Phase-55
coordinate witness into a small ordered action plan. It does not choose an ISA
or ABI. `derive_fixed_frame_actions` consumes an existing
`StackPointerOwnedBackendFramePlan` and preserves it as provenance.

A successful addressed frame emits exactly:

1. an entry stack-pointer adjustment in the sealed stack-growth direction by the
   fixed frame size;
2. a frame-base materialization from the adjusted stack pointer using the sealed
   `frame_base_from_adjusted_stack_pointer` displacement;
3. an exit stack-pointer movement of the same magnitude in the opposite address
   direction.

An infeasible nested frame emits no entry or exit actions.

## Action invariant

The action operands replay the sealed witness exactly:

- the stack-pointer register is the Phase-56 owned register `R-1`;
- the frame-base register is the Phase-56/54 reserved register;
- entry adjustment magnitude equals `frame_size_bytes`;
- entry movement direction matches the Phase-55 stack-growth direction;
- frame-base materialization uses the exact Phase-55 displacement from the
  adjusted stack pointer;
- exit movement has the same magnitude and opposite direction, so allocation
  and restoration are exact inverses.

Stack adjustment is represented as `(direction, size_t magnitude)` instead of a
signed `int64_t` delta. This is intentional: Phase 55 can legally represent a
lower-growing entry adjustment of `INT64_MIN`; its inverse positive magnitude
`2^63` does not fit in `int64_t`, but it is representable as a 64-bit `size_t`.
The action model therefore preserves exact inverse restoration without signed
negation overflow.

## Verification

Deterministic tests cover:

- zero-/one-register and nested register-shortage infeasible plans producing no
  actions;
- successful lower- and higher-growing frames with exact action ordering and
  register/displacement replay;
- repeated deterministic derivation;
- the `INT64_MIN` lower-growing boundary and its representable inverse action;
- rejection of malformed Phase-56 ownership, missing addressed provenance,
  inconsistent frame size, and unknown stack-growth direction.

A fixed-seed 360-program corpus builds real Phase-56 plans for both stack-growth
directions, derives actions, and independently replays every action operand
against the sealed Phase-55/56 witness.

The repository GCC/Clang/ASan+UBSan matrix is the integration authority.

## Complexity and boundary

The action derivation itself emits at most three actions and performs constant
structural checks. The result deliberately copies the complete sealed Phase-56
provenance, so total construction cost and storage are linear in that nested
witness size.

This phase does **not** introduce concrete machine opcodes, encoded immediate
widths, ABI stack/frame register names, caller/callee-save classification,
return-address handling, frame-base value restoration, red zones, stack
probing, unwind metadata, dynamic `alloca`, or variable-sized frames. The exit
plan restores only the fixed stack-pointer movement because no ABI save/restore
policy exists yet.

## Sealed checkpoint and Phase 58 promotion

Phase 57 is sealed at merged `main@f457a88ded9aecf1fb020bb65024c21070bce180`.
The exact post-merge CI run `34425593986` completed successfully on GCC release,
Clang release, and GCC ASan+UBSan. The phase-level audit is recorded in
`docs/phase57_sealing_audit.md`.

The action-sequencing gap is therefore closed. The next backend boundary is not
another high-level action variant but **target-neutral action legalization under
explicit immediate constraints**.

Phase 58 should consume a sealed `BackendFixedFrameActionPlan` plus a
caller-supplied legality policy and produce a replayable legalized action
sequence that:

- splits a stack-pointer movement into deterministic same-direction chunks no
  larger than the configured positive stack-adjustment immediate magnitude;
- preserves exact entry/exit inverse movement after chunking, including the
  Phase-57 `2^63` magnitude boundary when `size_t` can represent it;
- requires the frame-base materialization displacement to lie inside an explicit
  caller-supplied signed immediate interval rather than silently narrowing it;
- returns no legalized frame actions for the same nested infeasible plans that
  Phase 57 leaves empty; and
- rejects malformed policies or Phase-57 witnesses instead of manufacturing a
  target-specific interpretation.

Concrete ISA opcodes/encodings, instruction-count optimality, ABI register
names, caller/callee-save policy, return-address handling, unwind metadata,
stack probing, red zones, dynamic `alloca`, and variable-sized frames remain out
of scope.
