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

### Phase 6 — advanced graph/offline algorithms — sealed

- Dinic max flow on directed multigraphs with explicit residual pairs, checked `int64_t` capacity arithmetic, per-original-edge realized flow, and residual source-side min-cut witness.
- Max-flow/min-cut equality is executable evidence: the returned cut capacity must equal the returned flow value before the result is exposed.
- Fixed-seed randomized flow graphs are checked against an independent Edmonds-Karp oracle and exhaustive cut enumeration, then replayed for capacity and conservation invariants.
- Hopcroft-Karp maximum bipartite matching returns reciprocal partner state plus a König minimum-vertex-cover witness.
- Four hundred fixed-seed bipartite multigraphs are checked against an exhaustive matching oracle and a unit-capacity reduction through the Phase-6 Dinic implementation; matching and cover witnesses are replayed independently.
- Binary-lifting LCA indexes a strictly validated rooted tree and exposes LCA, edge-distance, depth, and k-th-ancestor queries without retaining mutable graph state.
- Five hundred fixed-seed random trees execute 100 query rounds each against an independent BFS parent array and naïve parent-climb oracle.
- Offline dynamic connectivity compiles add/remove/query operations into edge-active intervals over a segment tree of time and evaluates them through the Phase-4 rollback DSU.
- Five hundred fixed-seed dynamic-connectivity traces of 120 operations are checked against an independent active-edge multiset that rebuilds adjacency and runs BFS for every query.

### Phase 7 — selected advanced foundations — sealed

- Exact bounded-integer 2D orientation, on-segment/intersection predicates, and deterministic Andrew monotone-chain convex hull.
- Convex hull is checked on 500 fixed-seed point multisets against an independent Jarvis-march oracle; segment predicates receive separate symmetry/adversarial coverage.
- Euclidean GCD, overflow-safe `uint64_t` modular multiplication/exponentiation, and deterministic Miller-Rabin classification over the unsigned 64-bit domain.
- Number-theory tests include full-width known-answer vectors plus 5,000 GCD, 5,000 modular-arithmetic, and 20,000 small-range primality differential cases; the fixed Miller-Rabin witness theorem remains an explicitly external theorem rather than a test-derived claim.
- Heavy-light decomposition integrates the existing strict-tree `Graph` surface with the Phase-4 checked `SegmentTree` for point assignment, inclusive path sums, and rooted-subtree sums.
- Three hundred fixed-seed random trees execute 120 mixed HLD operations each against independent BFS/parent-array naïve oracles.

### Phase 8 — algebraic transforms and polynomial algorithms — sealed

- Iterative radix-2 NTT over `998244353` with fixed primitive root `3`, explicit `2^23` transform-order boundary, forward/inverse transforms, and modular polynomial convolution.
- Transform round trips cover every power-of-two size through 4096; 600 fixed-seed random polynomial pairs are checked exactly against an independent `O(nm)` modular-convolution oracle.
- Exact signed `int64_t` convolution reuses one parameterized NTT engine over `998244353` and `1004535809`, then performs centered two-prime CRT reconstruction only when a conservative coefficient bound proves uniqueness.
- Five hundred fixed-seed signed polynomial pairs are compared with an independent exact `O(nm)` oracle; unproven cancellation-heavy inputs fail closed instead of being mislabeled exact.
- Formal power series inversion over `998244353` uses Newton doubling and delegates both polynomial products to the NTT convolution substrate.
- Four hundred fixed-seed random series are checked against an independent `O(n^2)` coefficient recurrence; the `2^22` public prefix bound is derived from the existing `2^23` NTT transform limit.

### Phase 9 — weighted combinatorial optimization — sealed

- Minimum-cost maximum flow maximizes integral source-to-sink flow first, then minimizes signed total cost, with checked arithmetic, input-order flow witnesses, max-flow/min-cut evidence, and residual-potential optimality certification.
- Three hundred fixed-seed small networks are checked against an independent exhaustive edge-flow assignment oracle; reverse residual rerouting and negative-cycle domain boundaries have deterministic regressions.
- Rectangular Hungarian assignment returns a deterministic injective row-to-column witness and is checked on 500 fixed-seed matrices against exhaustive assignment enumeration, with the merged min-cost-flow API used only as cross-layer integration evidence.
- Lower-bounded / demand minimum-cost circulation separates feasibility from optimization: Phase-6 Dinic proves bounded-demand feasibility, then negative residual cycles are cancelled until a feasible residual-potential certificate exists.
- Three hundred fixed-seed bounded-circulation instances are checked against exhaustive bounded-flow enumeration, including infeasible cases, negative cycles, self-loops, exact `INT64_MIN` total cost, and overflow rejection.


### Phase 10 — general graph matching — sealed

- Edmonds blossom maximum-cardinality matching extends the graph-matching surface from bipartite graphs to arbitrary undirected graphs with odd cycles.
- The returned mate witness is symmetric; self-loops are ignored, parallel endpoint pairs are collapsed for ordinary matching semantics, edge weights are irrelevant, and directed input is rejected.
- Deterministic adversarial coverage includes triangle/5-cycle blossoms, an augmenting tail, and the Petersen graph; 1,000 fixed-seed general graphs are checked against an independent exhaustive matching oracle.
- Five hundred fixed-seed bipartite instances cross-check exact cardinality against the sealed Phase-6 Hopcroft-Karp implementation without using it as the primary oracle.

