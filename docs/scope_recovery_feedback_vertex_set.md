# Scope recovery: bounded exact directed feedback vertex set

## Coverage decision

A fresh live `main`, open-PR/issue, branch, code-search, and PR-history audit after
exact densest-subgraph recovery found no feedback-vertex-set capability or
occupied FVS surface.  The repository already has directed cycle detection,
elementary-cycle enumeration, SCCs, exact cover, treewidth, parity games, and a
large graph-optimization surface, but none solves minimum directed vertex
deletion to acyclicity.

This slice deliberately changes proof model again.  It adds a bounded exact
NP-hard cycle-breaking capability rather than extending the recent
biconnectivity/density streak or reviving the frozen compiler/backend surface.

## Production contract

`minimum_directed_feedback_vertex_set(graph)` accepts directed multigraphs with at
most 20 vertices.  Parallel arcs are preserved, stored weights are ignored, and
a self-loop forces its incident vertex into every feasible answer.  Undirected
input is rejected.  Inputs above the explicit vertex bound are rejected rather
than hiding exponential work behind an unbounded API.

The result contains:

- the lexicographically smallest sorted minimum feedback-vertex set;
- the sorted vertices forced by self-loops;
- a deterministic topological ordering of every remaining vertex, replayable as
  an acyclicity certificate;
- the number of distinct removed-vertex states explored by the exact search.

## Exact branching obligation

For a current removed set `R`, production performs deterministic DFS on the
remaining directed graph.  If no directed cycle exists, `R` is feasible.  If a
cycle `C` is found, every feedback-vertex set extending `R` must delete at least
one vertex of `C`.  Branching once on every distinct vertex of that concrete
cycle therefore preserves at least one branch containing every optimum solution.
Self-loop vertices are inserted into `R` before search because their one-vertex
cycles make them mandatory.

Removed subsets are memoized, so a state is explored at most once.  With the
explicit `V <= 20` contract the direct first-principles implementation has a
conservative worst-case bound of `O(2^V * (E + V log V))` time and
`O(2^V + V)` auxiliary search state, excluding result storage.  This is a
bounded exact exponential algorithm.  It makes no polynomial-time, FPT, or
practical-large-instance claim.

Ties among optimum sets use lexicographic order on sorted vertex ids.  The final
certificate uses deterministic min-id Kahn ordering and replays every surviving
arc, including parallel copies.

## Independent verification

The primary oracle does not use DFS cycle branching.  For fixed-seed random
multigraphs with at most nine vertices it enumerates every vertex-removal subset,
uses an independent Kahn indegree process to test acyclicity, and chooses the
minimum-cardinality then lexicographically smallest feasible subset.

Focused evidence covers:

- empty and singleton DAGs;
- mandatory self-loops;
- DAGs requiring no deletion;
- a directed triangle with deterministic tie-breaking;
- disjoint directed cycles;
- two cycles sharing one optimum deletion vertex;
- parallel arcs and ignored positive/negative weights;
- undirected rejection and the explicit 20-vertex bound;
- 500 fixed-seed random directed multigraphs with self-loops and parallel arcs,
  exact equality against the exhaustive oracle, topological-certificate replay,
  and deterministic repeated execution;
- strict GCC and Clang warning gates plus actual GCC ASan+UBSan execution.

The randomized corpus is implementation evidence, not a complexity or universal
correctness proof; exactness follows from the cycle-hitting branching argument.

## Non-claims

This is not weighted FVS, undirected FVS, parameterized kernelization, iterative
compression, approximation, or a large-instance solver.  It intentionally
establishes a small exact baseline and an executable witness/oracle boundary.
