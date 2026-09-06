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

### Phase 3 — dynamic programming — in progress

- 0/1 knapsack with explicit `dp[i][c]` state, deterministic reconstruction, zero-weight 0/1 items, and checked value overflow.
- Unbounded knapsack with the contrasting same-row inclusion recurrence, explicit reconstruction counts, and strict positive-weight precondition.
- Randomized 0/1 cases are checked against exhaustive subset enumeration; unbounded cases are checked against an independent capacity-recursion oracle.
- Longest increasing subsequence is implemented twice: textbook `O(n^2)` ending-state DP and a first-principles `O(n log n)` tails/binary-search formulation, both with witness reconstruction.
- LIS lengths are checked against exhaustive small-instance oracles and the two implementations are differentially compared on larger randomized inputs.
- Levenshtein edit distance uses explicit prefix-state DP and returns a deterministic match/substitute/erase/insert script that can be applied back to the source.
- Edit distance is checked against a separate two-row oracle plus symmetry, length-bound, and triangle-inequality properties.

Correctness notes for Phase 1 live in [`docs/invariants.md`](docs/invariants.md), Phase 2 in [`docs/phase2_greedy_graph_structure.md`](docs/phase2_greedy_graph_structure.md), and the current DP state model in [`docs/phase3_dynamic_programming.md`](docs/phase3_dynamic_programming.md). The ordered sequence is in [`ROADMAP.md`](ROADMAP.md).

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
- Demonstrated heap/DSU/sorting logic remains first-principles rather than hidden behind library equivalents.
- Dijkstra rejects the entire graph if any negative edge exists.
- MST requires undirected input, supports disconnected graphs as forests, ignores self-loops, supports parallel/negative edges, and checks total-weight overflow.
- SCC decomposition requires directed input; component IDs are not canonical labels.
- Knapsack DP is deliberately `O(n * capacity)` in both time and table storage to keep the state transition and reconstruction explicit; this is pseudo-polynomial in the numeric capacity.
- 0/1 knapsack permits zero-weight items; unbounded knapsack rejects every zero-weight item so the mathematical optimum/reconstruction remains well-defined.
- Edit distance is byte-oriented over `std::string_view`; operation kinds, not sentinel characters, distinguish insert/erase from embedded null bytes.
- Graph vertex IDs are dense integers in `[0, V)`.

## Current frontier

Phases 1 and 2 are sealed. Phase 3 now covers state/reconstruction across knapsack, LIS, and edit distance. The next ordered Phase-3 frontier is **interval DP**, followed by tree DP. The repository does not claim completeness.
