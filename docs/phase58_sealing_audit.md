# Phase 58 sealing audit

## Live checkpoint

Phase 58 reached `main@bca64abdf3ec9caf8a6895db5545c232e5015291` through PR #133. The exact merged-main CI run `34429736965` completed successfully on all repository gates: GCC release, Clang release, and GCC ASan+UBSan.

At audit time there was no open pull request, no open issue, no `phase-58-seal` branch, and no Phase-59 branch competing for the next backend surface. The old `phase-58-frame-action-legalization` implementation branch is already merged history rather than active work.

## Capability sealed

Phase 58 consumes only a canonical sealed Phase-57 fixed-frame action witness and legalizes it against one explicit target-neutral immediate policy.

The sealed contract is:

- stack-adjustment magnitude limit is positive;
- frame-base immediate interval is well formed and inclusive;
- the Phase-57 source witness is re-derived and structural tampering is rejected;
- infeasible upstream plans remain successful empty legalized plans;
- every nonzero stack movement is split deterministically into same-direction chunks no larger than the configured magnitude;
- explicit zero-byte movement remains explicit rather than disappearing;
- frame-base materialization keeps its exact signed displacement and must fit the configured interval without narrowing;
- exit chunks replay the entry chunk magnitudes in reverse order using the sealed opposite direction; and
- the complete Phase-57 source plan and legality policy remain attached as provenance.

The direction-plus-`size_t` magnitude representation preserves the legal lower-growing `INT64_MIN` / positive `2^63` inverse boundary on 64-bit hosts without signed negation overflow.

## Evidence and invariant boundary

Deterministic verification covers malformed policies, canonical-source tampering, infeasible empty plans, both stack-growth directions, exact chunk ordering, inclusive materialization boundaries, explicit zero-byte frames, and the 64-bit `2^63` magnitude boundary where supported.

A fixed-seed 320-program corpus runs both stack-growth directions for 640 real upstream Phase-56/57 witnesses. Independent replay checks per-chunk legality, exact cumulative movement, reverse exit chunks, materialization range, provenance, and deterministic equality.

The merged-main GCC/Clang/ASan+UBSan matrix is the authoritative integration evidence. No machine opcode, instruction encoding, ABI register name, caller/callee-save convention, red zone, stack probing, unwind metadata, dynamic allocation, variable-sized frame, or instruction-count optimality is claimed.

## Architecture audit and Phase 59 promotion

The fixed-frame backend chain now reaches a clean legality boundary:

`spill lowering -> register reservation -> byte frame layout -> base-relative addressing -> frame-base reservation -> stack-directed entry coordinates -> stack-pointer ownership -> fixed-frame actions -> immediate legalization`.

Another immediate-policy variant would only farm the same surface. The next coherent executable gap is **symbolic fixed-frame instruction lowering**: consume the canonical Phase-58 legalized plan and select explicit target-neutral instruction forms whose operands can be executed/replayed.

Phase 59 should therefore introduce a small symbolic instruction vocabulary rather than byte encodings. At minimum it should distinguish stack-pointer decrement-immediate, stack-pointer increment-immediate, and frame-base-from-stack-pointer-plus-signed-immediate forms; preserve physical-register ids and already-legal immediates exactly; revalidate the complete Phase-58 witness; preserve infeasible empty plans; and expose deterministic entry/exit instruction sequences.

Verification should include an independent instruction-state replayer that checks selected instruction effects against the Phase-58 action semantics, plus fixed-seed real upstream plans across both stack directions and varied legality policies. This makes instruction selection executable rather than a renamed action enum.

Concrete ISA opcode numbers/bytes, ABI register names, save/restore conventions, return-address handling, red zones, stack probing, unwind metadata, dynamic allocation, variable-sized frames, and target-specific instruction optimality remain out of scope.

## ROADMAP presentation note

`ROADMAP.md` is known to lag the late backend continuation. Following the sealed Phase-56/57 governance pattern, this audit and the Phase-58 proof document are authoritative for the Phase-58 seal and Phase-59 promotion; the seal does not rewrite the long historical ROADMAP merely to create documentation churn.
