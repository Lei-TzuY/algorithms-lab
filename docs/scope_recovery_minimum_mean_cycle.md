# Scope recovery — exact minimum mean directed cycle

## Why this recovery slice

After deterministic Huffman prefix coding reached merged-main green, a fresh live
code and PR-history audit found no minimum-mean-cycle implementation. The nearby
minimum-cost-circulation code optimizes total circulation cost under capacities
and demands; it does not solve the cycle-average objective or expose the Karp
walk-length invariant. This slice therefore changes proof model again rather
than farming another heap, string index, streaming sketch, or frozen backend
variant.

The frozen Phase-45–69 compiler/backend surface remains untouched. Prospective
selection continues to follow `docs/scope_recovery_after_phase69.md` plus fresh
live coverage audits, not stale historical ROADMAP headings.

## Production contract

`minimum_mean_cycle(const Graph&)` accepts directed signed-weight multigraphs.
Self-loops and parallel edge copies are supported. Undirected graphs are
rejected. A graph with no directed cycle returns `std::nullopt`.

A successful result contains:

- the exact reduced rational minimum mean `p/q`;
- one concrete directed cycle whose mean is exactly `p/q`;
- each witness edge's source vertex, adjacency index, destination, and weight, so
  parallel copies can be replayed against the original graph;
- the Karp terminal vertex and reference walk length selected by the min-max DP,
  exposed only as diagnostics.

All exact dynamic-programming, shifted-cost, potential, and witness sums use
checked signed `int64_t` arithmetic. Inputs whose required intermediate exact
arithmetic is not representable fail closed with `std::overflow_error`; no
arbitrary-precision claim is implied.

## Karp dynamic-programming invariant

Let `d[k][v]` be the minimum weight of any exactly-`k`-edge walk ending at `v`.
Every vertex is initialized as a zero-edge start, equivalent to adding a
conceptual zero-cost super-source whose edges are not counted in `k`. For
`n = |V|`, Karp's theorem gives the global minimum cycle mean as

`min_v max_{0 <= k < n} (d[n][v] - d[k][v]) / (n-k)`

over states that exist.

Fractions are compared exactly without cross multiplication. The implementation
uses a continued-fraction / Euclidean comparison, avoiding overflow that could
arise even when both individual numerator/denominator values are representable.
The selected rational is reduced by gcd before publication.

The theorem is the correctness proof obligation; randomized differential tests
are implementation evidence, not a substitute proof.

## Reconstructing a concrete optimum cycle

Karp's value alone is not accepted as the public witness. For the reduced optimum
`p/q`, each original edge weight `w` is shifted to `q*w - p`. By definition of
minimum mean, every directed cycle has non-negative shifted total and at least
one cycle has total zero.

A conceptual-super-source Bellman-Ford pass computes feasible potentials `h` for
the shifted graph. Every reduced edge cost

`q*w - p + h[u] - h[v]`

is therefore non-negative. On a zero-total optimum cycle those reduced costs
also sum to zero, so every edge on that cycle individually has reduced cost zero.
A deterministic iterative DFS of the zero-reduced-cost subgraph then reconstructs
one concrete cycle. Production replays its shifted sum and rejects any internal
inconsistency.

This witness step deliberately uses a different invariant from the Karp DP,
which makes the returned cycle independently checkable against the reported
mean.

## Verification

Deterministic evidence covers directed-input validation, empty/DAG no-cycle
semantics, negative self-loops, parallel edge copies, disconnected competing
cycles, exact negative rational means, replay determinism, exact `INT64_MIN` /
`INT64_MAX` self-loop means, and fail-closed path-sum overflow.

The primary differential oracle is independent of Karp's recurrence: 700
fixed-seed directed multigraphs with 0–6 vertices and up to 14 edge copies are
checked by exhaustive enumeration of every simple directed cycle. The oracle
uses only small weights `[-8,8]`, so its direct rational cross products remain
far from overflow. Every production witness is replayed through exact adjacency
indices and checked to attain the reported rational mean.

A separate local stress run raises this corpus to 5,000 graphs. The committed
suite remains 700 cases to avoid unnecessary full-suite inflation.

Focused pre-upload verification passes under repository-equivalent GCC C++20
strict warnings-as-errors, Clang C++20 strict warnings-as-errors, and actual GCC
ASan+UBSan builds.

## Complexity / non-claims

The Karp table costs `O(VE)` time and `O(V^2)` storage. Shifted-cost Bellman-Ford
adds `O(VE)` time, and zero-reduced-cost cycle extraction adds `O(V+E)`. Overall
time is `O(VE)` with `O(V^2 + E)` auxiliary/result storage.

This slice does not claim minimum cycle basis, cycle enumeration, arbitrary-
precision arithmetic, floating-point cycle ratios, library graph optimization,
or any compiler/backend continuation.
