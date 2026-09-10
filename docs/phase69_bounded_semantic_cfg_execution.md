# Phase 69 — bounded semantic CFG execution

## Capability

Phase 69 promotes the sealed Phase-68 one-block semantic-control boundary into a
bounded dynamic CFG executor. Execution starts only at `program.start`; callers
provide opaque-instruction reply scripts per *dynamic visit* and never provide a
block id, successor, or preselected path.

Every visit reuses `execute_backend_semantic_control_block`. A completed explicit
`jump` or `branch_if_nonzero` follows the returned concrete
`execution_successor`, including compiler-generated critical-edge copy blocks.
The logical source-CFG successor remains diagnostic evidence and is not used to
skip lowered execution blocks.

## State and handoff invariant

The executor owns a current physical register file and frame-slot vector. Between
dynamic visits it applies the same sealed Phase-64 reserved stack/frame-register
entry convention and requires the next block's `after_entry_registers` to equal
the previous block's `after_block_registers` exactly. Frame-slot state is passed
without reconstruction.

Thus every dynamic visit starts from the exact persistent state produced by its
predecessor after canonical entry handling; no caller-supplied path state or
fresh register/frame snapshot is injected between visits.

## Honest stopping semantics

The result distinguishes three stop reasons:

- `body_suspended`: an opaque body instruction still needs a reply; its terminator
  was not evaluated;
- `opaque_control`: the body completed but successor semantics remain caller-owned;
- `visit_budget_exhausted`: the final allowed visit selected an explicit concrete
  successor, recorded as `next_execution_block`, but execution intentionally did
  not follow it.

A positive finite visit budget and exactly one reply script per allowed visit are
required. Unused later scripts are inert if execution stops early. Budget
exhaustion is a resource boundary only: it is not return, function termination,
or an implicit Phase-65 exit event.

## Correctness obligations

The implementation must preserve:

1. canonical start ownership: visit zero is always `program.start`;
2. semantic successor ownership: only Phase-68 explicit control chooses the next
   concrete block;
3. exact inter-block persistent-state handoff;
4. dynamic-visit reply ownership, including repeated visits to the same block;
5. suspension-before-control ordering;
6. concrete critical-edge split execution before its logical successor; and
7. bounded looping without any termination claim.

Phase-68 still rebuilds and validates backend/program provenance on every visited
block, validates control descriptors/targets, reads branch predicates from their
real persistent register or stack storage, and rejects transient scratch
predicate storage.

## Verification

Native tests cover explicit branch selection against reversed adjacency order,
per-visit opaque replies in a loop, honest body suspension, finite-budget loop
exhaustion with an exact next-block witness, critical-edge phi-copy split routing,
and malformed budget/request shape. Every multi-visit test independently checks
the Phase-64 register-handoff equality between consecutive visits.

The repository's full GCC release, Clang release, and GCC ASan+UBSan matrix is the
integration gate for the retained SSA-destruction, allocation, frame, instruction,
and semantic-control layers.

## Complexity and scope

For at most `B` dynamic visits, Phase 69 adds `O(B)` orchestration and replay
storage around the costs of the sealed per-block backend executor. It also copies
the finite register file once per visit to preserve the Phase-64 entry convention;
therefore no sublinear-in-state handoff claim is made.

This phase does **not** add inferred return/function termination, fixed-frame exit
execution, opaque-control interpretation, general memory, calls, exceptions,
ABI semantics, native code, target-ISA execution, or unbounded loop execution.
