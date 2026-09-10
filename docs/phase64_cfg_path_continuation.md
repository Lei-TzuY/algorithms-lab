# Phase 64 — caller-supplied CFG-path continuation

## Scope

Phase 64 composes the sealed Phase-63 block continuation semantics across an
explicit finite path in the sealed out-of-SSA control-flow graph. The caller is
still the semantic oracle: it chooses every visited block and supplies the
ordered instruction replies for each visit. Production does not evaluate a
branch predicate, invent a successor, infer termination, or derive an opaque
instruction result.

The path is therefore a replayable control oracle, not an automatic program
executor. Exhausting the caller path does not execute the fixed-frame exit
sequence and is not interpreted as a function return.

## Binding and preflight

Before body execution, Phase 64 requires a directed, non-empty out-of-SSA CFG
with dense block storage and a valid start vertex. It rebuilds the retained
Phase-56 stack-pointer-owned backend plan from the supplied `OutOfSsaProgram`
and the register budget/layout/addressing/stack-direction policies retained by
the Phase-60 plan. Exact Phase-56 equality is required. The later Phase-60/62/63
canonical byte/provenance checks remain authoritative for each visited block.

The entire caller path is preflighted before execution:

- the first block must equal `program.start`;
- every block id must be in range and reachable in both the supplied program and
  the retained addressed backend frame;
- every consecutive pair must be an actual directed CFG edge;
- repeated vertices and explicit self-loop transitions are allowed.

Parallel arcs require only the requested successor vertex because the Phase-47
out-of-SSA lowering has already materialized edge-specific copy semantics into
physical split blocks where required; Phase 64 does not invent an edge identity
or branch condition.

## Exact state handoff

A subtle integration constraint is that sealed Phase 63 deliberately reuses the
Phase-61 fixed-frame entry sequence on every API invocation. Calling it naively
with an already-entered register snapshot would apply the prologue twice.

Phase 64 therefore treats the first Phase-63 result as the canonical entered
state. Before a later block call, it copies the current owned register state and
resets only the dedicated stack-pointer and frame-base registers to their
original function-entry caller values. Phase 61 then reconstructs the canonical
reserved-register state. Phase 64 requires the returned `after_entry_registers`
to equal the previous block's exact `after_block_registers` before accepting the
handoff. Phase-62 validation already forbids backend body operations from
writing those reserved registers, so no body state is discarded by this
normalization.

Frame-slot values are handed directly from one completed block execution to the
next. If a visited block suspends because its instruction-reply prefix is
exhausted, Phase 64 returns that suspension and never follows the next caller
transition.

## Invariants and proof obligations

1. **Program-binding invariant.** The supplied out-of-SSA program must reproduce
   the retained Phase-56 backend witness under the retained policies.
2. **Start invariant.** A non-empty path begins exactly at `program.start`.
3. **Transition invariant.** Every followed consecutive block pair is a real
   directed CFG edge supplied by the caller's bound program witness.
4. **No-control-fabrication invariant.** Production never selects a successor;
   it only validates and follows the caller path.
5. **No-result-fabrication invariant.** Every opaque instruction remains under
   the sealed Phase-63 reply contract.
6. **Exact-handoff invariant.** Every block after the first observes the exact
   previous completed block register/frame state after canonical reserved-state
   reconstruction.
7. **Suspension invariant.** An unacknowledged instruction stops before any
   successor block executes.
8. **Finite-prefix invariant.** Caller path exhaustion is only oracle exhaustion,
   not a termination or exit-sequence claim.
9. **Caller-immutability invariant.** Program, path/replies, registers, and frame
   inputs are read-only; all execution state is owned by the returned witness.

## Verification

Deterministic tests cover empty/wrong-start/non-edge/out-of-range paths,
program/plan mismatch, Phase-60 byte tamper passthrough, completed loop paths,
exact inter-block handoff, non-reapplication of the fixed-frame entry state,
finite-path non-exit semantics, suspension before a requested successor, and
caller input preservation.

A fixed-seed 120-trial corpus reuses a real three-block cyclic out-of-SSA program
that forces spill-frame storage. Each trial varies finite caller register/frame
state and executes a path of 1–18 blocks. A test-local interpreter begins from
the sealed Phase-61 after-entry snapshot and independently replays the retained
addressed register moves, reloads, stores, and no-output instruction
acknowledgements across the caller path. Every per-block after-entry/body state,
final register state, and final frame state must match exactly.

Focused strict-warning compilation of the new production source passes GCC and
Clang against a minimal interface harness. Exact-head repository GCC release,
Clang release, and GCC ASan+UBSan CI remain the authoritative full integration
gate.

## Complexity

Let `P` be path length and let `O_i` be operations in visited block `i`. Phase 64
first pays one complete retained Phase-56 re-derivation. Path preflight costs
`O(P + sum outdegree(path[i]))` under adjacency-list successor checks. Execution
then pays the sealed Phase-63 cost for each started block; the adapter itself
adds `O(P * R)` register-snapshot work plus `O(F)` owned frame state, where `R`
is caller register count and `F` frame-slot count. The returned witness retains
all started block traces, so output storage is proportional to those sealed
Phase-63 traces.

No native execution, ABI, automatic branch semantics, termination proof, or
performance claim is made from CI timing.

## Frontier

Phase 64 is implementation-complete / sealing-audit-pending only after exact
candidate and merged-main GCC release, Clang release, and GCC ASan+UBSan
matrices succeed.

A later phase should advance only if it adds a new executable semantic boundary,
not by inventing branch behavior. One coherent next frontier would be an
explicit caller-supplied termination/exit event that composes the sealed
fixed-frame exit sequence exactly once after a completed finite CFG path.
