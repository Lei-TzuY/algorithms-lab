# Phase 51 — scratch-aware backend register reservation

## Capability

Phase 51 closes the gap between the sealed Phase-49 allocator-visible register budget and the sealed Phase-50 operation-local abstract spill-scratch namespace.

Given a caller-supplied physical-register count `R`, the planner tests reserved scratch suffix sizes `s = 0..R` in deterministic ascending order. For each candidate it:

1. exposes `allocatable_registers = R - s` to the sealed Phase-49 coalescer;
2. reruns the sealed Phase-50 storage/spill materializer;
3. records measured `max_scratch_registers`, stack-slot count, spill-location count, and feasibility `max_scratch_registers <= s`;
4. accepts the first feasible candidate.

The selected result retains the exact Phase-49 allocation and abstract Phase-50 lowering for replay. It also exposes the physical scratch map `scratch[i] -> allocatable_registers + i` and a physicalized copy of every backend block.

Persistent assigned registers are required to remain below `allocatable_registers`; mapped scratch registers occupy only the reserved suffix. If no candidate through `s = R` is feasible, the plan has no selection and retains every failed attempt rather than overlapping scratch temporaries with assigned values.

## Proof obligations and invariants

For every attempt, `allocatable_registers + reserved_scratch_registers == R`. Its feasibility bit is exactly the measured sealed Phase-50 scratch demand compared with the candidate suffix size.

Because attempts are evaluated in ascending reservation size, the first feasible attempt is the minimum reservation for this fixed deterministic Phase-49/50 pipeline. This does **not** imply minimum spill count, optimal coalescing/coloring, or globally optimal register allocation; changing the reservation can itself change the allocator result.

For a selected attempt:

- every persistent physical-register binding is `< allocatable_registers`;
- every abstract scratch index is `< reserved_scratch_registers`;
- scratch index `i` maps to physical register `allocatable_registers + i < R`;
- assigned and scratch physical registers are therefore disjoint;
- physicalization preserves block order, operation kind, origin provenance, stack slots, and non-scratch physical-register ids while eliminating the abstract scratch storage kind from operation operands.

## Verification

Deterministic tests cover a program whose minimum feasible reservation is exactly one register, a program feasible with zero reserved scratch, and an infeasible physical-register file.

A fixed-seed 350-program corpus generates copy/instruction mixes and total register counts from zero through five. Every reported attempt is replayed through the real sealed Phase-49 coalescer and Phase-50 materializer; measured scratch demand, stack slots, spill count, and feasibility must match exactly. Repeated planning must be deterministic. Selected plans additionally verify assigned/scratch disjointness and operation-level physicalization provenance.

## Complexity and claim boundary

At most `R + 1` complete sealed Phase-49/50 pipeline attempts are evaluated, stopping at the first feasible reservation. Phase 51 therefore multiplies the underlying coalescing/materialization work by at most `R + 1`; it does not claim a more efficient fixed-point solver.

Byte-level stack-frame layout/alignment, concrete ISA opcode legality and addressing modes, calling conventions, spill-cost heuristics, minimum-spill/global allocation optimality, MemorySSA, and post-rewrite reallocation remain outside this slice.
