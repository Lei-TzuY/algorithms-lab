# Scope recovery: undirected edge replacement paths

## Coverage decision

A fresh live audit at `main@b4971cde8067dedd3712e4c2ba5104a618063f1c`
after the exact original-arc DAG path-cover checkpoint found no replacement-paths
production API in default-branch code, pull-request history, or branch names.
Nearby shortest-path capabilities already include Dijkstra/Bellman-Ford/Johnson,
A*, radix-heap Dijkstra, Yen k-shortest simple paths, and weighted spanners; this
slice deliberately adds **single-edge failure sensitivity along one shortest
s-t path**, rather than another ordinary shortest-path query wrapper.

Prospective authority remains `docs/scope_recovery_after_phase69.md`; the frozen
compiler/backend excursion and historical ROADMAP are untouched.

## Production contract

`undirected_edge_replacement_paths(graph, source, target)` accepts an undirected
multigraph whose every stored edge weight is strictly positive.

- a deterministic shortest-path tree rooted at `source` selects one baseline
  source-target shortest path;
- the result replays that baseline by vertices and exact logical-edge witnesses;
- for every logical edge on that selected path, the result reports the exact
  shortest source-target distance after removing **that copy only**;
- parallel copies remain distinct through `(canonical endpoint,
  adjacency_index)` provenance;
- a disconnected replacement is represented by `nullopt`;
- the selected detour logical edge and its traversal direction are exposed for
  each reachable replacement;
- unreachable baseline target returns `nullopt`; `source == target` returns the
  zero-edge path;
- directed input and zero/negative weights are rejected;
- exact `INT64_MAX` distances are supported; a finite base or replacement
  distance above the public signed-64 range fails closed with `overflow_error`;
- capped internal arithmetic prevents an irrelevant enormous non-optimal walk
  from causing a false overflow.

Self-loops are legal when positive but cannot improve a shortest path or cross a
replacement cut. Edge weights are part of the objective; they are not ignored.

## Fundamental-cut / interval invariant

Let `P = p_0 ... p_k` be the deterministic shortest source-target path inside a
shortest-path tree `T` rooted at the source. Removing the `k` path edges from
`T` partitions the source component into tree pieces `T_0 ... T_k`, where
`p_i in T_i`.

A non-path logical edge joining `u in T_i` to `v in T_j`, `i < j`, crosses the
fundamental cut of exactly the baseline path edges with indices in `[i,j)`.
Its reduced replacement cost is

`d_s(u) + w(u,v) + d_t(v)`.

For a failed baseline tree edge, every surviving source-target path must cross
its fundamental cut through some non-tree edge. The classical undirected
replacement-path exchange argument therefore reduces the exact replacement
value to the minimum reduced cost among the crossing edges. Conversely the
minimum replacement edge yields the corresponding shortest detour. Strictly
positive weights keep the selected shortest-path-tree parent relation acyclic
and avoid zero-length tie subtleties outside this bounded baseline.

Production computes two capped Dijkstra distance maps only once, labels the
shortest-tree pieces once, converts every non-path edge into one contiguous
active interval, and sweeps the baseline path with the repository's
first-principles `BinaryHeap`. It does **not** rerun Dijkstra once per failure.

## Independent verification

Focused exact-candidate verification before upload passed:

- GCC C++20 repository strict warnings-as-errors: 4/4;
- Clang C++20 repository strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with fail-fast/leak detection: 4/4.

The committed randomized corpus uses 700 fixed-seed positive-weight undirected
multigraphs with 2-9 vertices, self-loops, parallel copies, disconnected cases,
and random source/target pairs. The primary oracle is intentionally the
**opposite algorithmic strategy**: after replaying the production baseline edge
witness, it removes that exact logical copy and independently reruns ordinary
Dijkstra for every failed path edge. Every production replacement distance must
match exactly.

A separate local 3,000-graph stress corpus also passed and is intentionally not
added to the full repository suite.

Deterministic evidence additionally covers directed/zero/negative rejection,
source-equals-target, unreachable targets, bridge-only paths, parallel-copy
identity, self-loops, detour provenance, exact `INT64_MAX`, finite replacement
overflow, baseline overflow, and a huge non-optimal route that must not trigger
false overflow.

## Complexity and non-claims

For `V` vertices and `E` logical undirected edge copies, two first-principles
binary-heap Dijkstra passes plus one interval sweep give conservatively
`O((V+E) log(E+1))` time and `O(V+E)` auxiliary/result storage. The returned
baseline contains `k <= V-1` edges.

This slice does **not** claim directed replacement paths, zero/negative-weight
support, vertex-failure replacement paths, multiple simultaneous failures,
all-pairs sensitivity, dynamic updates, a full replacement-path vertex sequence
for each failure, or a stronger asymptotic theorem than the implemented
fundamental-cut sweep.

## Scope

Exactly four repository paths change:

- `include/algorithms/graphs/replacement_paths.hpp`
- `tests/test_replacement_paths_cases.hpp`
- one include in `tests/test_main.cpp`
- `docs/scope_recovery_replacement_paths.md`

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark,
frozen compiler/backend, or occupied recovery-surface churn is introduced.
