# Phase 60 sealing audit

## Live checkpoint

Phase 60 reached `main@a4c0601739a1ceda8bedb66aeaff9ccfbb855e4c`. Exact merged-main CI run `34440248577` completed successfully on GCC release, Clang release, and GCC ASan+UBSan. At audit time there was no open pull request, no open issue, and no active Phase-61 branch. The existing `phase-60-fixed-frame-bytecode` branch is merged implementation history rather than competing work.

As established by the late-backend sealing audits, the long `ROADMAP.md` intentionally remains presentation-only for this continuation. The phase proof and sealing documents are authoritative for Phase 53 onward; rewriting the historical roadmap is not required to prove executable progress.

## Capability sealed

Phase 60 closes the representation gap between sealed Phase-59 symbolic fixed-frame instructions and a concrete repository-defined byte stream.

The sealed contract is:

- consume only a canonical Phase-59 symbolic instruction plan and re-derive that witness before encoding;
- encode entry and exit sequences with fixed magic, version, explicit instruction counts, fixed-width little-endian operands, and one opcode per sealed symbolic form;
- preserve register ids and immediates exactly when representable in the fixed `u64` wire domain;
- serialize signed frame-base displacement by its exact modulo-`2^64` bit pattern and reconstruct it without implementation-defined narrowing;
- decode one complete stream strictly, rejecting bad magic/version/opcodes, truncation, impossible count claims, host-unrepresentable operands, and trailing bytes; and
- retain the full Phase-59 source plan alongside encoded bytes as provenance in the canonical encoded-plan value.

This is repository bytecode only. It is not an ISA, ABI, object format, relocation format, executable machine code, instruction-size-optimal encoding, or target compatibility claim.

## Evidence and proof boundary

Deterministic verification includes one byte-for-byte known encoding, both stack-growth directions, negative signed displacement, canonical-plan tampering, malformed/truncated/trailing streams, zero immediates, and the `2^63` fixed-width magnitude/displacement boundary where the host can represent it.

A fixed-seed 320-case upstream corpus is exercised in both stack-growth directions. For each canonical plan, encoding is deterministic, strict decode reproduces the Phase-59 entry/exit instruction vectors, and a test-local raw-byte interpreter independently replays the serialized opcodes against a register state and is required to match a separate Phase-59 symbolic interpreter.

The exact merged-main GCC/Clang/ASan+UBSan matrix is the authoritative integration evidence. The corpus is implementation evidence rather than a theorem proof; correctness rests on canonical-witness revalidation, the one-to-one opcode schema, explicit integer serialization, and strict complete-stream parsing.

## Architecture audit

No unresolved Phase-60 correctness blocker was found. The encoder and decoder have intentionally different trust boundaries: encoding proves canonical provenance by re-deriving Phase 59, while the standalone decoder validates the byte grammar only because raw bytes do not contain the upstream provenance witness.

The main remaining architectural gap is therefore not another opcode or immediate-policy variant. Execution semantics exist only as test-local independent replay evidence. Production has no reusable operation that consumes a canonical Phase-60 encoded plan, revalidates source-to-byte identity, executes the entry and exit sequences over an explicit finite register file, and exposes checked state snapshots.

## Phase 61 promotion

Phase 61 is **canonical fixed-frame bytecode execution**. It should add one target-neutral production execution boundary with these obligations:

- consume a complete `BackendFixedFrameBytecodePlan`, re-encode its Phase-59 source plan, and require exact equality of the canonical bytes before execution;
- strictly decode the byte stream before state mutation;
- execute against a caller-supplied finite signed register file, rejecting any referenced physical-register id outside that file instead of growing hidden state;
- implement stack-pointer increment/decrement with exact checked arithmetic across the full representable Phase-60 magnitude domain, including the `2^63` boundary on 64-bit hosts;
- implement frame-base materialization with checked signed displacement arithmetic;
- return explicit after-entry and after-exit register snapshots while leaving caller-owned input untouched if validation or arithmetic fails; and
- verify production execution against an independent raw-byte interpreter across deterministic boundary cases plus a fixed-seed canonical upstream corpus.

The promoted executor is still an educational target-neutral semantics engine. It must not claim native execution, real machine registers, memory effects, body-instruction execution, ABI conformance, calling-convention behavior, object loading, relocation, or target opcode compatibility.
