# algorithms-lab

A systematic rebuild of core algorithms and data-structure foundations in modern C++20.

This repository is **not** a LeetCode dump, competitive-programming archive, or solution-count project. The goal is to make a small set of foundational techniques explainable, testable, and verifiably correct before expanding breadth.

## Milestone 1: foundations + graph-search vertical slice

Implemented in this checkpoint:

- Searching: binary search returning the first equivalent index.
- Sorting: stable merge sort; median-of-three, three-way quicksort.
- Data structures: binary heap / priority queue from first principles; disjoint-set union with path compression and union by size.
- Graph model: directed or undirected adjacency lists, weighted edges, deterministic insertion-order iteration, explicit vertex validation, self-loops and parallel edges.
- Traversal: BFS, DFS, shortest unweighted path reconstruction, reachability, undirected connected components, directed/undirected cycle detection, DAG topological sort.
- Weighted shortest paths: Dijkstra for globally non-negative graphs and Bellman-Ford with reachable negative-cycle detection.
- Verification: deterministic edge cases, adversarial shapes, fixed-seed randomized tests, sorting/heap reference checks, and Dijkstra-vs-Bellman-Ford differential testing.

The important invariants and proof sketches live in [`docs/invariants.md`](docs/invariants.md). The future sequence is in [`ROADMAP.md`](ROADMAP.md); later phases are intentionally not implemented yet.

## Repository layout

```text
include/algorithms/       public APIs and template implementations
src/graphs/               graph/traversal/shortest-path implementations
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

The benchmark executable uses a fixed seed and reports wall-clock microseconds for selected operations. It is evidence for experiments, **not** a substitute for asymptotic analysis or correctness tests.

```bash
./build/algorithms_benchmark
```

Do not compare timings across machines or build modes without controlling the environment.

## Design constraints

- C++20 is the primary language.
- Standard containers are allowed as storage/oracles in tests, but `std::sort`, `std::priority_queue`, or a library DSU are not used as the demonstrated implementations.
- Dijkstra rejects the entire input graph if any negative edge exists, even if that edge is unreachable from the selected source.
- Bellman-Ford reports whether a negative cycle is reachable from the source; it does not yet return an explicit negative-cycle witness.
- Graph vertex IDs are dense integers in `[0, V)`.
- Insertion order determines adjacency iteration; no canonical sorting of neighbors is imposed.

## Current scope

Milestone 1 deliberately stops after the first shortest-path comparison. It does **not** claim algorithmic completeness. MST, SCC, dynamic programming, range-query structures, string algorithms, flows, geometry, and number theory belong to later reviewed phases.
