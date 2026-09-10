# Phase 59 — symbolic fixed-frame instruction lowering

## Scope

Phase 59 consumes only a canonical sealed Phase-58 `LegalizedBackendFixedFrameActionPlan` and lowers each already-legal target-neutral action into one explicit symbolic instruction form. This phase selects executable instruction semantics; it does not choose concrete ISA opcode numbers/bytes, ABI register names, save/restore conventions, return-address handling, red zones, stack probing, unwind metadata, dynamic allocation, variable-sized frames, or instruction-count-optimal encodings.

The symbolic vocabulary is intentionally small:

- `BackendStackPointerDecrementImmediateInstruction { sp, magnitude }`;
- `BackendStackPointerIncrementImmediateInstruction { sp, magnitude }`; and
- `BackendFrameBaseFromStackPointerImmediateInstruction { fp, sp, signed_immediate }`.

The result retains the entire Phase-58 plan as provenance and exposes deterministic entry and exit instruction sequences.

## Preconditions and canonical-witness obligation

Lowering does not trust a caller-supplied legalized sequence merely because every individual operand looks representable. It recomputes

`legalize_fixed_frame_actions(source_plan.source_plan, source_plan.policy)`

and requires exact equality with the supplied Phase-58 witness before selecting any instruction. Tampered actions, reordered chunks, changed physical-register ids, changed immediates, and valid-but-changed legality policies therefore fail the canonical equality check with `std::logic_error`; a malformed policy may be rejected earlier by the sealed Phase-58 policy validation with `std::invalid_argument`.

An infeasible canonical Phase-58 plan remains a successful empty symbolic plan.

## Instruction-selection invariant

Every Phase-58 action maps one-to-one, in order, to exactly one symbolic instruction:

- a stack-pointer action toward lower addresses maps to decrement-immediate;
- a stack-pointer action toward higher addresses maps to increment-immediate; and
- a frame-base materialization maps to frame-base-from-stack-pointer-plus-signed-immediate.

Physical-register ids are copied unchanged. Stack-adjustment magnitudes are copied as `size_t` without signed narrowing, including the legal 64-bit `2^63` boundary inherited from Phase 57/58. Frame-base displacement remains the exact already-legal `int64_t` immediate. Explicit zero-byte stack movements remain explicit zero-immediate instructions.

Because Phase 58 already guarantees chunk legality and reverse exit chunking, Phase 59 neither rechunks nor optimizes the sequence.

## Replay / correctness evidence

Deterministic tests cover:

- complete Phase-58 witness tampering and policy tampering rejection;
- both stack-growth directions and explicit instruction-form selection;
- physical-register and immediate preservation;
- infeasible empty plans;
- explicit zero-byte movement; and
- the `2^63` direction-plus-magnitude boundary on hosts where `size_t` provides at least 64 value bits.

A fixed-seed 240-program upstream corpus runs both stack-growth directions for 480 Phase-56/57/58 plans. An independent register-state interpreter executes the Phase-58 action sequence and the Phase-59 symbolic instruction sequence separately, then requires identical register state after entry and after exit. This checks instruction effects rather than only variant names. Repeated lowering must be deterministic.

The randomized corpus is executable evidence, not a proof substitute. Correctness relies on the one-to-one selection invariant plus canonical Phase-58 witness revalidation.

## Complexity

For `A` legalized entry/exit actions, lowering performs one Phase-58 canonical revalidation plus one linear instruction-selection pass. The new selection work itself is `O(A)` time and `O(A)` output storage. The total function cost also includes the sealed Phase-58 re-derivation/legalization cost required for tamper detection.

## Frontier

Phase 59 is SEALED at merged `main@01ec5849e5a346414bfd18af8a6c7b89fdc33720` after exact merged-main CI run `34436410591` completed successfully on GCC release, Clang release, and GCC ASan+UBSan. The promoted frontier is Phase 60 canonical fixed-frame byte encoding: consume only a canonical Phase-59 symbolic plan, revalidate that witness, serialize the three symbolic instruction forms into one explicit repository-defined byte format, and provide an independent decoder/replayer that proves the bytes preserve Phase-59 register-state semantics.

Phase 60 is an educational repository bytecode boundary, not a claim about x86-64, AArch64, RISC-V, any ABI, instruction-size optimality, relocations, object files, or executable machine code.
