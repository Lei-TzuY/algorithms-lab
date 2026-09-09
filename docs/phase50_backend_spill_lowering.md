# Phase 50 — Explicit Backend Storage and Spill Materialization

Phase 50 consumes the sealed Phase-47 phi-free program plus the sealed Phase-49
coalesced allocation. It introduces a machine-independent backend storage IR;
it does not choose concrete machine instructions or frame byte offsets.

## Contract

`lower_phi_free_backend_storage(program, allocation)` assigns final backend
storage to every Phase-49 coalescing class:

- an assigned class keeps its Phase-49 physical-register index;
- each spilled class receives one stack slot, numbered deterministically in
  final class-id order;
- every original virtual location exposes its class id and final storage as
  replayable provenance.

The lowering validates that class membership partitions the allocation location
universe, class register colors stay inside the recorded register budget,
per-location assignments agree with class colors, and the spill list agrees
with stack-backed classes.

## Backend operation IR

The backend IR has three explicit storage namespaces:

- `physical_register` — Phase-49 assigned register indices;
- `stack_slot` — one slot per spilled coalescing class;
- `spill_scratch_register` — operation-local abstract temporaries used only to
  materialize spilled operands.

Every emitted operation retains its original phi-free operation kind and index.
The four operation kinds are:

- `register_move`: register -> register;
- `stack_reload`: stack slot -> register;
- `instruction`: register inputs -> optional register output;
- `stack_store`: register -> stack slot.

This is deliberately an abstract backend representation. It does not claim ISA
opcode legality or physical scratch-register availability.

## Scheduled-copy lowering

For every entry/exit scheduled copy, final source/destination storage controls
materialization:

- identical storage: emit nothing;
- register -> register: one register move;
- register -> stack: one store;
- stack -> register: one reload;
- stack -> stack: one reload into scratch 0 followed by one store.

Therefore Phase-49 class coalescing is not the only source of removable copies:
two distinct noninterfering classes that happen to receive the same physical
register also produce identical final storage and the copy is safely omitted.
No other scheduled copy is silently removed.

## Instruction spill lowering

An abstract phi-free instruction still executes exactly once. Assigned uses and
definitions remain physical-register operands. A spilled use is reloaded before
the instruction into an operation-local scratch register. Repeated uses of the
same spilled stack slot reuse the same scratch and therefore need only one
reload. A spilled definition is produced in a scratch register and stored back
immediately afterward; when its stack slot was already materialized for a use,
the same scratch is reused.

Scratch indices restart at zero for every phi-free operation. The result reports
`max_scratch_registers`, the maximum scratch namespace cardinality required by
any one materialized operation. This is an explicit abstract demand witness,
not a claim that real hardware reserves that many registers or that the scratch
assignment is globally optimal.

## Structural invariants

The central replay obligations are:

1. class storage is total and deterministic;
2. every original location maps to exactly its Phase-49 class storage;
3. stack slots are unique per spilled class and dense from zero;
4. backend blocks preserve phi-free block order, reachability, and
   `original_block` provenance;
5. origin tags preserve entry-move -> instruction -> exit-move program order;
6. every non-omitted scheduled copy has exactly the storage transfer shape
   implied by its endpoints;
7. every original instruction has exactly one backend `instruction` operation,
   with the same use multiplicity and definition presence;
8. no stack slot is passed directly to an abstract instruction operand;
9. `max_scratch_registers` equals the largest referenced scratch index plus one.

These invariants are sufficient to replay the storage/materialization structure
without relying on hidden allocator state.

## Verification

Deterministic regressions cover all four nontrivial scheduled-copy storage
shapes, identical-final-storage copy elimination, repeated spilled-use scratch
reuse, spilled definitions, and malformed allocation rejection.

A fixed-seed corpus generates 400 one-block phi-free programs, obtains the
actual sealed Phase-49 coalesced allocation, materializes it, and verifies:

- one stack slot per spilled class;
- per-location class/storage provenance;
- operation-kind storage typing;
- scheduled-copy materialization counts from final storage alone;
- exactly one backend instruction per source instruction;
- preservation of instruction use multiplicity and definition presence;
- scratch-demand accounting;
- deterministic repeated lowering.

Full repository CI remains the integration gate because this test links directly
to the sealed Phase-49 implementation rather than maintaining a second
production allocator inside Phase 50.

## Complexity and non-claims

Let `L` be the Phase-49 location count, `C` the coalescing-class count, and `O`
the number of phi-free operations/operands. This first-principles baseline uses
linear scans for class/location validation and provenance lookup, so the
conservative implementation cost is `O(L*C + O*L)` time with `O(L + C + O)`
result/working storage. The intentionally simple lookup strategy keeps the
correctness surface explicit; no backend-wide performance claim is made.

Concrete ISA opcodes/addressing modes, byte-level frame layout or alignment,
calling conventions, hardware scratch-register reservation, spill-cost
optimization, post-rewrite reallocation, MemorySSA, and backend-wide optimality
remain outside this Phase-50 implementation slice.

## Phase status

Implementation is complete only when this candidate passes exact full-repository
GCC release, Clang release, and GCC ASan+UBSan CI and the merged `main` repeats
that matrix successfully. Until then Phase 50 remains an active frontier rather
than a sealed claim.
