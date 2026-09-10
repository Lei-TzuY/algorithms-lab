# Phase 62 — target-neutral spill-transfer execution

## Scope

Phase 62 adds one stateful storage-transfer execution boundary on top of the sealed Phase-61 fixed-frame bytecode executor. It does not invent a second backend IR. A canonical Phase-60 bytecode plan retains the Phase-59 -> Phase-58 -> Phase-57 -> Phase-56 provenance chain, and the Phase-56 plan retains the successful Phase-53 addressed frame whose blocks contain the Phase-50 materialized backend operations.

The production API executes the semantically known storage-transfer prefix of one caller-selected reachable addressed backend block. This block-local boundary is deliberate: blocks form a CFG, so iterating them in vector order would fabricate control-flow semantics that the backend does not yet define.

## Canonical provenance boundary

Phase 61 strictly validates the serialized fixed-frame entry/exit bytecode. Its byte stream does not serialize the addressed body operations that Phase 62 now consumes. Phase 62 therefore adds a deeper replay check before any storage mutation:

1. require a retained successful Phase-51 register selection, Phase-52 byte frame, and Phase-53 addressed frame;
2. deterministically rebuild Phase 52 from the retained selection and layout configuration and require exact equality;
3. deterministically rebuild Phase 53 from that byte frame and addressing configuration and require exact equality; and
4. rederive the Phase-55 stack-directed frame entry plan and require exact equality with retained provenance.

This is an internal cross-layer consistency boundary over the provenance that is actually retained. It is not a claim that arbitrary caller mutation could be reconstructed all the way back to an unavailable external source program.

## Supported execution semantics

Phase 62 executes the storage semantics that are already fully materialized:

- `register_move`: copy one signed scalar value between physical registers;
- `stack_reload`: copy one canonical frame-slot scalar value into a physical register; and
- `stack_store`: copy one physical-register scalar value into a canonical frame slot.

`BackendOperationKind::instruction` is an explicit opaque barrier. The complete selected block is structurally preflighted first, including every instruction's register inputs and optional register output. Execution then proceeds in operation order until the first opaque instruction. That barrier is recorded in the trace and execution suspends immediately before its unknown computation. The instruction receives no fabricated result, and no later transfer is executed from stale register state.

A transfer-only block therefore completes normally. A real spilled instruction block can execute its reload prefix and stop honestly at the computation boundary rather than being rejected merely for containing an instruction or silently treating that instruction as a no-op.

## State model

Caller register input is a finite read-only `span<const int64_t>`. The sealed Phase-61 executor validates canonical fixed-frame bytecode/provenance and produces the after-entry register state. Phase 62 executes known storage transfers on an owned copy of that state.

Frame state is one signed scalar cell per canonical Phase-52/53 `stack_slot` id. The caller supplies one value for every sealed slot. Base-relative displacements are resolved back to those unique canonical slots. This models spill-cell semantics, not byte-addressable native memory, load width/endianness, aliasing, object memory, or an ABI.

## Validation and transactional boundary

Before the first transfer mutation, production validates the complete selected block:

- successful retained Phase-51/52/53 provenance must exist and replay exactly;
- block index must be valid and the block must be reachable;
- caller frame state must contain exactly one value per sealed stack slot;
- stack-slot ids must be dense/unique and frame-base displacements unique;
- the caller register file must cover the Phase-56 planned register budget;
- every body register reference must lie below the dedicated Phase-54 frame-base register, preserving frame-base/stack-pointer ownership separation;
- transfer operations must have exactly one input and one output with the storage kinds required by their operation kind;
- instruction operands and optional output must be valid body physical registers; and
- every frame displacement must resolve to a sealed stack slot.

Caller-owned register and frame spans are never mutated. Invalid later operations are found during complete preflight, before an earlier transfer can change owned execution state.

## Replay witness

Successful execution returns:

- the sealed Phase-61 after-entry register snapshot;
- the register state after the known transfer prefix;
- final frame-slot cell values after that prefix;
- one ordered trace record per executed transfer; and
- when an instruction barrier is reached, its operation index plus a trace entry carrying kind/origin provenance and no transferred scalar value.

The witness distinguishes completion from suspension. It is not a control-flow trace or an instruction-computation trace.

## Verification

Deterministic regressions cover transfer-only register moves/stores/reloads, exact Phase-61 entry integration, caller-state preservation, finite register and frame-state rejection, malformed bytecode, Phase-53 body-provenance tampering, and explicit opaque-barrier suspension that does not execute a dependent post-instruction store.

A dedicated cross-phase integration case builds an actual one-block `OutOfSsaProgram`, runs the sealed register-reservation / frame-layout / addressing / frame-action / bytecode pipeline, requires a real spill reload before an opaque instruction, and compares Phase-62 prefix state against a test-local direct storage replay.

The transfer-only differential corpus retains 240 fixed-seed plans with 80 operations each and compares final register/frame state plus every transfer trace value against an independent scalar-cell replay. The corpus is implementation evidence, not theorem proof.

Correctness rests on canonical retained-provenance replay, complete structural preflight, unique displacement-to-slot resolution, deterministic operation order, exact scalar copying, and explicit suspension at unknown computation.

## Complexity

Let `S` be frame slots, `O` selected-block operations, `P <= O` operations up to and including the first opaque barrier, `B` Phase-60 bytecode size, `I` Phase-61 entry/exit instruction count, and `R` caller registers.

Phase-61 validation/execution is `O(B + I + R)`. Rebuilding Phase 52/53 is linear in retained frame/body representation plus checked layout work. Displacement indexing costs `O(S log S)`. Complete preflight costs `O(O log(S + 1))`, and known-prefix replay costs `O(P log(S + 1))`. Owned/result storage is `O(R + S + P)` beyond the retained plan.

No wall-clock or throughput claim is derived from CI timing.

## Frontier

Phase 62 is implementation-complete / sealing-audit-pending only after its exact candidate and merged-main GCC/Clang/ASan+UBSan matrices pass.

This phase deliberately stops at the first opaque body instruction and does not define CFG execution. A later phase may add an explicit target-neutral instruction-semantics boundary or control-flow execution, but must not infer native ISA, ABI, object loading, relocation, or real machine-memory behavior from these scalar spill cells.
