# algorithms-lab

A systematic rebuild of core algorithms and data-structure foundations in modern C++20.

This repository is **not** a LeetCode dump, competitive-programming archive, or solution-count project. The goal is to make a deliberately bounded set of techniques explainable, testable, reusable, and verifiably correct before expanding breadth.

## Verified capability checkpoints

### Phase 1 — foundations and graph search — sealed

- Binary search; stable merge sort; median-of-three three-way quicksort.
- First-principles binary heap / priority queue and DSU with path compression + union by size.
- Directed/undirected weighted graph representation with deterministic insertion-order adjacency, validation, self-loops, and parallel edges.
- BFS, DFS, unweighted path reconstruction, reachability/components, cycle detection, DAG topological sort.
- Dijkstra and Bellman-Ford with fixed-seed randomized differential verification.

### Phase 2 — greedy reasoning and deeper graph structure — sealed

- Maximum-cardinality interval scheduling by earliest finish time, checked against exhaustive small-instance oracles.
- Kruskal minimum spanning forest using the lab DSU.
- Prim minimum spanning forest using the lab binary heap.
- Directed SCC decomposition with independently implemented Tarjan and Kosaraju algorithms.
- SCC condensation DAG integrated with the existing topological-sort capability.
- Randomized Kruskal-vs-Prim and Tarjan-vs-Kosaraju differential/property verification.

### Phase 3 — dynamic programming — sealed

- 0/1 and unbounded knapsack with explicit capacity/item-prefix state, deterministic reconstruction, checked value arithmetic, and independent randomized oracles.
- Longest increasing subsequence in both textbook `O(n^2)` ending-state DP and first-principles `O(n log n)` tails/binary-search form, both with witness reconstruction and exhaustive/differential verification.
- Byte-oriented Levenshtein edit distance with deterministic executable edit-script reconstruction, an independent two-row oracle, and metric/property checks.
- Matrix-chain interval DP with checked `uint64_t` cost arithmetic, deterministic split plans, exhaustive small-chain parenthesization comparison, and replayed reconstruction.
- Tree DP for maximum-weight independent set on the existing undirected `Graph`, with strict tree validation, take/skip reconstruction, checked arithmetic, and exhaustive small-tree verification.

### Phase 4 — range-query and structural data structures — sealed

- Fenwick tree with first-principles binary-indexed decomposition, zero-based point updates, half-open prefix/range queries, checked arithmetic, transactional overflow behavior, and randomized naïve-array differential verification.
- Segment tree with bottom-up explicit interval hierarchy, point assignment, half-open range sums, transactional ancestor-path recomputation, checked node/query arithmetic, and randomized Fenwick/naïve cross-structure verification.
- Sparse table for immutable range minimum with `O(n log n)` preprocessing, `O(1)` non-empty half-open queries, deterministic leftmost argmin reconstruction, and exhaustive randomized naïve verification.
- Byte trie with arbitrary-byte keys, duplicate/empty-key multiplicity, exact and prefix counts, and randomized map/prefix-scan differential verification.
- Rollback DSU with union-by-size reversible snapshots, no path compression, and randomized rebuilt-graph differential verification.

### Phase 5 — string algorithms — sealed

- KMP with explicit prefix/failure state, overlap-aware all-occurrence search, byte-oriented semantics, and randomized naïve differential verification.
- Z-function with explicit `z[0] = n`, rightmost half-open prefix-match-box reuse, arbitrary-byte semantics, and randomized naïve LCP differential verification.
- Double-modular rolling hash with immutable substring fingerprints, arbitrary-byte semantics, O(1) extraction, direct-polynomial randomized verification, and explicit collision/non-cryptographic limits.
- Suffix array with prefix doubling, inverse suffix ranks, Kasai adjacent-LCP reconstruction, unsigned-byte ordering, and randomized raw-suffix/LCP differential verification.

### Phase 6 — advanced graph/offline algorithms — active

- Dinic max flow on directed multigraphs with explicit residual pairs, checked `int64_t` capacity arithmetic, per-original-edge realized flow, and residual source-side min-cut witness.
- Max-flow/min-cut equality is executable evidence: the returned cut capacity must equal the returned flow value before the result is exposed.
- Fixed-seed randomized flow graphs are checked against an independent Edmonds-Karp oracle and exhaustive cut enumeration, then replayed for capacity and conservation invariants.
- Hopcroft-Karp maximum bipartite matching returns reciprocal partner state plus a König minimum-vertex-cover witness.
- Four hundred fixed-seed bipartite multigraphs are checked against an exhaustive matching oracle and a unit-capacity reduction through the Phase-6 Dinic implementation; matching and cover witnesses are replayed independently.
- Binary-lifting LCA indexes a strictly validated rooted tree and exposes LCA, edge-distance, depth, and k-th-ancestor queries without retaining mutable graph state.
- Five hundred fixed-seed random trees execute 100 query rounds each against an independent BFS parent array and naïve parent-climb oracle.

