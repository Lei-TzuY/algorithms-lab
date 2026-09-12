# Scope recovery — metric-TSP double-tree approximation

## Coverage decision

Fresh live state at `main@23b1d360501cd39a0f6c6e425b61967b74f47cc9` was green after exact prime-field extension arithmetic, with no open PRs. Live code and PR-history searches found the exact directed Held-Karp Hamiltonian-cycle solver but no metric-TSP approximation implementation; PR #218 explicitly records metric-TSP approximation as considered but not implemented. Searches also found no TSP recovery branch. Occupied fractional-cascading, minimum-cycle-basis, and general-graph-isomorphism surfaces are intentionally left untouched.

This slice deliberately changes proof model after the recent polynomial/CFG/coding streak: it adds a theorem-bounded approximation algorithm that composes the sealed MST and exact Hamiltonian-cycle layers rather than adding another finite-field or parser variant.

## Production contract

`approximate_metric_tsp_double_tree(distance)` accepts a complete symmetric integer metric: square matrix, zero diagonal, strictly positive distinct-vertex distances, symmetry, non-negativity, and every triangle inequality. Triangle checks avoid overflow in the validator.

Empty and singleton inputs return canonical zero-cost trivial tours. For at least two vertices the returned tour starts at vertex 0, visits every vertex exactly once, and returns to 0. The result also exposes the exact Kruskal MST edges and MST weight used by the approximation. Required aggregate weights outside signed 64-bit range fail closed.

## Approximation proof obligation

Let `OPT` be the optimum metric Hamiltonian-cycle cost and `T` the returned MST. Deleting any one edge from an optimum tour leaves a spanning tree, so `w(T) <= OPT`. Doubling every edge of `T` produces an Eulerian walk of cost `2*w(T)`. Shortcutting repeated vertices cannot increase cost by the triangle inequality. The deterministic first-visit DFS order is exactly such a shortcut of a doubled-tree traversal. Therefore `returned_cost <= 2*w(T) <= 2*OPT`.

The universal factor-two guarantee is this theorem boundary. Finite tests are implementation evidence, not a proof derived from random sampling.

## Verification

Focused pre-upload verification passed under strict GCC, strict Clang, and actual GCC ASan+UBSan. The focused harness ran the final production semantics against 500 random Manhattan metrics.

Repository-native evidence adds empty/singleton semantics; square/diagonal/positivity/symmetry/triangle validation; deterministic square-metric witness and MST replay; exact tour-sum overflow rejection; and 350 fixed-seed unique-point Manhattan metrics with 2-8 vertices. The primary exact oracle directly enumerates all tours rooted at vertex 0 and checks `MST <= OPT` and `tour <= 2*OPT`. As secondary cross-layer integration evidence, the same complete metric is passed to the sealed directed Held-Karp solver and its optimum must equal the independent permutation oracle.

Held-Karp is secondary integration evidence only; it is not the primary oracle for the approximation implementation.

## Complexity / non-claims

The direct implementation validates all `O(V^3)` triangle inequalities, materializes `Theta(V^2)` complete-graph edges, runs the sealed `O(E log E)` Kruskal baseline, and performs linear tree traversal. The public bound is `O(V^3 + V^2 log V)` time and `O(V^2)` auxiliary/result storage.

No Christofides 3/2 approximation, weighted perfect-matching implementation, asymmetric TSP, non-metric shortcut guarantee, Euclidean geometric acceleration, polynomial-time exact TSP, or large-instance performance claim is made.

## Scope

Exactly four paths differ from main: `include/algorithms/approximation/metric_tsp.hpp`, `tests/test_metric_tsp_cases.hpp`, one include line in `tests/test_main.cpp`, and this proof document. No CMake, README, stale ROADMAP, recovery-authority, workflow, benchmark, frozen compiler/backend, or occupied recovery-surface file changes.
