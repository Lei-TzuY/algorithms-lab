# Phase 70 — explicit void return and canonical semantic exit

## Scope

Phase 70 adds one explicit function-termination boundary on top of sealed Phase
69. Successor control and function termination remain distinct: the existing
`SsaControlTerminatorKind` continues to describe opaque/jump/conditional
successor behavior, while `SsaControlTerminationKind::return_void` is a
payload-free termination descriptor. A return carries no predicate, return
value, logical successor, or lowered execution successor.

The first return slice is deliberately `void`. Return values, calls, general
memory, exceptions, ABI/calling-convention semantics, target-ISA/native
execution, and unbounded execution remain out of scope.

## Construction and provenance

A source `return_void` is accepted only on a start-reachable CFG sink. Combining
return termination with jump/branch control or with any successor/predicate
payload is rejected. Unreachable return descriptors are rejected rather than
silently becoming executable semantics.

The descriptor is attached to the lowered out-of-SSA block and copied into the
machine-independent backend control record during spill lowering. Later register
reservation/frame/addressing/bytecode planning preserves complete backend block
control records, so semantic execution can validate the same return provenance
that was constructed before allocation and spill lowering. No new temporary,
liveness use, register, stack slot, or CFG edge is invented for return_void.

## One-block semantic return

`execute_backend_semantic_control_block` always executes the block body first.
If the body suspends on an opaque instruction, return is not consumed and the
status remains `body_suspended`. Only after body completion may a validated
return descriptor produce `BackendSemanticControlStatus::returned`.

A returned block has no predicate value and no logical or execution successor.
The semantic-control layer reports termination but does not itself run the frame
exit sequence.

## Bounded CFG composition

`execute_backend_semantic_cfg` recognizes the returned status as distinct from
body suspension, opaque control, and visit-budget exhaustion. It increments the
completed-visit count for the completed return block, then performs the sealed
Phase-65 canonical fixed-frame exit composition exactly once:

1. retain the exact post-body registers as the pre-exit state;
2. privately restore only the reserved stack-pointer and frame-base inputs to
   their original function-entry caller values;
3. execute the sealed fixed-frame bytecode replay;
4. require replay `after_entry_registers` to equal the exact pre-exit register
   state, proving that entry replay reconstructed the already-entered frame
   rather than semantically applying a second prologue;
5. accept `after_exit_registers` as the final register file and expose the replay
   witness; and
6. stop immediately with `BackendSemanticCfgStopReason::returned`.

Frame-slot values remain the exact final body witness. This phase does not claim
stack-memory lifetime/deallocation semantics.

## Invariants and proof obligations

1. **Explicit-termination invariant.** Only an explicit return_void descriptor
   can produce `returned`; a CFG sink alone is never termination.
2. **Sink invariant.** Every executable return_void block is start-reachable and
   has no outgoing CFG edge.
3. **Payload-free invariant.** return_void carries no predicate or successor
   payload and cannot be combined with jump/conditional control.
4. **Body-first invariant.** Suspension prevents return consumption and fixed-
   frame exit.
5. **Provenance invariant.** The return descriptor survives out-of-SSA lowering,
   allocation/spill lowering, and physicalization unchanged.
6. **Canonical pre-exit invariant.** Fixed-frame entry replay must reconstruct the
   exact post-return-block register state before exit is accepted.
7. **Exactly-once invariant.** One completed return visit triggers one fixed-frame
   exit replay and stops the bounded executor immediately.
8. **Non-terminal boundaries remain non-terminal.** Opaque control and finite
   visit-budget exhaustion never consume return/exit semantics.
9. **Caller immutability.** Program, visit scripts, registers, and frame-slot
   inputs remain caller-owned and read-only.

## Verification

A dedicated Phase-70 test translation unit checks:

- non-sink, unreachable, payload-bearing, and successor-combined returns are
  rejected;
- a valid sink return survives lowering into backend control provenance and the
  one-block semantic executor reports `returned` with no successor;
- a two-block semantic program executes a jump into a return block, stops before
  unused visit-budget entries, exposes the fixed-frame replay, requires exact
  pre-exit reconstruction, preserves frame-slot state, and restores the caller's
  stack pointer through the canonical exit;
- a return block containing an opaque body instruction suspends without an exit
  replay, then returns only when the missing instruction reply is supplied.

The full repository GitHub Actions matrix on GCC release, Clang release, and GCC
ASan+UBSan is the authoritative integration gate. Existing Phase-68/69 tests
remain unchanged and therefore continue to guard legacy successor, suspension,
opaque-control, critical-edge, and budget-exhaustion behavior.

## Complexity and claim boundary

The new return descriptor adds constant metadata per block. Return validation is
constant apart from checking that the return block's adjacency list is empty.
The semantic CFG return path pays the already-sealed completed block cost plus
one fixed-frame bytecode replay and `O(R)` private register reconstruction for
`R` caller registers. No asymptotic improvement or performance claim is inferred
from CI timing.

## Frontier

Phase 70 is implementation-candidate scope only until exact-head CI, merge gates,
and merged-main CI succeed. No later phase is promoted from this document before
those integration gates close.