Correctness notes for Phase 1 live in [`docs/invariants.md`](docs/invariants.md), Phase 2 in [`docs/phase2_greedy_graph_structure.md`](docs/phase2_greedy_graph_structure.md), Phase 3 in [`docs/phase3_dynamic_programming.md`](docs/phase3_dynamic_programming.md), Phase 4 in [`docs/phase4_range_structures.md`](docs/phase4_range_structures.md), Phase 5 in [`docs/phase5_string_algorithms.md`](docs/phase5_string_algorithms.md), Phase 6 in [`docs/phase6_advanced_graph_offline.md`](docs/phase6_advanced_graph_offline.md) plus its [`sealing audit`](docs/phase6_sealing_audit.md), Phase 7 in [`docs/phase7_advanced_foundations.md`](docs/phase7_advanced_foundations.md), [`docs/phase7_number_theory.md`](docs/phase7_number_theory.md), [`docs/phase7_heavy_light_decomposition.md`](docs/phase7_heavy_light_decomposition.md), plus the [`Phase-7 sealing audit`](docs/phase7_sealing_audit.md), Phase 8 in [`docs/phase8_algebraic_transforms.md`](docs/phase8_algebraic_transforms.md), [`docs/phase8_exact_convolution.md`](docs/phase8_exact_convolution.md), [`docs/phase8_formal_power_series.md`](docs/phase8_formal_power_series.md), plus the [`Phase-8 sealing audit`](docs/phase8_sealing_audit.md), and Phase 9 in [`docs/phase9_weighted_optimization.md`](docs/phase9_weighted_optimization.md), [`docs/phase9_min_cost_circulation.md`](docs/phase9_min_cost_circulation.md), plus the [`Phase-9 sealing audit`](docs/phase9_sealing_audit.md), and Phase 10 in [`docs/phase10_general_matching.md`](docs/phase10_general_matching.md) plus the [`Phase-10 sealing audit`](docs/phase10_sealing_audit.md). The ordered sequence is in [`ROADMAP.md`](ROADMAP.md).

## Repository layout

```text
include/algorithms/       public APIs and template implementations
src/dynamic_programming/  dynamic-programming implementations
src/greedy/               greedy algorithm implementations
src/graphs/               graph/traversal/path/structure implementations
src/polynomials/          transform and polynomial-algebra implementations
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
- Offline dynamic connectivity uses undirected multiedge semantics; inactive removal is rejected, self-loops are temporally balanced but connectivity-neutral, and the reused rollback DSU has no path compression. Ordered-map pairing is `O(T log D)` and temporal DSU work is `O((A log T + Q) log V)`.
- Geometry predicates are exact only inside the documented coordinate domain `[-1e9, 1e9]`; larger coordinates are rejected rather than evaluated with overflow or epsilon arithmetic.
- Number-theory modular arithmetic is first-principles and overflow-safe across `uint64_t`; deterministic primality relies on the documented seven-witness theorem and is not presented as a cryptographic primitive.
- Heavy-light decomposition accepts only a non-empty connected acyclic undirected simple graph and inherits the checked `int64_t` representability/transactional-update boundary of the Phase-4 `SegmentTree`.
- NTT polynomial arithmetic is deliberately fixed to `998244353` with primitive root `3`; the root fact is a mathematical parameter assumption, non-empty transforms must be powers of two, and the largest transform order is `2^23`.
- Exact integer convolution uses a second NTT prime and centered CRT only when a conservative input-derived bound proves every coefficient lies in the unique reconstruction interval; otherwise it rejects rather than aliasing modulo the CRT product.
- Formal power series inversion is defined over `998244353`; a non-zero constant coefficient is required, and the requested prefix is capped at `2^22` so every Newton-step convolution remains within the existing `2^23` NTT boundary.
- Minimum-cost maximum flow rejects `INT64_MIN` edge costs and globally rejects positive-capacity negative-cost input cycles; its final no-negative-residual-cycle potential certifies cost optimality only inside that supported domain.
- Hungarian assignment solves complete rectangular matrices with `rows <= columns`; it is exact over representable signed `int64_t` costs and fails closed on unrepresentable dual/slack/total arithmetic.
- Lower-bounded min-cost circulation treats feasibility as a separate bounded-flow problem, permits negative residual cycles during optimization, and uses a pseudo-polynomial cycle-cancelling phase rather than claiming a stronger bound.
- General matching accepts undirected graphs only, ignores edge weights/self-loops, collapses parallel endpoint pairs, uses dense `O(V^2 + E)` preprocessing storage, and conservatively claims `O(V^2 E)` runtime rather than borrowing a stronger textbook bound.
- Graph vertex IDs are dense integers in `[0, V)`.

## Current frontier

Phases 1–10 are sealed. Phase 11 is active: randomized algorithms and probabilistic contracts. The first executable hypothesis is Karger-style undirected global minimum cut with explicit seed/trial semantics, replayable cut witnesses, and exhaustive small-graph optimum verification. A finite randomized run is not to be mislabeled as a deterministic exact guarantee.
