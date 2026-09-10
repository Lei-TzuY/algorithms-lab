# Phase 66 sealing audit

## Integrated checkpoint

Phase 66 reached merged `main@18b73244539faa6b5d22be8ad3c35b415c8556d3` through PR #149. The exact implementation head was `027eec23f22f33166b1589ceea02fcd339754673`. Exact-head CI run `34474243250` completed successfully, and merged-main push CI run `34474919545` completed successfully on GCC release, Clang release, and GCC ASan+UBSan.

At audit time there was no open pull request, no open issue, and no existing Phase-67 branch competing for the next semantic-execution surface.

## Capability sealed

Canonical SSA now carries validated, opt-in scalar semantics for constants, copies, checked signed arithmetic, and integer comparisons while preserving `opaque` as the legacy default. Semantic construction reuses the sealed SSA versioning/phi-placement machinery instead of creating a parallel SSA representation. The full semantic descriptor survives out-of-SSA destruction because the canonical instruction is copied through the lowering boundary.

The scalar evaluator validates opcode shape, requires one runtime operand per SSA use, returns canonical comparison values, and performs checked signed arithmetic without relying on undefined overflow behavior. The Phase-66 differential corpus is independent of the evaluator recurrence over a bounded arithmetic domain.

## Architecture boundary

Phase 66 does not yet make the retained backend self-executing. The sealed Phase-63 instruction continuation still treats every body instruction as an oracle barrier; Phase-64 still requires an explicit caller-selected CFG path; Phase-65 still requires an explicit caller-owned exit event. No branch predicate, terminator, memory operation, call, ABI semantic, or inferred completion exists in the retained IR.

Growing the scalar opcode vocabulary would therefore deepen the wrong surface. The highest-value next integration step is to consume the semantics that now already exist.

## Phase 67 promotion

Phase 67 is promoted as semantic instruction continuation. The backend continuation layer should automatically evaluate only body instructions carrying validated non-opaque Phase-66 scalar semantics from their observed physical-register inputs and materialize the result into the already-allocated output register. `opaque` instructions must retain the sealed caller-oracle contract: supplied replies are consumed in order, and execution suspends honestly when the next opaque result is unavailable.

The implementation must preserve exact operation/origin provenance and reuse the sealed out-of-SSA/backend mapping rather than introducing a second instruction representation. It must not infer CFG successors or termination. Independent verification should interleave known and opaque instructions, cover spilled/reloaded operands and results, exercise exact signed-overflow rejection, and compare automatic scalar results with the Phase-66 evaluator while retaining Phase-63 behavior for fully opaque blocks.
