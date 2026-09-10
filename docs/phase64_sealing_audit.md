# Phase 64 sealing audit

## Integrated checkpoint

Phase 64 reached merged `main@5da077c705f97e89da21349861e1a96df3cfa7db` through PR #145. The exact implementation-head CI run `34460706642` and exact merged-main push CI run `34461222150` both completed successfully on GCC release, Clang release, and GCC ASan+UBSan.

## Capability sealed

The repository can now compose the sealed Phase-63 instruction-result continuation across an explicit finite caller-supplied path in the bound out-of-SSA CFG without fabricating branch semantics. Production validates the canonical program/backend provenance, requires the path to start at `program.start`, validates every requested directed transition, preserves repeated/self-loop visits, hands register and frame state exactly across completed blocks, and stops before any requested successor when an opaque instruction suspends.

A key integration invariant is that later block visits do not semantically reapply the fixed-frame prologue: only the dedicated stack/frame registers are normalized back to their original caller values before sealed Phase 61 reconstructs the canonical entered state, and that reconstructed state must exactly equal the previous block's completed register state. Frame-slot state is handed directly.

Deterministic regressions cover malformed paths, provenance and byte tampering, cyclic paths, fixed-frame entry non-reapplication, finite-path non-exit semantics, suspension barriers, and caller immutability. A fixed-seed 120-trial cyclic spill/reload corpus executes caller-selected paths of 1–18 blocks and compares every block/final register and frame state against a test-local addressed-operation interpreter rather than replaying the production continuation code.

## Architecture audit

No unresolved Phase-64 correctness blocker was found. More path shapes or successor variants would refine the same caller-oracle boundary rather than add a new semantic capability.

Three limits remain deliberate and must not be blurred by promotion:

- `SsaInstruction` still contains no opcode/computation semantics, so opaque instruction results remain caller supplied;
- the retained CFG still contains no runtime branch predicate or automatic successor-selection semantics, so the caller must continue to choose the path;
- exhausting a finite caller path is not itself evidence of function termination, and Phase 64 intentionally does not execute the fixed-frame exit sequence.

Concrete ISA instructions, ABI/calling-convention behavior, native memory/system-call semantics, object loading, relocation, and automatic whole-program execution remain outside the sealed capability.

## Phase 65 promotion

The next coherent executable boundary is an explicit caller-supplied termination/exit event composed after a fully completed finite Phase-64 path. The promoted slice should:

- retain the sealed Phase-64 program/path/reply validation and exact inter-block state handoff;
- distinguish ordinary finite path exhaustion from an explicit caller termination request;
- consume the termination event only if every requested path block completed without suspension;
- execute the canonical fixed-frame exit instruction sequence exactly once from the completed path register state;
- preserve frame-slot values and all non-frame registers except for effects explicitly encoded by the sealed exit sequence;
- expose whether the exit event was requested and actually consumed plus the post-exit register witness;
- prove by regression that re-running the fixed-frame entry sequence before exit would be incorrect and is not done;
- keep all caller-owned program/path/reply/register/frame inputs immutable.

Phase 65 must not infer that the final requested block is a return block: the current IR lacks such a terminator semantic. The caller-supplied event remains the semantic oracle for termination just as the caller path remains the oracle for control-flow choice.

## Governance

The long ROADMAP presentation intentionally lags these late-backend phases. Commit chronology plus the per-phase proof and sealing documents remain authoritative; this seal therefore does not rewrite ROADMAP or unrelated status files.
