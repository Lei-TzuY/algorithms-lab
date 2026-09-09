# Phase 45 sealing audit

Phase 45 is sealed after the minimal scalar SSA construction reached the exact integrated `main` tree and the merged-main CI matrix passed.

## Integration evidence

- implementation PR: #107
- implementation candidate: `bb8d28fcf9af449cff5479104fa8becf04722dbb`
- squash-merged main: `800f63b54fcfb0612fdf67eba27c5c3d41849576`
- exact candidate CI: run `34377126366`, GCC release / Clang release / GCC ASan+UBSan all successful
- merged-main CI: run `34377844865`, GCC release / Clang release / GCC ASan+UBSan all successful

## Capability boundary

The sealed slice turns the repository's Phase-43/44 control-flow analysis substrate into executable scalar SSA construction:

- Phase-44 iterated dominance frontiers place deterministic phi nodes from explicit definition sites plus each variable's version-0 entry definition;
- Phase-43 dominator-tree traversal renames phi results, statement uses, and definitions with checked monotonically allocated versions;
- phi incoming witnesses are emitted for every unique reachable predecessor block, preserving loop/backedge values while deduplicating parallel CFG edges;
- unreachable blocks remain explicitly outside the renamed domain and malformed entry/block/variable inputs are rejected;
- the renaming traversal is iterative and deterministic rather than relying on recursive C++ stack depth or incidental adjacency order.

## Correctness evidence

Deterministic tests cover diamonds, loop-carried values, parallel predecessor edges, statement use-before-definition order, validation, and repeated-build determinism.

The primary randomized differential corpus uses 350 fixed-seed reachable DAGs with independent symbolic reaching-definition propagation in topological order. The oracle does not call the production dominator tree, dominance-frontier index, or IDF recurrence. Production versions are translated back to symbolic `initial`, `phi(block, variable)`, and `definition(block, instruction, variable)` identities before exact comparison of phi placement, renamed uses/definitions, and phi incoming witnesses.

The dominance-frontier placement theorem and dominator-stack renaming argument remain proof obligations; the randomized corpus is implementation evidence rather than a substitute proof.

## Architecture audit

No unresolved correctness or integration blocker remains in the Phase-45 scope. Adding more minimal-SSA graph-shape variants would now be lower-value coverage farming.

The next coherent gap is liveness-aware phi pruning. Full IDF placement is intentionally not sufficient for that next phase: a pruned placement worklist may propagate a block as a new definition site only when a phi is actually materialized there, and materialization is conditioned on the variable being live-in at that block. This keeps the next phase's proof obligation distinct from simply computing minimal SSA and deleting dead-looking phis afterward.

## Promotion decision

Phase 46 targets liveness-pruned scalar SSA over the same event IR:

1. compute deterministic reachable-block liveness from upward-exposed uses and local definitions;
2. verify liveness independently, preferably via a def-free-path-to-use characterization rather than copying the production fixed-point recurrence;
3. perform IDF worklist phi placement conditioned on live-in status, propagating only actually materialized phis as definition sites;
4. reuse the sealed Phase-45 renaming semantics and witness format;
5. verify that every retained phi is live-in, the pruned phi set is contained in the minimal-SSA placement, dead-join phis disappear, live joins/loops remain correct, and observable renamed uses agree with an independent reaching-definition oracle.

Semi-pruned SSA, MemorySSA, SSA destruction, optimization passes, post-dominators/control dependence, and incremental CFG maintenance remain outside this promoted slice unless a future architecture audit supplies a separate reason to pursue them.
