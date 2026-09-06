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

## Phase 3 — dynamic programming — IN PROGRESS

- [x] 0/1 and unbounded knapsack with reconstruction and independent randomized oracles
- [ ] Longest increasing subsequence
- [ ] Edit distance
- [ ] Interval DP
- [ ] Tree DP

## Phase 4 — range-query and structural data structures

- [ ] Fenwick tree
- [ ] Segment tree
- [ ] Sparse table
- [ ] Tries
- [ ] Advanced DSU variants

## Phase 5 — string algorithms

- [ ] KMP
- [ ] Z-function
- [ ] Rolling hash
- [ ] Suffix array and related structures

## Phase 6 — advanced graph/offline algorithms

- [ ] Max flow / min cut
- [ ] Bipartite matching
- [ ] Lowest common ancestor
- [ ] Offline algorithms

## Phase 7 — selected advanced foundations

- [ ] Computational geometry
- [ ] Number theory
- [ ] Selected advanced algorithms based on learning value
