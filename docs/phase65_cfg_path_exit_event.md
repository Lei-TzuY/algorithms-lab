# Phase 65 — explicit caller-supplied CFG exit event

## Scope

Phase 65 adds exactly one semantic boundary on top of sealed Phase 64: the
caller may explicitly request fixed-frame exit after a finite caller-supplied
CFG path has completed. Calling `execute_backend_cfg_path_exit_event` is that
exit event. The existing Phase-64 API remains the non-terminal path-only API;
finite path exhaustion by itself still does not mean return or termination.

Production does not infer a return block, evaluate a branch predicate, select a
successor, derive an opaque instruction result, or decide that control flow has
terminated. The caller still owns the path and all Phase-63 instruction replies.

## Composition

The implementation first executes the complete request through sealed Phase 64.
If any visited block suspends, Phase 65 returns the exact suspended Phase-64
state with `exit_event_consumed == false`; no fixed-frame exit replay occurs.

If the complete caller path finishes, Phase 65 requires
`completed_path_blocks == path.size()`. It then takes the exact Phase-64 terminal
register state, resets only the sealed stack-pointer and frame-base registers in
a private replay input to their original function-entry caller values, and calls
the sealed Phase-61 fixed-frame executor.

Phase 61 replays the canonical entry sequence before its exit sequence. Phase 65
therefore requires Phase-61 `after_entry_registers` to equal the exact completed
Phase-64 terminal registers. This equality proves that the private normalization
reconstructed the already-entered frame state rather than semantically applying
a second prologue. Only then is Phase-61 `after_exit_registers` accepted as the
Phase-65 final register state and the event marked consumed.

Frame-slot values are preserved as the exact terminal Phase-64 witness. Phase 65
does not claim stack-memory deallocation, object lifetime, ABI epilogue rules,
or any other memory semantics that the educational backend does not encode.

## Invariants and proof obligations

1. **Explicit-event invariant.** Fixed-frame exit occurs only through this
   dedicated caller-invoked API. Plain Phase-64 path exhaustion remains
   non-terminal.
2. **Complete-path invariant.** The event can be consumed only after every
   requested path block completed.
3. **Suspension invariant.** A Phase-63 suspension prevents event consumption
   and preserves the exact Phase-64 suspended register/frame state.
4. **Canonical pre-exit invariant.** Before exit is accepted, sealed Phase-61
   entry replay must reconstruct the exact Phase-64 terminal register state.
5. **Exactly-once invariant.** One successful Phase-65 call executes one
   canonical fixed-frame exit sequence; no second exit is inferred or replayed.
6. **No-control-fabrication invariant.** Phase 65 adds no return-block,
   successor, branch-predicate, or automatic termination semantics.
7. **Caller-immutability invariant.** Program, path/replies, registers, and
   frame-slot inputs remain caller-owned and read-only.

## Verification

Deterministic tests cover a completed cyclic path, exact embedding of the sealed
Phase-64 result, pre-exit reconstruction equality, stack-pointer restoration,
and the fact that the completed path's pre-exit stack pointer differs from the
function-entry value. Together these make accidental double-exit or missing-exit
behavior observable.

A suspension regression supplies a complete first block and no reply for the
next opaque instruction. The returned Phase-64 state must suspend at that block,
`exit_event_consumed` remains false, no fixed-frame replay is present, and the
entered stack pointer remains un-restored.

A fixed-seed 120-trial corpus executes 1–18 blocks around the real three-block
cyclic spill/reload fixture from Phase 64. For each completed path, a test-local
interpreter applies the retained symbolic **exit instructions only** to the exact
Phase-64 terminal registers and compares the resulting register file with the
Phase-65 final registers. This independently checks the new exit-composition
step instead of using the production Phase-61 executor as the expected result.
Caller-input immutability and malformed-bytecode rejection are also retained as
explicit regressions.

Focused source-level review is precursor evidence only. The exact-head GitHub
Actions matrix on GCC release, Clang release, and GCC ASan+UBSan is the
authoritative integration gate because the sandbox cannot clone the repository.

## Complexity and claim boundary

Phase 65 pays the complete sealed Phase-64 path cost plus one sealed Phase-61
fixed-frame replay. The adapter adds `O(R)` private register normalization and
snapshot storage for `R` caller registers; frame-slot witness storage remains
that of the Phase-64 result. The fixed-frame replay is linear in its small
canonical entry/exit instruction sequence.

This phase does not claim native execution, ABI/calling-convention semantics,
return-address behavior, automatic branch/return semantics, stack-memory
lifetime, unwind behavior, or performance from CI timing.

## Frontier

Phase 65 is implementation-complete / sealing-audit-pending only after the exact
candidate and merged-main GCC release, Clang release, and GCC ASan+UBSan matrices
succeed. A later frontier must be chosen by architecture audit from semantics
actually present in the retained IR; it must not fabricate branch predicates,
return blocks, or automatic whole-program termination merely to keep expanding.
