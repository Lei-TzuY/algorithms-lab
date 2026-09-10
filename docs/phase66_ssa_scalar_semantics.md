# Phase 66 — explicit scalar semantics in SSA

Phase 65 sealed the caller-driven backend because the retained SSA/CFG encoded dataflow and topology but no computation semantics. Phase 66 deliberately changes that premise at the IR boundary instead of inventing behavior in the backend.

## Capability

Canonical `SsaInstruction` now carries an optional-by-default scalar semantic descriptor. Legacy `construct_ssa()` remains source-compatible and produces `opaque` semantics. New callers that possess executable meaning use `construct_semantic_ssa()`, which delegates all versioning, phi placement, reachability, and renaming to the sealed SSA constructor and then attaches validated semantic descriptors to the corresponding renamed instructions.

The first executable scalar vocabulary is intentionally small: `constant_i64`, `copy_i64`, checked `add_i64`, `subtract_i64`, `multiply_i64`, and exact `equal_i64` / `less_than_i64`. `opaque` remains the compatibility/default state.

## Invariants and validation

Each known opcode has one fixed shape. Constants have zero uses, one definition, and one immediate. Copy has one use and one definition. Arithmetic/comparison operations have two uses and one definition. Only constants carry immediates. Opaque instructions may retain the legacy arbitrary use/definition shape but cannot smuggle an immediate.

`evaluate_ssa_scalar_instruction()` requires one runtime operand for every SSA use and rejects opaque or malformed instructions. Signed arithmetic is checked before the C++ operation executes, so no signed-overflow undefined behavior is used as semantics. Comparisons return canonical integer booleans `0` or `1`.

## Compatibility and propagation

This phase does not change caller-selected CFG paths, opaque-instruction oracle replies, branch selection, or termination. Existing aggregate initializers remain valid because the semantic descriptor is a trailing field with an `opaque` default.

SSA destruction already copies each full `SsaInstruction` from the validated SSA block into the corresponding lowered block. Phase-66 tests therefore verify that a semantic descriptor survives both renaming and out-of-SSA lowering without adding a second metadata side table.

## Verification

Deterministic tests cover opcode shape rejection, legacy opaque behavior, exact `INT64_MIN`/`INT64_MAX` boundaries, arithmetic overflow rejection, comparisons, semantic construction, and out-of-SSA preservation. A fixed-seed 5,000-trial small-domain differential compares the scalar evaluator against direct arithmetic/comparison expressions in a domain where the independent oracle cannot overflow.

## Scope boundary

This is an IR semantic substrate, not an automatic backend executor. Phase 66 deliberately does not add branch predicates, terminators, memory operations, calls, ABI behavior, or implicit function completion. A follow-up phase is justified only after this semantic substrate is merged and can be consumed without weakening the sealed Phase-63/64 caller-oracle trust boundary.
