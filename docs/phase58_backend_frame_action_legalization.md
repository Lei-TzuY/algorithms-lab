# Phase 58 — immediate-constrained fixed-frame action legalization

## Executable boundary

Phase 58 consumes the sealed Phase-57 `BackendFixedFrameActionPlan` and a
caller-supplied immediate-legality policy. It remains target-neutral: the policy
contains only a positive stack-adjustment magnitude limit and one inclusive
signed interval for frame-base materialization.

A successful plan is transformed as follows:

- the entry stack-pointer movement is split deterministically into maximum-sized
  same-direction chunks followed by one remainder chunk when needed;
- the frame-base materialization remains after all entry movement and is accepted
  only when its exact signed displacement lies in the policy interval;
- exit movement uses the sealed opposite direction and the reverse entry chunk
  magnitudes, so the sequence is a replayable exact inverse; and
- an infeasible Phase-57 plan remains an empty legalized plan.

The result retains both the complete Phase-57 source plan and the policy as
provenance.

## Canonical-source obligation

The legalizer does not trust an arbitrary action vector carrying Phase-57 types.
It re-derives the canonical Phase-57 action plan from `source_plan.source_plan`
and requires structural equality before legalization. This reuses the sealed
Phase-57 witness validation and rejects tampered registers, directions,
magnitudes, materialization operands, extra actions, or malformed nested
Phase-56 provenance.

## Chunk invariant

Let the original stack movement magnitude be `F` and the configured positive
immediate limit be `M`.

For `F > 0`, entry chunk magnitudes are

`M, M, ..., M, F mod M`

with the final remainder omitted when zero. Every chunk is in `[1, M]`, their
exact sum is `F`, and all use the original Phase-57 stack-pointer register and
direction. For `F == 0`, one explicit zero-magnitude action is retained so the
sealed Phase-57 action shape is replayable without manufacturing movement.

Exit chunks use the Phase-57 opposite direction and the exact reverse magnitude
sequence. Consequently entry allocation followed by exit restoration has zero
net stack-pointer movement without signed negation. This preserves the sealed
Phase-57 `2^63` lower-growing boundary on 64-bit `size_t` platforms: for example,
a `2^62` immediate limit produces two legal `2^62` chunks rather than attempting
to represent `+2^63` in `int64_t`.

The number of output chunks is checked against vector representability before
allocation. No arbitrary instruction-count cap is introduced.

## Immediate policy

`maximum_stack_adjustment_immediate_magnitude` must be nonzero. The frame-base
interval must satisfy `minimum <= maximum`. Both endpoints are `int64_t` and the
Phase-57 displacement is compared directly, so no narrowing or target-specific
encoding rule is hidden in the abstraction.

A displacement outside the configured interval is rejected with
`std::out_of_range`; malformed policies use `std::invalid_argument`; malformed or
non-canonical Phase-57 witnesses use the existing Phase-57 validation failures or
`std::logic_error`.

## Verification obligations

Deterministic coverage includes:

- malformed policy rejection;
- infeasible zero-/one-register plans remaining empty;
- exact one-chunk and multi-chunk legalization in both stack directions;
- inclusive frame-base immediate boundaries and out-of-range rejection;
- canonical-source tamper rejection;
- explicit zero-byte frame replay;
- the lower-growing `INT64_MIN` / `2^63` magnitude boundary where supported; and
- repeated deterministic derivation.

A fixed-seed randomized corpus must build real sealed Phase-56/57 plans, apply a
range of chunk limits, and independently replay chunk direction, per-chunk
legality, exact total movement, reversed exit chunks, materialization operands,
and deterministic equality.

The repository GCC/Clang/ASan+UBSan matrix remains the full integration authority.

## Complexity and scope

Canonical Phase-57 re-derivation and provenance copying retain the cost of the
nested source witness. If `K` stack-adjustment chunks are emitted, legalization
adds `O(K)` time and output storage. No stronger constant-time claim is made for
arbitrarily small immediate limits.

This phase does not choose ISA opcodes, instruction encodings, ABI register
names, caller/callee-save rules, prologue/epilogue conventions, red zones, stack
probing, unwind metadata, dynamic `alloca`, variable-sized frames, or
instruction-count optimality. It only proves that the sealed target-neutral
actions fit one explicit immediate policy after deterministic legalization.

Phase 58 remains implementation-complete / sealing-audit-pending until the exact
candidate and merged-main repository matrices are green.