Correctness notes for Phase 1 live in [`docs/invariants.md`](docs/invariants.md), Phase 2 in [`docs/phase2_greedy_graph_structure.md`](docs/phase2_greedy_graph_structure.md), Phase 3 in [`docs/phase3_dynamic_programming.md`](docs/phase3_dynamic_programming.md), Phase 4 in [`docs/phase4_range_structures.md`](docs/phase4_range_structures.md), Phase 5 in [`docs/phase5_string_algorithms.md`](docs/phase5_string_algorithms.md), and the active Phase 6 proof obligations in [`docs/phase6_advanced_graph_offline.md`](docs/phase6_advanced_graph_offline.md). The ordered sequence is in [`ROADMAP.md`](ROADMAP.md).

## Repository layout

```text
include/algorithms/       public APIs and template implementations
src/dynamic_programming/  dynamic-programming implementations
src/greedy/               greedy algorithm implementations
src/graphs/               graph/traversal/path/structure implementations
tests/                    deterministic, adversarial, randomized, differential tests
benchmarks/               fixed-seed micro-benchmark harness
examples/                 small usage examples
docs/                     invariants and correctness notes
.github/workflows/        CI build/test/sanitizer jobs
```

## Build and test

Requirements: CMake 3.20+ and a C++20 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Strict warnings are enabled for project targets and treated as errors. To run AddressSanitizer + UndefinedBehaviorSanitizer on GCC/Clang:

```bash
cmake -S . -B build-asan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DALGORITHMS_ENABLE_SANITIZERS=ON
cmake --build build-asan --parallel
ctest --test-dir build-asan --output-on-failure
```

## Benchmark harness

The benchmark executable uses a fixed seed and reports wall-clock microseconds for selected operations. It is experimental evidence, **not** a substitute for asymptotic analysis or correctness tests.

```bash
./build/algorithms_benchmark
```

Do not compare timings across machines or build modes without controlling the environment.

## Design constraints

- C++20 is the primary language.
- Standard containers and library algorithms may support an implementation when they are not the subject being demonstrated; tests may use independent library/oracle formulations.
- Demonstrated heap/DSU/sorting/range/string logic remains first-principles rather than hidden behind library equivalents.
- Dijkstra rejects the entire graph if any negative edge exists.
- MST requires undirected input, supports disconnected graphs as forests, ignores self-loops, supports parallel/negative edges, and checks total-weight overflow.
- SCC decomposition requires directed input; component IDs are not canonical labels.
- Knapsack DP deliberately keeps full tables for visible recurrence/reconstruction and is pseudo-polynomial in numeric capacity.
- 0/1 knapsack permits zero-weight items; unbounded knapsack requires strictly positive weights.
- LIS is strictly increasing; equal values never extend a subsequence.
- Edit distance is byte-oriented over `std::string_view`; operation kinds, not sentinel characters, distinguish insert/erase from embedded null bytes.
- Matrix-chain costs are represented in `uint64_t`; overflowing parenthesizations are excluded while representable alternatives remain eligible.
- Tree DP accepts only connected acyclic undirected simple graphs; edge weights are irrelevant to its vertex-weight objective.
- Fenwick internal buckets and returned sums use checked `int64_t`; updates that would make an internal bucket unrepresentable are rejected transactionally.
- Segment-tree internal nodes and returned sums use checked `int64_t`; construction/assignment rejects any unrepresentable node summary transactionally.
- Sparse-table RMQ is immutable; queries must be non-empty and ties return the leftmost minimum index.
- Byte-trie keys are arbitrary byte strings and insertion multiplicity is preserved; prefix counts include duplicate keys.
- Rollback DSU uses union by size without path compression so every successful union has a compact exact undo record.
- KMP operates on arbitrary bytes; an empty pattern matches every boundary and full-match fallback preserves overlapping occurrences.
- Z-function operates on arbitrary bytes, defines `z[0] = n` for non-empty input, and exposes prefix-LCP state rather than delimiter-based matching.
- Rolling hash uses fixed public non-cryptographic parameters; equal fingerprints are only a collision-prone candidate filter and must not be treated as proof of byte equality. No numeric collision-probability claim is made without an input/adversary model.
- Suffix-array ordering uses unsigned byte values, excludes a synthetic empty suffix, and the prefix-doubling implementation intentionally claims `O(n log^2 n)` rather than radix/counting-sort complexity it does not implement.
- Max flow is defined on directed capacity edges with non-negative `int64_t` capacities; parallel and antiparallel edges are independent, self-loops return canonical zero flow, and an unrepresentable total flow is rejected rather than wrapped.
- Dinic claims the general `O(V^2 E)` bound; the blocking-flow DFS is recursive and can use `O(V)` call stack on a deep level graph.
- Bipartite matching uses explicit left/right vertex domains, accepts parallel edges, returns a König minimum-vertex-cover certificate, and claims the standard Hopcroft-Karp `O(E sqrt(V))` bound; its augmenting DFS can use `O(V)` call stack.
- The Dinic reduction in bipartite tests is cross-layer integration evidence, not the primary matching oracle; exhaustive matching remains structurally independent from Hopcroft-Karp.
- LCA construction accepts only a non-empty connected acyclic undirected simple graph with a valid root; edge weights are ignored, preprocessing is `O(V log V)`, and LCA/k-th-ancestor queries are `O(log V)`.
- Graph vertex IDs are dense integers in `[0, V)`.

## Current frontier

Phases 1–5 are sealed. Phase 6 is active: max flow / min cut, bipartite matching, and LCA are executable; offline algorithms are the remaining ordered Phase-6 frontier. The repository does not claim completeness.
