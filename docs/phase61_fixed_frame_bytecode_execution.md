# Phase 61 — canonical fixed-frame bytecode execution

## Scope

Phase 61 adds one target-neutral production execution boundary over the sealed Phase-60 repository bytecode. It consumes a complete `BackendFixedFrameBytecodePlan` plus a caller-supplied finite signed register file and returns explicit register snapshots after the entry and exit instruction sequences.

This is an educational semantics engine. It is not native execution, an ISA emulator, an ABI, a calling convention, a memory model, body-instruction execution, object loading, relocation, or target opcode compatibility.

## Trust and validation boundary

Execution first strictly decodes the complete Phase-60 byte stream. Malformed magic/version/opcodes, truncation, impossible count claims, host-unrepresentable operands, and trailing bytes therefore fail before semantic state exists.

The executor then re-runs `encode_backend_fixed_frame_bytecode(plan.source_plan)` and requires exact equality with the supplied plan. This revalidates the complete Phase-59 provenance chain and proves that a well-formed but altered byte stream is not accepted merely because it can be decoded. The decoded entry/exit instructions are also required to equal the canonical source witness.

Every physical-register reference in both sequences is preflighted against the caller's finite register-file size before execution. The executor never grows hidden register state.

## Arithmetic obligations

Stack immediates are unsigned Phase-60 wire magnitudes. They are not narrowed to `int64_t` before arithmetic. Increment/decrement uses sign-aware checked arithmetic over the full `uint64_t` wire magnitude domain that can be represented by the canonical Phase-60 plan. On 64-bit hosts this includes the `2^63` magnitude boundary; for example, decrementing zero by `2^63` yields exactly `INT64_MIN`, while decrementing `INT64_MIN` by the same magnitude is rejected.

Frame-base materialization performs checked signed `int64_t` addition of the selected stack-pointer value and the sealed signed displacement.

The caller input is exposed as `span<const int64_t>` and is copied only after decode/canonical/register validation. Arithmetic then operates on executor-owned state. Any validation or arithmetic exception therefore leaves caller-owned registers untouched.

## Verification

Deterministic tests cover both stack-growth directions, explicit after-entry/after-exit snapshots, malformed bytes, well-formed canonical-byte tampering, tampered Phase-59 provenance, finite-register rejection, caller-state preservation, the `2^63` stack-magnitude boundary, and arithmetic underflow.

A fixed-seed corpus generates 320 bounded canonical frame configurations and executes both stack-growth directions. For every plan, production execution is compared with a test-local raw-byte interpreter that parses opcodes and operands independently of the production decoder and uses a finite register file. This produces 640 bytecode-execution differential cases while keeping arithmetic in the independent oracle's deliberately small safe domain.

The corpus is implementation evidence, not a theorem proof. Correctness relies on strict complete-stream parsing, complete canonical provenance revalidation, finite-register preflight, and checked integer arithmetic.

## Complexity

For `B` bytecode bytes, `I` decoded instructions, and `R` caller registers, validation and execution take `O(B + I + R)` time including Phase-60 canonical re-encoding and the two returned register snapshots. Working/output storage is `O(I + R)` in addition to the supplied canonical plan.

## Sealed boundary and next frontier

Phase 61 is **SEALED** at merged `main@3ec4a413d4c0aa2bd28410a332b7d45e365128c0` after exact push CI run `34444146450` completed successfully on GCC release, Clang release, and GCC ASan+UBSan. The architecture audit is recorded in `phase61_sealing_audit.md`.

The next coherent backend gap is not another fixed-frame opcode or byte-encoding variant. Phase 62 is promoted as **target-neutral spill-transfer execution**: reuse the preserved addressed backend blocks beneath the canonical Phase-60/61 provenance, add explicit finite frame-storage state, and execute only the already-materialized `register_move`, `stack_reload`, and `stack_store` transfer operations with strict address/register validation. Opaque `instruction` operations remain an explicit non-executed boundary until a later phase supplies body-instruction semantics. Phase 62 must not claim native memory, ISA execution, ABI behavior, or arbitrary body computation.
