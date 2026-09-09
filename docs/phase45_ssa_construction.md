# Phase 45 — minimal SSA phi materialization and version renaming

## Scope and contract

`construct_ssa` turns a block-ordered variable-event IR into deterministic SSA on
the subgraph reachable from one specified entry block. It deliberately reuses
two sealed control-flow capabilities rather than reimplementing them:

- Phase 43 `DominatorTree` supplies the dominator tree used by renaming;
- Phase 44 `DominanceFrontierIndex` supplies iterated-dominance-frontier phi
  placement.

Each variable has an explicit entry value `(variable, version=0)`. The entry must
have no predecessor edge, including a self-loop. Input block count must equal the
CFG vertex count, every variable id must be in range, and unreachable blocks must
contain no input instructions; unreachable CFG vertices remain explicit
`reachable=false` output blocks rather than being silently renamed.

An input instruction lists uses followed by at most one definition. Uses observe
the current version before that instruction's definition, so an event such as
`x = f(x)` reads the pre-definition version and then creates a new one.

Parallel CFG edges are legal but phi predecessor witnesses are block-based and
therefore deduplicated. Edge weights are irrelevant. Non-entry self-loops and
backedges are supported.

## Phi placement

For each variable, version 0 is treated as an implicit definition at the entry.
Every reachable block containing an explicit definition is added to that
variable's definition set. Production asks the sealed Phase-44 IDF index for the
least placement closure and materializes one phi for that variable at every
returned block.

Phi lists are sorted by variable id. A phi contains:

- its variable id;
- one newly allocated SSA result version;
- one incoming `(predecessor block, SSA value)` witness for every unique
  reachable predecessor block, sorted by predecessor id.

The predecessor-free entry cannot legitimately acquire a phi; production treats
that condition as an internal invariant violation rather than inventing entry
backedge semantics.

## Renaming invariant

Renaming walks the sealed dominator tree with an explicit iterative frame stack,
not recursive C++ calls. For every variable a version stack begins with version
0.

On entry to a dominator-tree block:

1. phi results allocate and push fresh versions in ascending variable order;
2. instructions are processed in source order: uses read the current top and a
   definition allocates/pushes a fresh version;
3. each reachable successor phi receives the current version for its variable
   from this predecessor block;
4. dominator children are visited in ascending vertex-id order;
5. versions created in the block are popped when the frame exits.

Thus the stack top is the latest definition dominating the currently renamed
program point. Backedge incoming values are filled after the backedge predecessor
has processed its own instructions, while the loop-header phi result remains
active throughout its dominated region.

Version counters are checked for `size_t` overflow. Returned version numbers are
stable for identical graph/IR input because phi order, block-child order,
instruction order, and predecessor output order are all deterministic.

## Complexity boundary

This phase does not claim a standalone optimized SSA-construction bound stronger
than its dependencies. Construction includes the sealed Phase-43 dominator cost
and sealed Phase-44 frontier/IDF cost. Beyond those indexes, phi materialization
and renaming are linear in reachable CFG adjacency, input variable events, phi
nodes, and emitted phi-incoming payload, plus sorting of deterministic block
lists. Resident output is proportional to the renamed IR plus phi/incoming
witnesses.

No liveness analysis is performed, so this is not liveness-pruned or semi-pruned
SSA. The repository does not claim MemorySSA, optimization passes, incremental
CFG maintenance, or a production compiler IR.

## Verification

Deterministic cases cover:

- a diamond where one branch redefines a variable and the join phi merges that
  definition with entry version 0;
- a loop with a backedge definition, confirming the loop-header phi, loop-carried
  incoming value, and use-before-definition ordering;
- parallel predecessor edges, confirming one phi incoming per predecessor block;
- repeated-build determinism;
- directed-input, entry-predecessor, block-count, variable-id, and unreachable-IR
  validation.

The randomized differential corpus uses 350 fixed-seed reachable DAGs with 1–9
blocks, 1–4 variables, parallel edges, ignored signed weights, and randomized
block-local use/definition events. Because edges are generated only from lower to
higher block ids, the independent oracle evaluates reaching symbolic definitions
in topological order without `DominatorTree`, dominance frontiers, or IDF:

- equal predecessor definitions propagate unchanged;
- multiple distinct predecessor definitions create an oracle phi symbol;
- statements consume the current symbolic definition then replace it on a local
  definition.

Production SSA values are mapped back to independent identities
`initial(variable)`, `phi(block, variable)`, and
`definition(block, instruction, variable)`. The suite compares the exact phi set,
every renamed use, every definition identity, and every phi incoming witness.

Focused strict GCC and Clang builds plus an actual GCC ASan+UBSan build pass the
same candidate before remote full-repository CI. These tests are implementation
evidence; the dominance-frontier phi-placement theorem and dominator-stack
renaming argument remain proof obligations.

## Sealed boundary

Phase 45 is sealed at merged `main@800f63b54fcfb0612fdf67eba27c5c3d41849576`.
The exact implementation candidate passed pull-request CI run `34377126366`, and
the merged-main tree passed push CI run `34377844865`; both matrices completed
successfully under GCC release, Clang release, and GCC ASan+UBSan.

The sealed capability is deterministic minimal scalar SSA for the explicit event
IR above. Liveness-pruned/semi-pruned SSA, MemorySSA, SSA destruction,
optimization passes, post-dominators/control dependence, and incremental CFG
maintenance remain separate architectural hypotheses. Phase 46 promotes the
first of those gaps: liveness-pruned SSA phi placement with independently verified
block liveness while preserving the sealed renaming semantics.
