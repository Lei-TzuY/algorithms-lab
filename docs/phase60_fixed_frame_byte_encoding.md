# Phase 60 — canonical fixed-frame byte encoding

## Scope

Phase 60 consumes only a canonical sealed Phase-59 `BackendSymbolicFixedFrameInstructionPlan`, re-derives that complete witness, and serializes its entry and exit instruction sequences into one repository-defined byte stream. The same module provides a strict decoder back to the three Phase-59 symbolic instruction forms.

This is deliberately educational repository bytecode. It is not x86-64, AArch64, RISC-V, an ABI, an object-file format, relocatable code, executable machine code, or an instruction-size-optimal encoding.

## Canonical wire format

All multi-byte integers are eight-byte little-endian values. The stream is:

1. four magic bytes `41 4c 46 42` (`ALFB`);
2. one format version byte `01`;
3. `u64` entry-instruction count;
4. exactly that many encoded entry instructions;
5. `u64` exit-instruction count;
6. exactly that many encoded exit instructions; and
7. end of input — trailing bytes are invalid.

Opcode `01` encodes stack-pointer decrement-immediate as `opcode, u64(sp), u64(magnitude)`. Opcode `02` uses the same operands for increment-immediate. Opcode `03` encodes frame-base materialization as `opcode, u64(fp), u64(sp), i64(displacement)`.

The signed displacement occupies the mathematical value modulo `2^64`; the decoder reconstructs values in the upper half of the wire range without relying on implementation-defined unsigned-to-signed narrowing. Phase-59 `size_t` register ids and magnitudes must fit the fixed `u64` wire domain; decoding also rejects any wire value that cannot fit the current host `size_t`.

Fixed widths, explicit endianness, explicit opcodes, one version, exact instruction counts, and mandatory end-of-input make the representation canonical: there is one byte representation for every encodable Phase-59 instruction plan.

## Canonical-witness and strict-decoder obligations

`encode_backend_fixed_frame_bytecode` first recomputes

`lower_fixed_frame_actions_to_symbolic_instructions(source_plan.source_plan)`

and requires exact equality with the supplied Phase-59 witness. Changed instruction forms, register ids, immediates, ordering, or provenance therefore fail before bytes are emitted.

`decode_backend_fixed_frame_bytecode` is self-contained and rejects bad magic, unsupported versions, unknown opcodes, truncated fixed-width fields, impossible count-versus-remaining-byte claims, host-unrepresentable `size_t` operands, and trailing bytes. Count bounds are checked before vector reservation so hostile count prefixes cannot request storage unsupported by the actual byte stream.

## Semantic replay evidence

Verification checks more than encode/decode identity. A test-local byte interpreter parses the canonical byte stream independently of the production decoder and applies each opcode directly to a register state. Its entry and exit effects are compared with an independent interpreter for the sealed Phase-59 symbolic instruction sequence.

Deterministic checks cover a byte-for-byte known encoding, negative signed displacement, malformed streams, canonical-plan tampering, zero immediates, both stack-growth directions, and the 64-bit `2^63` unsigned magnitude boundary where supported. A fixed-seed corpus of canonical Phase-57/58/59 frame plans then checks encode determinism, strict decode equality, and independent byte-versus-symbolic register-state replay.

The corpus is executable evidence, not a proof substitute. Correctness relies on the Phase-59 canonical-witness revalidation, the one-to-one fixed opcode schema, explicit integer serialization, and strict complete-stream parsing.

## Complexity

For `I` symbolic instructions, encoding performs one Phase-59 canonical re-derivation plus `O(I)` byte serialization and output storage. Strict decoding is `O(B)` time for `B` input bytes and `O(I)` decoded-instruction storage. The wire size is exactly the fixed header/count overhead plus 17 bytes per stack adjustment and 25 bytes per frame-base materialization.

## Frontier

Phase 60 is **SEALED** after exact merged-main GCC/Clang/ASan+UBSan verification and the Phase-60 architecture audit. The next promoted boundary is Phase 61 canonical fixed-frame bytecode execution: production must revalidate the complete encoded plan and execute it over an explicit finite register file with checked arithmetic and replayable entry/exit snapshots. This remains target-neutral educational execution semantics; no target-specific opcode, ABI, object-code, or native-execution claim is implied.
