# Phase 63 — explicit instruction-result continuation

## Scope

Phase 63 extends the sealed Phase-62 block-local storage executor across opaque body-instruction barriers without inventing instruction semantics. `SsaInstruction` retains dataflow uses and an optional definition but no opcode or computation. A caller therefore supplies an ordered instruction-reply script as an explicit semantic-oracle boundary.

The implementation remains target-neutral. It does not claim arithmetic opcode semantics, branch predicates, native ISA behavior, byte-addressable memory, an ABI, object loading, relocation, or system-call execution.

## Execution contract

`execute_backend_instruction_continuation_block` first invokes the sealed Phase-62 executor on the same canonical Phase-60 plan, block, finite register file, and canonical frame-slot state. Phase 62 therefore remains the canonical byte/provenance/layout/preflight trust boundary and produces the exact owned state immediately before the first opaque instruction.

The Phase-63 script is an ordered prefix of the selected block's opaque instructions. Every reply names the exact backend operation index:

- an instruction with a materialized output register requires one signed scalar `output_value`;
- an instruction with no output requires an explicit reply whose `output_value` is absent;
- wrong operation indices, skipped barriers, output-shape mismatches, and extra replies are rejected.

At an instruction barrier production snapshots the current materialized input-register values in operand order. If a matching reply exists, the reply is recorded as acknowledged. A supplied scalar is written only to the already-materialized output register; production never computes or transforms that value. Execution then continues through the sealed register-move/reload/store semantics. If the reply prefix is exhausted, execution suspends before the next opaque instruction and returns that instruction's observed inputs without acknowledging it.

This supports multiple opaque instructions in one block. A script covering every instruction lets the known block operations complete; a shorter script deliberately yields a replayable suspension checkpoint.

## Invariants and proof obligations

1. **Sealed-prefix invariant.** Phase 63 can execute only after Phase 62 has successfully revalidated the Phase-60/52/53 provenance and preflighted the complete selected block.
2. **Script-prefix invariant.** Reply `i` corresponds to opaque instruction `i` in block order; caller input cannot skip or reorder barriers.
3. **No-fabrication invariant.** Production writes an instruction output iff the caller supplied that exact scalar for an instruction that structurally has an output.
4. **Observed-input invariant.** Each instruction trace records register values from the owned state immediately before that instruction reply is consumed or suspension occurs.
5. **Transfer-continuation invariant.** Operations after an acknowledged instruction reuse only the already-validated Phase-62 register/frame transfer semantics.
6. **Caller-immutability invariant.** Caller register/frame/reply spans are read-only. Rejection never mutates caller-owned state.

The returned witness contains the Phase-61 after-entry register state, final/paused owned register and frame state, ordered transfer/instruction trace, consumed reply count, and optional next suspended instruction.

## Verification

Deterministic tests cover result injection followed by the Phase-50-style spill store, multiple barriers with a script prefix, outputless acknowledgement, wrong index/result shape/extra-script rejection, caller-state preservation, and passthrough of sealed Phase-62 byte/provenance tamper rejection.

A real upstream integration fixture reuses the Phase-62 path from `OutOfSsaProgram` through register reservation, frame layout/addressing/actions, symbolic lowering, byte encoding, and spill reloads. Phase 63 explicitly acknowledges its real opaque no-output instructions until the block completes.

A fixed-seed 180-program corpus generates 60 mixed register moves, reloads, stores, output-producing instructions, and no-output instructions per block. A test-local scalar interpreter starts from the sealed Phase-61 entry snapshot, consumes the same reply prefix, and independently derives every instruction input snapshot, transfer value, suspension point, final register state, and final frame state. Exact vector/state equality is required.

Focused source-level GCC/Clang strict-warning compilation plus a stubbed Phase-62 continuation-state smoke test passed under GCC, Clang, and actual ASan+UBSan. The exact-head PR CI and merged-main CI matrices subsequently passed on GCC release, Clang release, and GCC ASan+UBSan.

## Complexity

Let `S` be canonical frame slots, `O` selected-block operations, `K` opaque instructions, and `Q <= K` supplied replies. Phase 63 first pays the sealed Phase-62 validation/prefix-execution cost. It then builds a displacement map in `O(S log S)`, validates the reply prefix in `O(O + Q)`, and replays at most the remaining `O` operations with `O(log S)` frame-slot lookup per reload/store. Additional owned/result storage is `O(S + R + O)` for frame/register snapshots and trace state.

No benchmark or native-execution performance claim is made from CI timing.

## Sealed frontier

Phase 63 is sealed at merged `main@a86e4cc9c48b360d592649262a0d88ac8973c8f8` after exact post-merge CI run `34451828432` completed successfully on GCC release, Clang release, and GCC ASan+UBSan.

The next coherent boundary is not automatic CFG execution. Runtime branch selection remains absent from the source semantics. Phase 64 may accept an explicit caller-supplied valid CFG path/transition oracle and compose completed Phase-63 block executions along that path, validating canonical program provenance, the start block, every graph edge, per-block instruction replies, suspension boundaries, and register/frame state handoff without fabricating control decisions.
