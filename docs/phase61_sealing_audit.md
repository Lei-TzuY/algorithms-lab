# Phase 61 sealing audit

## Live checkpoint

Phase 61 reached merged `main@3ec4a413d4c0aa2bd28410a332b7d45e365128c0`. Exact push CI run `34444146450` completed successfully on GCC release, Clang release, and GCC ASan+UBSan; every Configure, Build, and Test step succeeded. At audit time there was no open pull request, no open issue, and no Phase-62 branch. The existing `phase-61-fixed-frame-bytecode-execution` branch is merged implementation history rather than competing work.

The repository's long `ROADMAP.md` is presentation-stale in the late-backend section: it still stops at Phase 53 even though Phase 53 through Phase 60 each have implementation/sealing records and live main is Phase 61. This is governance debt, not executable-state ambiguity. Commit chronology plus the per-phase implementation and sealing documents remain authoritative for Phase 53 onward. This audit does not rewrite the large historical roadmap merely to duplicate already sealed evidence.

## Capability sealed

Phase 61 closes the execution gap left by Phase 60 for fixed-frame entry/exit bytecode:

- strictly decode the complete repository byte stream before semantic mutation;
- re-encode the retained Phase-59 source plan and require exact equality with the supplied Phase-60 plan;
- require decoded entry/exit instructions to equal the canonical source witness;
- preflight every referenced physical-register id against the caller's finite register file;
- execute stack-pointer increment/decrement with checked sign-aware arithmetic across the representable wire-magnitude domain, including the `2^63` boundary on 64-bit hosts;
- materialize the frame-base register with checked signed displacement arithmetic; and
- return after-entry and after-exit register snapshots without mutating caller-owned input.

The sealed result is intentionally target-neutral. It does not define native execution, ABI/calling-convention behavior, a target register file, memory effects, body-instruction computation, object loading, relocation, or concrete ISA compatibility.

## Evidence and proof boundary

Deterministic regressions cover both stack-growth directions, canonical and provenance tampering, malformed/truncated/trailing byte streams, finite-register rejection, caller-state preservation, checked arithmetic boundaries, and the exact `INT64_MIN` result reached from a `2^63` decrement magnitude.

A fixed-seed 320-plan corpus is executed in both stack-growth directions for 640 differential cases. Production is compared against a test-local raw-byte interpreter that parses the serialized instruction stream independently of the production decoder. The corpus is implementation evidence rather than theorem proof; the correctness argument rests on strict complete-stream parsing, canonical provenance revalidation, finite-state preflight, and checked arithmetic.

The exact merged-main GCC/Clang/ASan+UBSan matrix is the integration gate for this phase.

## Architecture audit

No unresolved Phase-61 correctness or integration blocker was found. The implementation performs one coherent job and its exclusions are real: the executor has no frame-memory state and does not execute the backend `BackendOperation` sequence retained much deeper in the provenance chain.

That retained chain is nevertheless valuable. The canonical Phase-60 plan preserves Phase-59 -> Phase-58 -> Phase-57 -> Phase-56 provenance. The Phase-56 plan retains the successful Phase-54/53 addressed frame, whose blocks still contain the Phase-50 materialized storage-transfer operations. Phase-50 already distinguishes `register_move`, `stack_reload`, opaque `instruction`, and `stack_store`; Phase-52/53 already provide deterministic byte offsets/base-relative displacements. The missing capability is therefore execution of those storage transfers, not another representation wrapper.

## Phase 62 promotion

Phase 62 is **target-neutral spill-transfer execution**. Its coherent vertical slice should:

- consume the complete canonical Phase-60/61 provenance and reject any non-canonical plan before state mutation;
- require a successful addressed frame and use its preserved block/operation order rather than inventing a parallel body IR;
- introduce explicit finite caller-supplied register state plus explicit finite frame-storage state;
- execute `register_move`, `stack_reload`, and `stack_store` exactly against that state, validating every register and frame location before mutation;
- preserve block/operation provenance in a replayable execution trace or snapshots;
- treat `BackendOperationKind::instruction` as an explicit opaque barrier: validate its storage operands but do not fabricate instruction semantics or output values;
- define transactional failure behavior for malformed storage references or representability errors; and
- verify transfer behavior against an independent direct storage-state oracle across deterministic cases and a fixed-seed corpus derived from real upstream backend plans.

This promotion adds an actual stateful storage/memory boundary while staying honest about what remains absent. It must not claim native memory, machine opcodes, ABI/calling-convention semantics, instruction computation, object loading, relocation, or target compatibility.
