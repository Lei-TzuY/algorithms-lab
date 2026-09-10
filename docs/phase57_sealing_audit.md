# Phase 57 sealing audit

## Live checkpoint

Phase 57 reached `main@f457a88ded9aecf1fb020bb65024c21070bce180`
through PR #131. The exact post-merge CI run `34425593986` completed successfully
on all repository gates: GCC release, Clang release, and GCC ASan+UBSan.

At audit time there was no open implementation pull request, no open issue, and
no `phase-58*` branch competing for the next backend surface.

## Capability sealed

Phase 57 consumes the sealed Phase-56 stack-pointer ownership and Phase-55
stack-directed frame-coordinate witness and emits an ordered, target-neutral
fixed-frame action plan.

For every successful addressed frame the sealed contract is exactly:

1. move the owned stack pointer by the fixed frame size in the stack-growth
   direction;
2. materialize the owned frame-base register from the adjusted stack pointer at
   the exact Phase-55 signed displacement;
3. move the same stack pointer by the same magnitude in the opposite direction
   on exit.

Nested infeasibility remains represented by empty action sequences rather than
an exception. The complete Phase-56 source plan remains attached as provenance.

The stack movement representation deliberately separates direction from a
`size_t` magnitude. That preserves the legal Phase-55 lower-growing
`INT64_MIN` entry displacement and its positive `2^63` inverse on a 64-bit host
without signed negation overflow.

## Evidence and invariant boundary

Deterministic tests cover zero-/one-register and nested-shortage cases, both
stack-growth directions, exact action ordering and register/displacement replay,
repeated deterministic derivation, the `INT64_MIN` inverse boundary, and
rejection of malformed ownership/coordinate witnesses.

A fixed-seed 360-program corpus derives real Phase-56 plans under both stack
directions and independently replays every Phase-57 action operand against the
sealed Phase-55/56 witness. The merged-main GCC/Clang/ASan+UBSan matrix is the
authoritative integration evidence.

No concrete opcode, encoded immediate width, ABI register naming,
caller/callee-save rule, return-address convention, stack probing, unwind
metadata, red zone, dynamic allocation, or variable-sized frame is claimed.

## Architecture audit and Phase 58 promotion

The fixed-frame backend now has a complete machine-independent chain from spill
layout and signed frame coordinates through owned stack/frame-base registers to
ordered entry/exit actions. The next meaningful gap is **legality**, not another
copy of the action model.

Phase 58 should therefore add a target-neutral legalization layer around the
sealed Phase-57 plan. A caller-supplied policy should bound stack-adjustment
immediate magnitude and frame-base signed-immediate range. Production should
split oversized stack movements deterministically, preserve exact cumulative
movement and entry/exit inversion, validate frame-base displacement without
narrowing, preserve infeasible empty plans, and retain the full Phase-57 plan as
provenance.

This remains intentionally below instruction selection: no opcode set,
encoding, ABI name, caller/callee-save assignment, unwind rule, or
instruction-count optimality is implied.

## ROADMAP presentation note

`ROADMAP.md` currently lags this late backend sequence and still presents the
older Phase-53 frontier. Following the existing Phase-56/57 governance pattern,
this sealing audit and the Phase-57 proof document are authoritative for the
Phase-57 seal and Phase-58 promotion; the seal does not rewrite the historical
ROADMAP presentation merely to inflate documentation churn.
