# Phase 48 sealing audit

## Integrated checkpoint

Phase 48 is sealed at merged `main@b4a99540c1bc199d389c364cca4fe4d4d8c4dc49`.

The implementation candidate was PR #113 at exact head
`7d654afd16c74217822aafc2070b50b5572d5968`. Candidate CI run
`34390159865` completed successfully on GCC release, Clang release, and GCC
ASan+UBSan. After squash integration, merged-main CI run `34390716189` also
completed successfully on all three jobs.

## Correctness boundary

The sealed capability is intentionally limited to:

- exact backward may-liveness over the sealed phi-free operation order;
- materialized block and per-operation live sets;
- a reconstructable interference graph over SSA values and copy temporaries;
- deterministic bounded greedy coloring;
- an explicit spill witness for unassigned virtual locations.

Randomized verification uses an independent forward path-to-use BFS oracle rather
than reproducing the production backward recurrence. The interference edge set is
rebuilt independently from those semantic oracle live sets, and every assigned
interfering pair must receive distinct physical registers.

No optimal-coloring, chromatic-number, minimum-spill, spill-cost, copy-coalescing,
spill-rewrite, MemorySSA, or backend-wide optimality claim is sealed here.

## Architecture audit

The next step should not be executable spill insertion yet. The current phi-free
IR records uses, definitions, and scheduled copies, but has no machine opcode,
memory-addressing, load/store, frame-slot, or calling-convention semantics. Adding
spill rewrite now would silently invent a backend IR inside the allocator and mix
two architectural hypotheses.

The next coherent capability is interference-safe copy coalescing. Phase 47
already exposes copy schedules and Phase 48 exposes exact interference, so the
repository has both inputs needed to test the central coalescing safety condition:
locations may be merged only when the chosen coalescing step preserves the
interference constraints of the quotient graph.

## Promotion

Phase 49 should introduce deterministic copy-preference/coalescing analysis over
the sealed phi-free program and Phase-48 interference result. The implementation
should expose coalesced equivalence classes, quotient interference, bounded
coloring, and scheduled moves made redundant by equal assigned locations. Tests
must independently verify that no original interfering locations collapse into the
same class and that every retained assigned interference edge remains register
safe.

Copy coalescing remains a heuristic optimization: no maximum-coalescing,
minimum-register, minimum-spill, or globally optimal allocation claim is implied.
Executable spill-code insertion remains a later frontier after an explicit backend
memory-operation representation exists.
