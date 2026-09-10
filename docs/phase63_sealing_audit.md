# Phase 63 sealing audit

## Integrated checkpoint

Phase 63 reached merged `main@a86e4cc9c48b360d592649262a0d88ac8973c8f8` through PR #143. The exact implementation-head CI run `34451215797` and exact merged-main push CI run `34451828432` both completed successfully on GCC release, Clang release, and GCC ASan+UBSan.

## Capability sealed

The repository can now continue one canonical addressed backend block across opaque instruction barriers without inventing instruction semantics. Production reuses the sealed Phase-62 provenance/preflight boundary, records the actual materialized register inputs observed at each instruction, consumes only an ordered caller-supplied acknowledgement/result prefix, writes only explicitly supplied scalar results to already-materialized outputs, and resumes known register/frame transfers. Script exhaustion produces an explicit suspension checkpoint.

Deterministic and fixed-seed differential tests cover result injection, no-output acknowledgement, multiple barriers, ordering and shape rejection, caller immutability, transfer continuation, suspension, retained Phase-62 provenance rejection, and real upstream spill/reload integration. The 180-by-60 mixed-operation corpus is checked against a test-local scalar interpreter rather than replaying the production state machine.

## Architecture audit

No unresolved Phase-63 correctness blocker was found. More reply-shape or same-block transfer variants would refine the sealed state machine rather than add a new execution boundary.

One semantic limit is fundamental and remains explicit: `SsaInstruction` records dataflow uses and an optional definition but no opcode/computation, so instruction results must remain caller supplied. Likewise the current late-backend witness does not itself encode runtime branch predicates or a chosen successor. Automatically selecting the next CFG edge would therefore fabricate control semantics.

The canonical `BackendFixedFrameBytecodePlan` retains the backend frame/register/provenance chain but not a standalone trustworthy CFG/start field at the execution API surface. A cross-block executor must therefore receive or revalidate the sealed `OutOfSsaProgram` control-flow provenance rather than infer graph edges from addressed block ordering or `original_block` labels.

## Phase 64 promotion

The next coherent slice is caller-supplied CFG-path continuation. It should:

- bind the execution plan to a sealed `OutOfSsaProgram` control-flow witness before following caller transitions;
- require the path to begin at the canonical program start;
- validate every consecutive block transition against the directed CFG, including repeated vertices/loops without inventing a successor choice;
- associate an explicit instruction-reply script with every visited block;
- compose Phase-63 executions by handing the owned register/frame state from one completed block to the next exactly;
- forbid transition to a successor when the current block suspends at an unacknowledged opaque instruction;
- expose per-block executions and the final/suspended state as a replayable witness;
- keep caller program/path/register/frame/reply inputs immutable.

A finite caller path is an oracle for control-flow choice, not evidence that production understands branch predicates or termination semantics. Native ISA/ABI behavior, memory instructions, branch computation, system calls, and automatic whole-program execution remain outside this promotion.

## Governance

The long ROADMAP presentation has intentionally lagged the late-backend continuation since earlier sealed phases. Commit chronology plus the per-phase proof/sealing documents remain authoritative here; this seal does not perform a risky unrelated ROADMAP rewrite.
