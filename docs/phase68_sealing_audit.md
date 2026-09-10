# Phase 68 sealing audit

## Verified checkpoint

Phase 68 implementation PR #153 used exact final candidate
`09b9a0d80994fdb962efce01be1cec9ab5e4ca4c` over sealed Phase-67
`main@e5e56498033cda18f2724862b658be760a680383`.

Exact PR CI run `34500447455` completed successfully on GCC release, Clang
release, and GCC ASan+UBSan. The implementation was squash-merged as
`main@508020ffb2c7fb14ccd2d1e4fc2bad01fd337479`; exact merged-main CI run
`34501679430` also completed successfully on all three repository gates.

## Capability sealed

The retained semantic SSA/backend pipeline now owns explicit executable control
for one completed block. Reachable blocks may carry validated `jump` or
`branch_if_nonzero` terminators, while `opaque` control remains caller-owned.
Control predicates are renamed as genuine block-end SSA uses and participate in
the same liveness, interference, allocation, spill, frame, and physicalization
pipeline as ordinary values.

Logical source-CFG targets remain distinct from concrete lowered execution
targets when SSA destruction inserts critical-edge copy blocks. Compiler-created
split blocks receive explicit jumps, and the backend selector never chooses a
successor from adjacency order, out-degree, or block numbering.

Conditional predicates are read after body execution from their real persistent
backend storage. Register and stack-slot bindings are supported; transient spill
scratch is rejected. A suspended opaque body instruction prevents terminator
evaluation, and backend/program provenance is rebuilt before any selected target
is trusted.

## Correctness findings closed during implementation

Remote CI exposed a real allocator soundness gap: the interference graph recorded
simultaneously-live cliques but omitted the machine-write obligation between an
operation definition and values live after that operation. A dead definition
could therefore share a physical register with a live-through branch predicate
and clobber it before control evaluation.

The implementation now adds definition-vs-live-after interference. An independent
forward/path liveness oracle was updated through a different procedure, and a
core regression proves the clobber cannot recur.

The repair also invalidated an old copy-coalescing expectation where a later
redefinition would overwrite a still-live copied value. That unsafe merge is now
rejected, while an existing genuinely safe coalescing case remains covered.
These are repository-wide allocator correctness repairs, not relaxed Phase-68
test expectations.

## Audit findings

No unresolved correctness blocker was found in:

- control descriptor shape and target-edge validation;
- SSA predicate version ownership at block exit;
- definition-vs-live-after interference;
- safe versus clobbering copy coalescing;
- persistent register/stack predicate storage;
- critical-edge split-block control routing;
- exact backend/program provenance binding;
- body suspension before terminator evaluation; or
- opaque control ownership.

The exact merged-main sanitizer gate passed with the complete repository suite.
No benchmark, native-code, ABI, memory-model, return, or whole-program execution
claim is inferred from that CI result.

## Promoted frontier

Phase 68 deliberately stops after one block plus one explicit successor choice.
The next coherent capability is bounded multi-block semantic CFG execution.

A promoted executor should:

1. begin at the canonical `program.start`;
2. reuse the sealed Phase-68 block executor for every dynamic visit;
3. follow only the concrete successor selected by explicit semantic control;
4. carry exact register and frame state between visits using the sealed Phase-64
   handoff invariant;
5. let the caller supply opaque-instruction replies per dynamic visit, but not a
   preselected CFG path;
6. stop honestly on body suspension or opaque control; and
7. use an explicit finite visit budget so loops are executable without claiming
   termination.

Budget exhaustion is a resource boundary, not a return or function-exit
semantic. General memory, calls, returns, exceptions, ABI behavior, target-ISA
execution, and opaque-control interpretation remain out of scope until explicitly
represented.

## Scope decision

Phase 68 is sealed here. Adding more branch-shape variants, allocator microcases,
or documentation aliases would refine an already verified boundary without a
comparable new architectural capability. The next implementation work must move
the retained backend from single-block successor selection to bounded semantic
CFG execution without reintroducing caller-chosen paths.
