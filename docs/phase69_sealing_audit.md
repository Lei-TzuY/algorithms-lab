# Phase 69 sealing audit — bounded semantic CFG execution

## Verified checkpoint

Phase 69 implementation PR #155 was merged as
`main@8167fe4dd2b71f3dc20a8f91410d913fa8e8d786` after its exact candidate
converged on the repository GCC release, Clang release, and GCC ASan+UBSan
gates. The exact merged-main push run `34510858503` subsequently completed
successfully on the same three-job matrix.

At sealing time there is no open pull request, no Phase-69 seal branch, and no
Phase-70 implementation branch. The merged implementation therefore remains the
only active semantic-CFG execution surface.

## Capability sealed

The backend can now begin at canonical `program.start`, execute a bounded number
of concrete dynamic CFG visits, reuse the sealed Phase-68 semantic-control block
executor on every visit, and follow only explicit concrete execution successors.
Opaque body instructions remain caller-replied per dynamic visit; opaque control
stops honestly instead of guessing an edge. Compiler-created critical-edge split
blocks execute as real visits before their logical successors, and exact
register/frame state is handed from one visit to the next under the sealed
Phase-64 reserved-frame-register invariant.

Three stop conditions remain intentionally distinct: body suspension, opaque
control, and finite visit-budget exhaustion. Budget exhaustion records the exact
next concrete block when one was selected, but it is not function termination.

## Audit findings

No unresolved correctness blocker was found in canonical start ownership,
per-dynamic-visit reply ownership, explicit successor selection, critical-edge
split execution, repeated loop visits, exact inter-block state handoff, or the
suspension-before-control ordering. The merged-main sanitizer run covers the
complete retained SSA destruction, allocation, spill/frame, instruction,
semantic-control, and bounded-CFG integration chain.

The remaining semantic gap is not another bounded-walk variant. The retained
control vocabulary has no explicit return terminator, so production still cannot
know when a function has completed. Phase-65 provides a caller-owned fixed-frame
exit event, but Phase 69 must not invoke it merely because the visit budget ends
or a graph sink is reached.

## Promoted frontier — Phase 70

Promote one explicit repository-defined return boundary through the existing
control pipeline:

1. add a `return_void` semantic terminator with no predicate or CFG-successor
   payload;
2. require an executable return block to be a reachable CFG sink rather than
   silently ignoring outgoing control-flow edges;
3. preserve the return descriptor through SSA destruction, allocation, spill
   lowering, register reservation, frame/addressing provenance, and backend
   semantic-control validation;
4. after the return block body completes, report an explicit return status rather
   than a selected successor;
5. make the bounded semantic CFG executor stop with a distinct `returned` reason
   and execute the sealed canonical fixed-frame exit sequence exactly once;
6. reconstruct and validate the canonical pre-exit register state before accepting
   the exit, as the sealed Phase-65 boundary already requires; and
7. keep body suspension, opaque control, and visit-budget exhaustion non-terminal.

The first return slice is deliberately `void`. Return values, general memory,
calls, exceptions, ABI/calling-convention semantics, target-ISA/native execution,
and unbounded execution remain separate frontiers.

## Scope decision

Phase 69 is **SEALED** at the verified merged-main checkpoint above. The long
ROADMAP remains intentionally presentation-lagged in the late-backend sequence;
commit chronology plus per-phase proof/sealing documents remain authoritative.
Further loop-shape, budget, or reply-script variants would refine an already
verified boundary without adding a comparable semantic capability.
