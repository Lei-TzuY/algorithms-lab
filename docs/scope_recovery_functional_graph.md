# Scope recovery: functional-graph decomposition and successor indexing

## Coverage decision

A fresh live-state audit at `main@396a8a3c5c4abe306920a8edd148669aa212347b`
found zero open pull requests and zero open issues. Exact main CI run
`35172815124` completed successfully on GCC release, Clang release, and GCC
ASan+UBSan. Default-branch code search found no reusable functional-graph
successor index, no `FunctionalGraphIndex`, and no generic `kth_successor` /
minimum reach-distance surface. Branch-namespace search likewise found no
functional-graph recovery branch.

The sealed Shapley-Scarf TTC slice does perform a problem-local functional-graph
cycle discovery inside each market round, but it does not expose a reusable
successor-function decomposition, cycle/tree metadata, large-step successor
index, or pairwise reachability distance. This slice therefore fills a genuine
algorithm/data-structure gap rather than duplicating TTC.

The post-Phase-69 authority remains `docs/scope_recovery_after_phase69.md`; the
historical compiler/backend ROADMAP is not an active frontier. The immediately
preceding recovered slice was cograph cotree recognition. Recovery deliberately
moves to a different proof model here instead of extending cograph recognition
with adjacent graph-class variants.

## Production contract

`FunctionalGraphIndex` is built from a dense successor function
`successor[v] in [0,n)`. The empty successor function is valid; any out-of-range
successor rejects.

Every weak functional-graph component contains exactly one directed cycle. The
index exposes:

- the original one-step successor;
- deterministic component ids ordered by the smallest cycle vertex;
- canonical cycle lists, each starting at its smallest vertex and then following
  successor direction;
- for every vertex, its first cycle entry, distance to that cycle, cycle length,
  and the cycle position of its entry;
- `kth_successor(v, k)` for every `uint64_t` step count;
- `steps_to_reach(u, v)`, returning the minimum non-negative step count when
  repeated successor application can reach `v`, and `nullopt` otherwise.

No edge weights or arbitrary-outdegree `Graph` semantics are invented: a
functional graph is represented directly by its successor function while
reusing the repository-wide dense `Vertex` type.

## Invariants and proof obligations

### Cycle kernel by indegree peeling

Compute indegrees and repeatedly remove zero-indegree vertices. In a functional
graph, exactly the directed-cycle vertices survive. A non-cycle vertex must
ultimately feed a cycle and is peeled from the reverse-tree fringe; every cycle
vertex retains one predecessor on the same cycle and therefore survives.

Scanning surviving vertices by ascending id and following successors enumerates
each cycle exactly once. The first unassigned surviving vertex is the smallest
vertex of that cycle, so the produced successor-order cycle is already canonical.

### Reverse-tree propagation

Reverse adjacency is built once from the successor function. A multi-source BFS
starting from all cycle vertices propagates component id, cycle entry,
entry-cycle position, and `distance_to_cycle` outward. Every non-cycle vertex has
exactly one forward parent, so these labels are unique even though a cycle vertex
may have many reverse-tree descendants.

### Binary lifting

For level `b`, `jump[b][v]` equals the vertex reached after exactly `2^b`
successor applications. Level zero is the original successor function and every
higher level composes the previous level with itself. Sixty-four levels therefore
answer every `uint64_t` step count exactly.

### Minimum reach distance

If the target lies in a reverse tree, it is reachable exactly when the source is
at least as deep and lifting by the depth difference lands on the target. If the
target lies on the component cycle, the minimum distance is the source's tree
depth plus the forward modular offset from its cycle entry to the target. Vertices
in different components are unreachable.

These structural facts are proof obligations. Finite differential testing is
implementation evidence, not a proof of the universal functional-graph
decomposition theorem.

## Independent verification

Focused final candidate bytes passed before upload under:

- GCC 14 C++20 repository strict warnings-as-errors: 4/4;
- Clang 17 C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with leak detection and fail-fast behavior: 4/4.

Deterministic tests cover empty input, singleton self-cycles, malformed
successors, multiple components, fixed points, multi-vertex cycles, reverse-tree
chains, same-tree and cross-component reachability, and `UINT64_MAX` successor
steps.

The primary oracle does not use indegree peeling, reverse BFS, or binary lifting.
For every successor function on `n=1..5` vertices (**3,413 functions total**), it
walks successors directly until the first repeated vertex and derives cycle
entry, depth, canonical cycle order, large-step successor behavior, and first
reach distance from that trace. Production must agree on every vertex and every
ordered vertex pair.

A further 800 fixed-seed random successor functions with 1..40 vertices verify
cycle metadata plus random full-width `uint64_t` successor steps and pairwise
reachability against the same direct-walk oracle.

## Complexity and non-claims

Construction uses `O(V)` indegree/reverse-tree work plus 64 doubling levels, so
its direct bound is `O(V log U)` time and storage for `U = 2^64` (64 is constant
for this API). Metadata queries are `O(1)`; `kth_successor` and
`steps_to_reach` use at most 64 jump levels.

This slice does **not** claim dynamic successor updates, weighted orbit costs,
aggregates along jumps, arbitrary precision step counts beyond `uint64_t`,
functional-graph isomorphism, random mapping statistics, or a replacement for
general directed-graph SCC/decomposition algorithms.

## Scope

Exactly three new paths are added:

- `include/algorithms/graphs/functional_graph.hpp`;
- `tests/test_functional_graph_cases.hpp`;
- `docs/scope_recovery_functional_graph.md`.

The merged verification architecture auto-enrolls `tests/test_*_cases.hpp` in
isolated translation units, so no CMake or `tests/test_main.cpp` edit is needed.
No README, historical ROADMAP, recovery-authority, workflow, benchmark, frozen
compiler/backend, occupied recovery branch, or temporary-file churn is included.
