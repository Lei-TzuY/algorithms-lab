# Phase 62 — target-neutral spill-transfer execution

## Scope

Phase 62 adds one stateful storage-transfer execution boundary on top of the sealed Phase-61 fixed-frame bytecode executor. It does not invent a second backend IR. A canonical Phase-60 bytecode plan already retains the full Phase-59 -> Phase-58 -> Phase-57 -> Phase-56 provenance chain, and the Phase-56 plan retains the successful Phase-53 addressed frame whose blocks contain the Phase-50 materialized backend operations.

The production API executes one caller-selected reachable addressed backend block. This block-level boundary is deliberate: `blocks` form a CFG, so iterating every block in vector order would fabricate control-flow semantics that the backend does not yet define.

## Supported execution semantics

The selected block must be transfer-only. Phase 62 executes exactly the already-materialized storage operations:

- `register_move`: copy one signed scalar value between physical registers;
- `stack_reload`: copy one canonical frame-slot scalar value into a physical register; and
- `stack_store`: copy one physical-register scalar value into a canonical frame slot.

`BackendOperationKind::instruction` remains opaque. If the selected block contains any opaque instruction, the complete block is rejected before transfer-state mutation. Phase 62 therefore does not silently treat instructions as no-ops and does not fabricate arithmetic/opcode semantics.

## State model

Caller register input is a finite read-only `span<const int64_t>`. The sealed Phase-61 executor validates canonical bytecode/provenance and produces the after-entry register state. Phase 62 then executes the selected transfer-only block on an owned copy of that state.

Frame state is one signed scalar cell per canonical Phase-52/53 `stack_slot` id. The caller supplies one value for every sealed slot. Base-relative displacements in Phase-53 operations are resolved back to those unique canonical slots. This intentionally models spill-cell semantics, not byte-addressable native memory, load width/endianness, aliasing, object memory, or an ABI.

## Validation and transactional boundary

Before the first transfer mutation, production validates the complete selected block:

- a successful addressed frame must exist;
- block index must be valid and the block must be reachable;
- caller frame state must contain exactly one value per sealed stack slot;
- stack-slot ids must be dense/unique and frame-base displacements unique;
- the caller register file must cover the Phase-56 planned register budget;
- every body register reference must lie below the dedicated Phase-54 frame-base register, preserving frame-base/stack-pointer ownership separation;
- each operation must have exactly one input and one output with the storage kinds required by its operation kind;
- every frame displacement must resolve to a sealed stack slot; and
- any opaque instruction rejects the entire block.

Caller-owned register and frame spans are never mutated. Validation failure therefore cannot expose a partially executed transfer prefix.

## Replay witness

Successful execution returns:

- the sealed Phase-61 after-entry register snapshot;
- the register state after the selected transfer-only block;
- final frame-slot cell values; and
- one ordered trace record per transfer with operation index, operation kind, retained Phase-50 origin kind/index provenance, and the transferred scalar value.

This trace is replay evidence for the transfer layer; it is not a control-flow trace or an instruction-execution trace.

## Verification

Deterministic tests cover register moves, stores, reloads, exact after-entry integration, caller-state preservation, opaque instruction rejection, invalid/unreachable block selection, finite-register rejection, reserved frame-base-register overlap, unknown frame displacement, wrong frame-state size, malformed bytecode, and Phase-59 provenance tampering.

A fixed-seed corpus constructs 240 canonical addressed plans. Each selected block contains 80 random transfer operations over two persistent physical registers and two frame slots. Production final register/frame state and every transferred-value trace entry are compared against an independent test-local scalar-cell replay that computes the fixed frame entry relation directly and does not call the Phase-62 implementation.

The randomized corpus is implementation evidence, not a theorem proof. Correctness rests on complete preflight, unique displacement-to-slot resolution, deterministic operation order, exact scalar copying, and reuse of the sealed Phase-61 canonical-entry trust boundary.

## Complexity

Let `S` be the number of frame slots, `O` the number of operations in the selected block, `B` the Phase-60 bytecode size, `I` the Phase-61 entry/exit instruction count, and `R` the caller register count.

Phase-61 canonical validation/execution costs `O(B + I + R)`. Phase-62 builds an ordered displacement-to-slot map in `O(S log S)`, preflights `O` transfer operations with `O(log S)` frame lookups, then replays them with the same lookup bound. Total time is `O(B + I + R + (S + O) log(S + 1))`; owned/result storage is `O(R + S + O)`.

No wall-clock or throughput claim is derived from CI timing.

## Frontier

Phase 62 is implementation-complete / sealing-audit-pending only after its exact candidate and merged-main GCC/Clang/ASan+UBSan matrices pass.

This phase deliberately stops before arbitrary body-instruction computation and CFG execution. A later phase may add an explicit instruction-semantics or control-flow boundary, but it must not infer native ISA, ABI, object loading, relocation, or real machine-memory behavior from these target-neutral scalar spill cells.
