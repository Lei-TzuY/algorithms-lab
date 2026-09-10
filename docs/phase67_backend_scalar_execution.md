# Phase 67 — semantic backend instruction continuation

## Scope

Phase 67 connects the sealed Phase-66 scalar SSA semantics to the existing
Phase-63 instruction-continuation executor without introducing a second runtime
or weakening the opaque-instruction boundary.

Known scalar descriptors are retained through the actual backend provenance path:

`SsaInstruction -> BackendOperation -> ByteAddressedBackendOperation -> BaseRelativeBackendOperation`.

The existing fixed-frame bytecode plan already retains the addressed backend
source plan, so the continuation API does not accept an additional, weakly bound
`OutOfSsaProgram` argument. Phase-64 CFG-path continuation and Phase-65 exit-event
execution automatically inherit this instruction behavior because both reuse the
Phase-63 block continuation boundary.

## Executable semantics

For a backend body instruction:

- `constant_i64`, `copy_i64`, `add_i64`, `subtract_i64`, `multiply_i64`,
  `equal_i64`, and `less_than_i64` are evaluated from the materialized register
  inputs through the sealed Phase-66 checked scalar evaluator;
- the derived value is written only to the already-materialized output register;
- known scalar instructions consume no caller reply and never suspend merely for
  lack of an oracle reply;
- `opaque` retains the Phase-63 ordered reply-prefix contract exactly, including
  explicit `nullopt` acknowledgement for no-output opaque instructions;
- execution still suspends honestly before the next opaque instruction when the
  caller prefix is exhausted.

Replay steps distinguish `instruction_semantically_evaluated` and
`derived_instruction_result` from the pre-existing opaque
`instruction_acknowledged` / `supplied_instruction_result` evidence.

## Validation and invariants

1. **Descriptor-shape invariant.** Before continuation mutates runtime state,
   every body instruction descriptor is checked against the materialized input
   count and output presence using the sealed Phase-66 validation rules.
2. **Transfer-opacity invariant.** Register moves, reloads, and stores may not
   carry scalar instruction semantics. A non-opaque descriptor on a transfer is
   treated as backend-provenance corruption.
3. **Opaque-prefix invariant.** Caller replies form a prefix of opaque body
   instructions only. Known scalar instructions cannot be overridden through a
   reply and do not shift the opaque reply order.
4. **Checked-arithmetic invariant.** Phase-66 add/subtract/multiply overflow is
   propagated as `std::overflow_error`; no wrapped result is written.
5. **Provenance invariant.** Scalar descriptors are preserved through spill
   lowering, scratch-register physicalization, byte-addressed frame layout, and
   base-relative addressing. Existing operation origin indices and storage
   provenance remain unchanged.
6. **No-fabrication invariant.** Production derives values only for the finite
   Phase-66 scalar opcode set. Opaque instructions remain caller-owned.

## Verification

The dedicated Phase-67 suite exercises a real pipeline from
`construct_semantic_ssa()` through SSA destruction, register/spill lowering,
frame layout/addressing, fixed-frame bytecode, and instruction continuation. A
mixed program proves that known constants/arithmetic execute automatically while
an intervening opaque instruction still suspends and consumes exactly one reply.

A separate fixed-seed randomized executor differential compares bounded binary
scalar operations against a test-local arithmetic/comparison oracle rather than
calling the production scalar evaluator. Deterministic regressions reject reply
overrides of known instructions, malformed known descriptors, semantic data on
transfer operations, and checked arithmetic overflow. The pre-existing opaque
Phase-63 randomized script replay remains unchanged and therefore continues to
protect backward compatibility.

## Complexity

Semantic dispatch adds `O(k)` validation for the `k` backend operations in a
block and `O(arity)` work per known scalar instruction. The current scalar opcode
set has arity at most two, so this does not change the asymptotic linear block
execution bound. Storage remains linear in the existing execution trace.

## Explicit non-goals

Phase 67 does **not** choose CFG successors, infer program termination, interpret
branches, add loads/stores over a general memory model, execute calls, define an
ABI, or claim target ISA semantics. CFG path and exit ownership remain exactly
where the sealed Phase-64/65 contracts place them.

## Sealed status

Phase 67 is **SEALED** after implementation PR #151 reached merged
`main@615cf49baf85502e18eef7aa95951767e99a6a77` and merged-main CI run
`34488175900` completed successfully on GCC release, Clang release, and GCC
ASan+UBSan. The architecture audit found no unresolved scalar-execution blocker.

The promoted frontier is an explicit control/terminator semantic boundary. The
current retained IR has CFG topology but no branch/terminator descriptor, so a
future executor must not infer successor choice from adjacency order. More scalar
opcode variants alone would not constitute the next architectural phase.
