# Roadmap

This roadmap is directional, not a completeness claim. Promotion happens only after the previous checkpoint has executable behavior, documented proof obligations/invariants, deterministic adversarial tests, and appropriate randomized/differential evidence.

## Phase 1 — foundations and graph search — SEALED

- [x] Binary search, merge sort, quicksort
- [x] Heap / priority queue and disjoint-set union
- [x] Graph representation, BFS, DFS, reachability/components, cycle detection, topological sort
- [x] Dijkstra and Bellman-Ford with differential verification

## Phase 2 — greedy and deeper graph structure — SEALED

- [x] Greedy algorithm pattern: interval scheduling with exchange argument and exhaustive small-instance oracle
- [x] Minimum spanning forests: Kruskal and Prim with differential verification
- [x] Strongly connected components: Tarjan and Kosaraju with partition/reachability verification
- [x] Advanced graph traversal integration: SCC condensation DAG verified through Phase-1 topological sort

## Phase 3 — dynamic programming — SEALED

- [x] 0/1 and unbounded knapsack with reconstruction and independent randomized oracles
- [x] Longest increasing subsequence with quadratic-vs-nlogn differential verification and reconstruction
- [x] Edit distance with deterministic edit-script reconstruction and metric/property verification
- [x] Interval DP: matrix-chain multiplication with reconstructable split plans and exhaustive small-instance verification
- [x] Tree DP: maximum-weight independent set with take/skip reconstruction and exhaustive small-tree verification

## Phase 4 — range-query and structural data structures — SEALED

- [x] Fenwick tree with transactional checked updates and randomized naïve-array differential verification
- [x] Segment tree with transactional point assignment and Fenwick/naïve cross-structure differential verification
- [x] Sparse table with immutable O(1) RMQ, deterministic argmin, and exhaustive randomized naïve verification
- [x] Byte trie with multiset prefix counts and randomized map/prefix-scan differential verification
- [x] Rollback DSU with reversible snapshots and randomized rebuilt-graph differential verification

## Phase 5 — string algorithms — SEALED

- [x] KMP with explicit prefix/failure state, overlap-aware all-match search, and naïve differential verification
- [x] Z-function with a rightmost half-open Z-box and naïve LCP differential verification
- [x] Rolling hash with double modular substring fingerprints and direct-polynomial differential verification
- [x] Suffix array with prefix-doubling order, inverse rank, Kasai LCP, and naïve suffix/LCP differential verification

## Phase 6 — advanced graph/offline algorithms — SEALED

- [x] Max flow / min cut: Dinic flow, per-edge witness, residual source-side cut, Edmonds-Karp + exhaustive-cut differential verification
- [x] Bipartite matching: Hopcroft-Karp matching witness, König minimum vertex cover, exhaustive + Dinic reduction verification
- [x] Lowest common ancestor: strict rooted-tree validation, binary lifting, distance/k-th ancestor queries, naïve differential verification
- [x] Offline dynamic connectivity: edge-active intervals, segment tree over time, rollback DSU, naïve temporal differential verification

## Phase 7 — selected advanced foundations — SEALED

- [x] Computational geometry foundations: exact bounded-integer predicates, segment intersection, and convex hull
- [x] Number theory foundations: Euclidean GCD, overflow-safe modular arithmetic, deterministic uint64 primality
- [x] Selected advanced integration: heavy-light decomposition over `Graph` + Phase-4 `SegmentTree` with mutable path/subtree sums

## Phase 8 — algebraic transforms and polynomial algorithms — SEALED

- [x] Radix-2 NTT and polynomial convolution over `998244353` with transform round-trip and naïve-convolution differential verification
- [x] Exact bounded integer convolution via multiple NTT primes + CRT with an explicit coefficient/representability bound
- [x] Formal power series inverse via Newton doubling over `998244353` with independent recurrence verification

## Phase 9 — weighted combinatorial optimization — SEALED

- [x] Minimum-cost maximum flow with signed costs, checked flow/cost certificates, residual-potential optimality evidence, and exhaustive small-flow differential verification
- [x] Assignment problem via Hungarian algorithm with exhaustive small-instance verification and a min-cost-flow reduction as cross-implementation integration evidence
- [x] Lower-bounded / demand min-cost circulation with explicit feasibility transformation and independent small-instance verification

## Phase 10 — general graph matching — SEALED

- [x] Maximum-cardinality matching in arbitrary undirected graphs via Edmonds blossom, with exhaustive small-graph verification and Hopcroft-Karp equality on bipartite instances


## Phase 11 — randomized algorithms and probabilistic contracts — SEALED

- [x] Karger-style randomized contraction for undirected global minimum cut with explicit seed/trial semantics, replayable cut witness, exhaustive small-graph optimum checks, and no unsupported finite-trial exactness claim

## Phase 12 — approximation algorithms and executable quality guarantees — SEALED

- [x] Deterministic 2-approximation for minimum vertex cover via maximal matching, with mandatory self-loop vertices, replayable matching/cover witness, exhaustive small-graph optimum checks, and explicit ratio validation

## Phase 13 — amortized self-adjusting data structures — SEALED

- [x] Splay-tree ordered set with zig/zig-zig/zig-zag rotations, access-to-root behavior, insertion/erasure, explicit structural invariants, randomized `std::set` differential verification, and an honest amortized-vs-worst-case complexity boundary

## Phase 14 — persistent and versioned data structures — SEALED

- [x] Persistent segment tree with immutable branching versions, `O(log n)` path-copy point assignment, structural sharing evidence, transactional failed-update behavior, checked `int64_t` range sums, and randomized version-DAG differential verification

## Phase 15 — online algorithms and competitive analysis — ACTIVE FRONTIER

- [x] Deterministic LRU paging with replayable hit/fault/eviction trace, independent exact offline optimum on bounded sequences, and an explicit theorem-derived competitive-ratio boundary
