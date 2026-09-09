# Phase 43 — directed dominator trees

## Scope and contract

`DominatorTree` indexes dominance only in the subgraph reachable from a specified
start vertex of a directed `Graph`.

- directed input is required; undirected input is rejected;
- the start vertex must be valid;
- edge weights are ignored;
- parallel edges and self-loops are accepted and do not change dominance;
- unreachable vertices have no DFS number or immediate dominator and are outside
  the dominance query domain, so `dominates(u, v)` returns false when either
  endpoint is unreachable;
- the reachable start vertex dominates itself and has no immediate dominator.

The index exposes deterministic DFS preorder/numbering, one immediate-dominator
witness for every reachable non-start vertex, the reconstructed dominator-tree
children, and constant-time dominance queries after construction.

## Construction and proof obligations

Construction uses the first-principles Lengauer–Tarjan structure:

1. deterministic insertion-order DFS numbers the reachable subgraph and records
   the DFS parent tree;
2. every reachable directed edge contributes a predecessor relation;
3. a reverse DFS pass computes semi-dominators using explicit `link`/`eval`
   state and path compression;
4. bucket resolution produces provisional immediate dominators;
5. the standard correction pass resolves provisional ancestors to exact
   immediate dominators;
6. the resulting immediate-dominator tree is Euler-numbered so dominance is an
   ancestor-interval query.

The Lengauer–Tarjan semi-dominator theorem, bucket-resolution argument, and
link/eval invariants are proof obligations. Random testing is evidence for this
implementation; it is not a proof of those theorems.

This direct implementation deliberately states the conservative `O(VE)`
worst-case construction bound rather than importing the stronger bound of more
carefully balanced/optimized link-eval variants. The stored graph-derived and
index state is `O(V+E)` on the start-reachable subgraph plus `O(V)` public index
state. After construction, reachability, DFS-number, immediate-dominator, and
`dominates` queries are `O(1)`; returning the child vectors exposes resident
state rather than recomputing the tree.

## Verification

Deterministic tests cover:

- empty/invalid start and undirected rejection;
- singleton/self-loop semantics;
- chain/diamond merges;
- cycles and an irreducible merge shape;
- a start vertex inside a larger graph with unreachable predecessors;
- parallel edges, self-loops, deterministic repeated construction, and
  out-of-range query rejection.

The randomized differential test uses 600 fixed-seed directed multigraphs with
1–18 vertices. An independent iterative dominator-set fixed-point oracle first
computes reachability, then repeatedly intersects predecessor dominator sets.
Immediate dominators are derived from the converged strict-dominator relation,
not from semi-dominators or link/eval. For every randomized graph the test
compares:

- reachable/unreachable classification;
- deterministic DFS numbering consistency;
- every immediate dominator;
- every all-pairs `dominates(u, v)` answer;
- reconstructed tree-parent/child consistency;
- repeated-build determinism.

Focused GCC and Clang strict-warning builds and GCC ASan+UBSan execution pass the
same suite before the remote full-repository CI gate.

## Current boundary

This slice implements exact dominance from one specified start vertex. It does
not add dominance frontiers, post-dominators, incremental CFG updates, or SSA
construction. Those are separate architectural hypotheses and are not implied by
this checkpoint.
