# Phase 68 — explicit SSA/backend semantic control

## Scope

Phase 68 adds the first explicit control-semantic boundary to the retained scalar
SSA/backend pipeline. It does not infer a branch from CFG adjacency. Reachable
blocks may instead carry one of three validated terminators:

- `opaque`: successor ownership remains with the caller;
- `jump`: one explicit logical CFG successor;
- `branch_if_nonzero`: one renamed SSA predicate plus explicit nonzero/zero
  logical successors.

`construct_control_semantic_out_of_ssa()` reuses the sealed Phase-66 semantic SSA
constructor and Phase-47 SSA destruction rather than introducing a parallel IR.
A conditional source variable is appended temporarily as a no-definition SSA use
at block exit, renamed by the existing SSA stack discipline, then removed from
the instruction list and retained as the control predicate. This makes the
terminator consume the exact reaching SSA version after all ordinary block
instructions.

## Lowered control and CFG ownership

SSA destruction can insert split blocks to materialize phi copies on critical
edges. A control target therefore retains both:

- the source-level logical successor; and
- the concrete lowered execution successor that must execute next.

When a logical critical edge expands to equivalent parallel split blocks, the
current control layer canonicalizes the execution target to the lowest split
block id; the logical successor remains explicit. Compiler-generated split blocks
carry an explicit jump to their sole lowered successor. No successor is derived
from adjacency ordering, block numbering, or out-degree in the runtime selector.

This phase does not claim physical-edge identity for parallel arcs. The graph
still retains those arcs, but semantic control selects a logical successor and a
canonical equivalent split-copy execution path.

## Liveness and allocation invariant

A conditional predicate is a real use at the end of its phi-free block.
`allocate_phi_free_registers()` therefore:

1. validates and inserts the predicate location into the allocation universe;
2. adds it to block `use` only when not already defined in the block;
3. seeds reverse operation liveness with the predicate live before control; and
4. includes that state in interference construction.

The predicate is not incorrectly reported as successor `live_out`; it is consumed
by the terminator before control leaves the block. Consequently a value defined
earlier in the block can interfere with later values that remain live until the
terminator, and the predicate receives ordinary persistent allocation/spill
storage.

## Backend storage and execution

Phase-50 spill lowering maps the renamed predicate to the same persistent
`BackendStorage` already owned by its allocation class. It may be either a
physical register or a stack slot, never transient scratch storage. Phase-51
scratch reservation validates that persistence and preserves the control
provenance in the physicalized block.

`execute_backend_semantic_control_block()` first reuses the sealed Phase-67 block
continuation executor. If an opaque body instruction suspends, control is not
evaluated. After a completed body:

- `opaque` returns `opaque_control` without choosing any edge;
- `jump` returns the explicit logical/lowered successor pair;
- `branch_if_nonzero` reads the predicate from the actual post-block persistent
  register or frame slot and selects the explicit nonzero/zero target.

The supplied `OutOfSsaProgram` must rebuild the exact retained Phase-56 backend
provenance before selection is trusted. Target tampering therefore fails the
program-binding check rather than silently redirecting execution.

## Verification

The dedicated suite exercises the real semantic SSA → SSA destruction →
liveness/allocation → coalescing/spill lowering → fixed-frame backend → Phase-67
continuation path. Deterministic regressions cover:

- branch choice independent of adjacency insertion order;
- predicate interference as a real block-end use;
- a forced spilled predicate read from its actual frame slot;
- a conditional target routed through a critical-edge phi-copy split block;
- body suspension before terminator evaluation;
- preservation of opaque caller-owned control; and
- invalid target / provenance tampering rejection.

The existing full repository suite remains the integration oracle for backward
compatibility across sealed phases.

## Complexity

Control-shape and target validation is linear in the inspected outgoing CFG
adjacency. Adding one predicate use does not change the asymptotic fixed-point
liveness complexity; it adds one ordinary location to the existing bit-vector
universe when needed. One semantic successor selection is constant time after
backend block execution, apart from bounded target validation against the
current block adjacency.

## Explicit non-goals

Phase 68 does **not** infer function termination, execute a whole CFG loop by
itself, define return values, model general memory, calls, exceptions, ABI
control transfer, target-ISA branches, or make opaque control semantic. It also
does not claim physical parallel-edge identity at the semantic-control layer.
Those remain separate frontiers rather than being smuggled into this first
terminator slice.

## Checkpoint status

Implementation is ready for exact PR CI and merged-main verification. Phase 68
must not be considered sealed until the registered semantic-control sources and
dedicated tests pass GCC release, Clang release, and GCC ASan+UBSan on the exact
candidate and again after integration.
