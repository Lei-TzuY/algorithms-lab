# Phase 59 sealing audit

## Live checkpoint

Phase 59 reached `main@01ec5849e5a346414bfd18af8a6c7b89fdc33720` through PR #135. Exact merged-main CI run `34436410591` completed successfully on GCC release, Clang release, and GCC ASan+UBSan.

At audit time there was no open pull request and no active Phase-60 branch. The existing `phase-59-symbolic-frame-instructions` branch is merged implementation history rather than competing work. The long `ROADMAP.md` remains presentation-only for this late backend continuation; the late phase proof/sealing documents are authoritative.

## Capability sealed

Phase 59 closes the gap between legalized fixed-frame actions and executable target-neutral symbolic instruction semantics.

The sealed contract is:

- consume only a canonical Phase-58 legalized action witness;
- re-derive Phase-58 legalization before selection and reject structural/policy tampering;
- preserve infeasible upstream plans as successful empty instruction plans;
- select one symbolic instruction for every legalized action, in order;
- distinguish stack-pointer decrement, stack-pointer increment, and frame-base materialization explicitly;
- preserve physical-register ids and already-legal immediates exactly;
- retain explicit zero-byte stack movement;
- preserve the direction-plus-`size_t` `2^63` boundary without signed narrowing; and
- retain the complete Phase-58 source plan as provenance.

## Evidence and proof boundary

Deterministic verification covers tampering, both stack-growth directions, exact instruction-form selection, zero-byte movement, infeasible plans, immediate/register preservation, and the 64-bit `2^63` magnitude boundary where supported.

A fixed-seed 240-program corpus is exercised in both stack-growth directions. An independent register-state interpreter replays Phase-58 actions and Phase-59 symbolic instructions separately and requires identical entry/exit state. Repeated lowering is deterministic.

The merged-main GCC/Clang/ASan+UBSan matrix is the authoritative integration evidence. The randomized corpus is evidence for the implementation; correctness still rests on canonical-witness revalidation and the one-action-to-one-instruction selection invariant.

## Architecture audit and Phase 60 promotion

The fixed-frame backend now has a coherent target-neutral chain:

`spill lowering -> register reservation -> frame layout -> base-relative addressing -> frame-base reservation -> stack-directed coordinates -> stack-pointer ownership -> fixed-frame actions -> immediate legalization -> symbolic instruction selection`.

Another symbolic-instruction enum or immediate-policy variant would farm the same surface. The next substantial executable gap is a concrete **repository-defined byte representation** that can be decoded and replayed independently.

Phase 60 is therefore canonical fixed-frame byte encoding. It should:

- consume only a canonical Phase-59 symbolic plan and revalidate the Phase-59 witness;
- define explicit versioned opcode bytes for the three sealed symbolic forms;
- serialize register ids and immediates with a fixed host-independent integer byte order/width policy;
- preserve entry/exit instruction boundaries and provenance;
- reject values that the encoding cannot represent rather than truncate them;
- provide decoding that rejects unknown opcodes, truncated records, malformed lengths, and non-canonical encodings; and
- verify decode/execute semantics independently against Phase-59 symbolic replay across deterministic boundary cases and a fixed-seed upstream corpus.

This promoted encoding is educational backend bytecode only. It must not claim real ISA opcode compatibility, ABI register numbers, executable object code, relocation behavior, calling conventions, unwind metadata, red zones, stack probing, variable-sized frames, or encoding optimality.

## ROADMAP presentation note

As recorded by the sealed Phase-56 through Phase-58 audits, `ROADMAP.md` intentionally lags this late backend continuation. This audit and the Phase-59 proof document are authoritative for the Phase-59 seal and Phase-60 promotion; rewriting the long historical roadmap would add churn without executable value.
