# Phase 43 sealing audit — directed dominator trees

## Live checkpoint

Phase 43 implementation PR #103 merged into `main` as
`026066ec4bd4e51c49b593000c67346335dc989b`.

The exact merged-main CI run `34370073918` completed successfully on all three
repository gates:

- GCC release;
- Clang release;
- GCC ASan+UBSan.

There are no open pull requests or open issues at the sealing checkpoint.

## Capability audit

The merged slice is an executable control-flow dominance capability rather than
an API placeholder:

- directed-input validation and explicit start-reachable semantics;
- deterministic insertion-order DFS numbering;
- first-principles semi-dominator/link-eval construction;
- exact immediate-dominator witness for every reachable non-start vertex;
- reconstructable dominator-tree children;
- Euler-interval `dominates(u,v)` queries;
- parallel-edge and self-loop tolerance without changing dominance semantics;
- unreachable vertices kept outside the dominance-query domain.

The source and test contract deliberately keep edge weights irrelevant, matching
standard control-flow dominance rather than inventing a weighted variant.

## Independent verification boundary

The 600 fixed-seed randomized directed multigraph cases use an independent
iterative dominator-set fixed-point oracle. The oracle does not share the
semi-dominator, bucket, or link/eval recurrence with production. Verification
checks reachability, every immediate dominator, all-pairs dominance, tree
parent/child consistency, DFS-numbering consistency, and repeated-build
determinism.

This evidence supports the implementation but does not replace the mathematical
Lengauer–Tarjan semi-dominator theorem, bucket-resolution argument, or link/eval
invariants. The repository therefore retains the intentionally conservative
`O(VE)` construction claim instead of importing a stronger optimized bound.

## Architecture decision

Phase 43 is complete after one coherent implementation. Adding another
dominator algorithm, incremental micro-optimizations, extra graph-shape cases, or
post-dominators merely to keep the phase open would either duplicate the sealed
capability or mix a distinct control-flow problem into the same checkpoint.

The next substantial integration boundary is dominance frontier computation.
It consumes the sealed immediate-dominator/dominance index and adds information
needed by SSA phi placement that the dominator tree alone does not provide.
Phase 44 therefore targets:

1. exact per-block dominance frontiers on the start-reachable CFG;
2. deterministic iterated-dominance-frontier closure for a set of definition
   blocks;
3. explicit handling of unreachable/invalid definition blocks and multigraph
   edges;
4. an independent definition-based oracle rather than another copy of the
   production frontier recurrence.

Full SSA renaming, post-dominators, control dependence, and incremental CFG
updates remain outside the promoted slice.
