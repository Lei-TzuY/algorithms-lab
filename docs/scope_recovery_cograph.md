# Scope recovery: cograph recognition and cotree decomposition

## Coverage decision

A fresh live-state audit at `main@798cfeb9017440f2633395f7a8c92b09e357d55c`
found zero open pull requests and zero open issues, with exact main CI run
`35169834381` completed successfully. Default-branch code search, pull-request
history, and branch-namespace search found no cograph / cotree / P4-free graph
capability. The post-Phase-69 recovery authority remains
`docs/scope_recovery_after_phase69.md`; the stale historical backend ROADMAP is
not an active frontier.

Several plausible alternatives were rejected before implementation because they
were already present or occupied. Minimum cycle basis already exists on main;
Birkhoff-von-Neumann decomposition has an occupied recovery branch; full SPQR
remains explicitly deferred as a much larger decomposition surface. This slice
instead adds a compact structural-recognition proof model that is absent from the
repository.

## Production contract

`cograph_cotree(const Graph&)` recognizes the structural simple graph underlying
an undirected repository `Graph` and returns a deterministic reduced cotree when
it is a cograph:

- directed input rejects;
- self-loops are ignored;
- parallel edge copies collapse to one structural adjacency;
- stored edge weights are ignored;
- the empty graph is a cograph with an empty cotree and no root;
- a singleton is represented by one leaf;
- internal nodes are either `disjoint_union` or `complete_join` and have at
  least two children;
- leaves store the original dense vertex ids;
- child order is deterministic by the smallest contained vertex id;
- adjacent internal levels alternate operators, yielding the reduced cotree
  produced by the recursive construction;
- non-cographs return `std::nullopt` rather than a partial witness.

`valid_cograph_cotree` is an expensive replay diagnostic. It validates rooted
shape/no sharing, one unique leaf per graph vertex, reduced-operator alternation,
deterministic child order, and every cross-child adjacency required by union or
join nodes. It also rejects unreachable extra nodes and malformed witnesses.

## Decomposition invariant and proof boundary

For a vertex set `S` with `|S| > 1`, production first finds the connected
components of the induced graph `G[S]`. If there is more than one component, the
cotree root for `S` is a disjoint-union node and recursion continues on those
components.

Otherwise production finds connected components of the complement of `G[S]`. If
the complement is disconnected, the root is a complete-join node and recursion
continues on the complement components. If both the induced graph and its
complement are connected, production rejects that subproblem and the whole graph
is not a cograph.

Correctness relies on the classical cograph characterization: a graph is P4-free
if and only if every induced subgraph on at least two vertices is disconnected or
has disconnected complement. Equivalently, cographs are exactly the graphs
constructed from singleton vertices by repeated disjoint union and complete
join. This theorem is a proof obligation; finite testing is implementation
evidence rather than a proof of the universal characterization.

## Independent verification

Focused final candidate bytes passed before upload under:

- GCC C++20 repository strict warnings-as-errors: 6/6;
- Clang C++20 repository strict warnings-as-errors: 6/6;
- actual GCC ASan+UBSan with leak detection and fail-fast behavior: 6/6.

Deterministic evidence covers directed rejection, empty/singleton semantics,
independent sets, cliques, an induced P4 rejection, a C4 acceptance, arbitrary
signed ignored weights, self-loops, parallel copies, insertion-order invariance,
repeated deterministic decomposition, and deliberate cotree tampering.

The primary oracle is structurally independent of the recursive production
criterion. Tests enumerate **every labeled simple graph with 0 through 6
vertices** (33,868 graphs total). For every 4-vertex subset, the oracle directly
counts its six possible edges and internal degrees; an induced P4 is present
exactly when that induced four-vertex graph has three edges and sorted degree
multiset `{1,1,2,2}`. Production recognition must equal the negation of that
oracle on every graph. Every accepted cotree is additionally replayed to recover
every pairwise adjacency.

A further 800 fixed-seed undirected multigraphs with 0..12 vertices vary
self-loops, parallel multiplicity, and signed weights. The same direct induced-P4
oracle supplies the expected structural decision, and every positive witness is
replayed independently.

Neither oracle computes connected components of graph/complement recursively, so
it does not copy the production recurrence.

## Complexity and non-claims

Production materializes a structural `V x V` byte adjacency matrix. Each recursive
subproblem of size `k` may scan `O(k^2)` pairs in the graph and complement; a
maximally unbalanced recursive decomposition therefore gives a conservative
`O(V^3 + E)` direct time bound and `O(V^2)` adjacency storage plus the returned
`O(V)` cotree. The replay validator is intentionally verification-oriented.

This slice does **not** claim linear-time modular decomposition, a general modular
decomposition tree, prime-module decomposition, graph isomorphism canonization,
dynamic cograph maintenance, weighted semantics, or SPQR/triconnected
decomposition.

## Scope

Exactly three new paths are added:

- `include/algorithms/graphs/cograph.hpp`;
- `tests/test_cograph_cases.hpp`;
- `docs/scope_recovery_cograph.md`.

The verification architecture merged in PR #351 auto-enrolls
`tests/test_*_cases.hpp` in isolated translation units, so no CMake or
`tests/test_main.cpp` edit is required. No README, historical ROADMAP,
recovery-authority, workflow, benchmark, frozen compiler/backend, occupied
recovery surface, or temporary-file churn is included.
