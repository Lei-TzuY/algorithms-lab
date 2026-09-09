# Phase 44 sealing audit — dominance frontiers and IDF

## Live checkpoint

Phase 44 implementation PR #105 was squash-merged into `main` as
`0d1115ff30a80947c7f37a1acce09812b9d9e6c1`.

The exact merged-main CI run `34373959705` completed successfully on all three
repository gates:

- GCC release;
- Clang release;
- GCC ASan+UBSan.

There were no open pull requests at the sealing checkpoint.

## Capability audit

The merged slice is an executable phi-placement substrate rather than a helper
API placeholder:

- exact per-block dominance frontiers over the start-reachable directed CFG;
- the mathematical frontier predicate evaluated directly against the sealed
  Phase-43 dominance index;
- deterministic ascending frontier output;
- parallel-edge deduplication without changing predecessor evidence;
- self-loop/backedge semantics that may place a block in its own frontier;
- unreachable vertices kept outside the frontier/IDF definition domain;
- deterministic least-fixed-point IDF closure for a set of definition blocks;
- explicit validation of invalid or unreachable definition vertices.

The direct definition-based construction intentionally retains its conservative
`O(V(V+E))` bound and `O(V^2)` worst-case frontier payload. The repository does
not import an optimized Cytron construction bound that this implementation does
not provide.

## Independent verification boundary

The 500 fixed-seed randomized directed multigraph cases use an independent
oracle that recomputes reachability and full dominator sets by iterative
predecessor intersection. The oracle then applies the mathematical dominance-
frontier definition and computes IDF closure independently. Every per-vertex
frontier and eight randomized definition sets per graph are compared exactly.

This evidence supports implementation correctness but does not replace the
underlying dominator theorem, the dominance-frontier definition, or the least-
fixed-point argument for IDF placement.

## Architecture decision

Phase 44 is complete after one coherent implementation. A second frontier
algorithm, further micro-variants, post-dominators, or control dependence would
not deepen the same capability boundary enough to justify keeping the phase
open.

The next substantial integration boundary is minimal SSA construction. Phase 45
therefore targets one coherent vertical slice:

1. accept a deterministic block-ordered variable-event IR on a directed CFG;
2. use the sealed Phase-44 IDF substrate to materialize phi nodes;
3. use the sealed Phase-43 dominator tree to rename uses and definitions from
   explicit version-0 entry values;
4. emit replayable phi incoming/version witnesses and deterministic renamed
   instructions;
5. verify diamonds, loops, parallel predecessor edges, statement ordering, and
   bounded randomized reachable DAGs against an independent reaching-definition
   oracle rather than the production IDF recurrence.

Liveness-pruned/semi-pruned SSA, MemorySSA, optimization passes,
post-dominator/control-dependence analysis, and incremental CFG maintenance stay
outside the promoted slice.
