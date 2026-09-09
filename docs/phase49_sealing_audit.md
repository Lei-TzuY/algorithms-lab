# Phase 49 sealing audit

## Integrated checkpoint

Phase 49 is sealed at merged `main@91b664d4c40c22f259211f46cf4ff6ad9c6a82d7`.

The implementation landed as one coherent mainline commit after the sealed Phase-48 checkpoint. Exact merged-main CI run `34396229380` completed successfully on GCC release, Clang release, and GCC ASan+UBSan; every Configure, Build, and Test step completed successfully.

## Correctness boundary

The sealed capability is intentionally limited to deterministic interference-safe copy coalescing over the sealed phi-free register-allocation substrate:

- Phase-48 remains the source of the canonical virtual-location universe and original interference graph;
- scheduled entry/exit moves are processed as deterministic coalescing preferences;
- two current equivalence classes merge only when no original interference edge crosses between them;
- the final quotient interference graph is reconstructed explicitly and recolored under the caller-supplied register budget;
- quotient assignments and spills are mapped back to every original virtual location;
- scheduled copies whose endpoints end in one class are emitted as replayable redundant-copy witnesses.

The central invariant is that every original interference edge continues to cross two distinct final classes. Recoloring then preserves register safety on every assigned quotient interference edge. A spilled class is spilled as one class; no minimum-register, minimum-spill, or maximum-coalescing claim is implied.

Deterministic regressions cover safe coalescing, direct interference, transitive class growth that later encounters interference, and zero-register whole-class spilling. A fixed-seed 500-program corpus verifies the class partition, original-interference separation, quotient-edge support, assignment safety, spill consistency, blocked-preference witnesses, redundant-copy witnesses, and exact repeated-run determinism.

## Architecture audit

Phase 48 deliberately deferred executable spill rewriting because the phi-free IR had no explicit backend memory-operation model. Phase 49 closes the independent copy-coalescing hypothesis but does not remove that backend gap.

The next step should therefore not hide stack traffic inside the allocator or invent ISA-specific opcodes. The coherent next capability is a machine-independent backend storage IR that makes the missing semantics explicit: allocated registers, deterministic stack slots for spilled coalescing classes, reserved spill-scratch registers as a distinct abstract namespace, and explicit load/store/register-move operations around lowered phi-free uses, definitions, and scheduled copies.

This boundary allows spill materialization to become executable and replayable while keeping concrete instruction selection, addressing modes, calling conventions, frame layout in bytes, and mapping abstract spill scratch registers onto real machine registers outside the slice.

## Promotion

Phase 50 should introduce deterministic backend storage lowering over the sealed Phase-49 result. The implementation should:

- assign one deterministic stack slot to each spilled coalescing class and map all class members to that slot;
- preserve assigned classes as physical-register bindings;
- lower scheduled copies into register moves, stack loads/stores, or load-then-store sequences, while omitting copies proven redundant by coalescing;
- lower each phi-free instruction to explicit register operands, inserting stack reloads before spilled uses and stack stores after spilled definitions through abstract spill-scratch registers;
- preserve the original CFG/block/operation order and expose enough provenance to replay every rewrite;
- expose the required maximum spill-scratch-register count rather than claiming a concrete machine register reservation policy.

Verification must prove that every virtual location has one backend storage binding, coalesced spilled class members share one slot, no non-spilled location gains stack traffic, each original use/definition/move is structurally replayable from the lowered sequence, and eliminated copies are exactly same-storage copies. Machine instruction selection, concrete frame offsets/alignment, calling conventions, physical scratch-register reservation, spill-cost optimization, reallocation after rewrite, and backend-wide optimality remain outside Phase 50.
