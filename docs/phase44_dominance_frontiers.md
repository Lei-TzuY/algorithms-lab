# Phase 44 — dominance frontiers and IDF phi-placement substrate

## Scope and contract

`DominanceFrontierIndex` consumes the sealed Phase-43 `DominatorTree` semantics
for one specified start vertex of a directed `Graph`.

For start-reachable vertices `x` and `y`, production uses the defining predicate

`y in DF(x)` iff there exists a start-reachable predecessor `p` of `y` such that
`x` dominates `p`, while `x` does not strictly dominate `y`.

Consequences are explicit rather than accidental:

- directed/start validation is inherited from the sealed dominator index;
- edge weights are ignored;
- parallel edges may repeat predecessor evidence but never duplicate a frontier
  vertex;
- self-loops and backedges may put a block in its own frontier;
- unreachable vertices have empty frontiers and do not participate as
  predecessors;
- invalid vertex queries throw, and IDF definition blocks must additionally be
  start-reachable.

Each frontier is emitted in ascending vertex-id order. `iterated_frontier`
deduplicates definition blocks, computes the least closure under frontier
expansion, and returns the resulting block set in ascending vertex-id order. An
original definition may appear in the result when a loop places it in its own
iterated frontier.

This is a phi-placement substrate only. It does not construct or rename SSA
values.

## Construction and complexity boundary

The production baseline intentionally evaluates the mathematical frontier
definition directly instead of copying the textbook local/up dominator-tree
recurrence. For each reachable candidate dominator it scans reachable blocks and
their reachable predecessor lists, consulting the sealed constant-time
`dominates` predicate.

With `V` graph vertices and `E` directed adjacency entries, the direct frontier
construction therefore states the conservative `O(V(V+E))` time bound, plus the
sealed Phase-43 dominator construction cost already paid by the embedded index.
Worst-case stored frontier payload is `O(V^2)` in addition to `O(V+E)` temporary
predecessor/reachability state.

IDF closure processes each non-definition block at most once after discovery and
scans its stored frontier, so one query is `O(V + F)` where `F` is the total
frontier payload scanned by that closure, bounded by `O(V^2)`. No optimized
Cytron frontier-construction bound or full SSA-construction bound is claimed.

## Correctness obligations

The per-block result is correct exactly when it satisfies the defining predicate
above. The production implementation makes that predicate visible rather than
hiding it behind an alternate graph primitive.

The IDF result is the least fixed point obtained by seeding the worklist with
all reachable definition blocks and repeatedly adding frontier blocks; newly
discovered non-definition blocks are themselves expanded. Original definitions
are already seeds, so rediscovering one does not require a second expansion.

The dominator theorem itself is inherited from sealed Phase 43. The frontier
definition and least-fixed-point interpretation remain proof obligations; tests
are implementation evidence, not theorem proofs.

## Verification

Deterministic tests cover:

- a diamond merge and two-definition IDF placement;
- a loop/backedge where a block lies in its own frontier;
- a singleton self-loop;
- parallel edges with distinct ignored weights;
- an unreachable predecessor entering a reachable block;
- unreachable definition rejection, invalid indices, empty definitions, and
  undirected/empty-start validation;
- repeated construction determinism.

The randomized differential suite uses 500 fixed-seed directed multigraphs with
1–12 vertices, self-loops, parallel edges, arbitrary signed weights, and random
start vertices. The independent oracle first recomputes reachability and
converges full dominator sets by iterative predecessor intersection. It then
applies the mathematical dominance-frontier definition directly and computes
IDF closure independently. Every vertex frontier and eight randomized definition
sets per graph are compared exactly.

Focused GCC and Clang strict-warning builds and an actual GCC ASan+UBSan build
pass the same repo-native tests before remote full-repository CI. Implementation
PR #105 passed the same three remote gates and was squash-merged as
`0d1115ff30a80947c7f37a1acce09812b9d9e6c1`. The exact merged-main push run
`34373959705` also completed successfully on GCC release, Clang release, and GCC
ASan+UBSan.

## Sealed boundary and next frontier

Phase 44 seals at exact definition-based dominance frontiers plus deterministic
IDF closure. Another frontier algorithm, additional graph-shape variants,
post-dominators, or control dependence would either duplicate this capability or
mix a distinct control-flow problem into the phase.

The next promoted frontier is minimal SSA construction: materialize phi nodes
from the sealed IDF substrate, then rename variable uses and definitions while
walking the sealed dominator tree. The Phase-45 baseline must expose replayable
phi incoming/version witnesses and validate the renamed program independently;
it must not imply liveness-pruned SSA, MemorySSA, optimization passes,
post-dominator/control-dependence analysis, or incremental CFG maintenance.
