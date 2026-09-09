# Phase 50 sealing audit

## Integrated checkpoint

Phase 50 is sealed at merged `main@d5c4153f7ca5e89ba3aa73a138aace8156534111`.

The implementation candidate `333d54fd25d551c70dd0dddc43db82d404ebe3b5` passed exact pull-request CI run `34402086929` on GCC release, Clang release, and GCC ASan+UBSan. After normal merge through PR #117, merged-main CI run `34402793128` repeated the same three-way matrix successfully; every Configure, Build, and Test step completed successfully.

## Correctness boundary

The sealed capability is deterministic machine-independent backend storage and spill materialization over the sealed phi-free / coalesced-allocation substrate:

- every Phase-49 coalescing class has exactly one final backend storage binding;
- assigned classes preserve their physical-register index;
- spilled classes receive unique dense stack slots in class-id order;
- every original virtual location exposes replayable class/storage provenance;
- scheduled copies lower solely from final endpoint storage into register moves, reloads, stores, or reload-plus-store, and are omitted only when both endpoints already have identical final storage;
- every source instruction remains exactly one backend instruction operation;
- spilled uses/definitions are materialized through operation-local abstract scratch registers, with repeated accesses to one stack slot reusing one scratch;
- no stack slot is passed directly as an instruction register operand;
- block order, reachability, original-block provenance, origin kind, and origin index remain replayable;
- `stack_slot_count` and `max_scratch_registers` are explicit materialization witnesses rather than hidden allocator assumptions.

Deterministic tests cover all scheduled-copy storage shapes, same-storage elimination, spilled use/definition behavior, scratch reuse, and malformed allocation rejection. A fixed-seed 400-program corpus calls the real sealed Phase-49 coalescer and verifies storage provenance, stack-slot density, operation typing/counts, scratch-demand accounting, source-operation replay, and exact repeated-run determinism.

The sealed implementation deliberately does not claim concrete ISA opcode legality, addressing modes, byte-level frame layout/alignment, calling conventions, actual hardware scratch availability, spill-cost optimality, post-rewrite reallocation, MemorySSA, or backend-wide optimality.

## Architecture audit

Phase 50 closes the backend-IR gap identified by the Phase-49 audit: spills and copies are no longer implicit allocator metadata, because the exact reload/store/register-move structure is executable and replayable.

A distinct cross-layer gap remains. Phase 49 colors using an allocator-visible `register_budget`, while Phase 50 introduces an abstract spill-scratch namespace after that allocation. If the allocator consumes the entire physical register file, Phase 50 has not yet proved that its reported scratch demand can be mapped to real physical registers without overlapping assigned values.

The next slice should close that gap before introducing ISA-specific instructions or frame byte offsets. A scratch-aware planner can reserve a suffix of a finite physical register file, rerun Phase-49 coalescing under the reduced allocatable prefix, invoke the sealed Phase-50 materializer, and accept the candidate exactly when measured abstract scratch demand fits the reserved suffix. Mapping scratch index `i` to physical register `allocatable_registers + i` then makes assigned and scratch registers disjoint by construction.

Searching reservation sizes in deterministic ascending order makes the selected reservation minimal for this fixed sealed pipeline. It does **not** imply minimum spill count, optimal coalescing/coloring, globally optimal register allocation, or an optimal backend.

## Promotion

Phase 51 should implement scratch-aware backend register reservation over a caller-supplied total physical-register count:

- enumerate candidate reserved scratch counts from zero through the total register count;
- expose `allocatable_registers = total_registers - reserved_scratch_registers` for every attempt;
- rerun sealed Phase-49 coalescing and sealed Phase-50 materialization under that allocatable budget;
- accept the first candidate whose `max_scratch_registers` does not exceed the reserved suffix;
- map every used abstract scratch register to a concrete non-overlapping physical register id in that suffix;
- expose enough attempt/selection provenance to replay why the chosen reservation is feasible and why every smaller reservation was rejected;
- verify that every assigned location remains below the allocatable boundary and every physicalized scratch operand lies inside the reserved suffix.

Byte-level stack-frame layout/alignment, concrete ISA instruction selection/addressing, calling conventions, minimum-spill/global-register optimality, MemorySSA, spill-cost heuristics, and post-rewrite reallocation remain outside Phase 51.
