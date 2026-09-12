# Scope recovery: exact general multigraph isomorphism

## Coverage decision

A fresh post-Phase-69 audit at `main@3fcc7e84a7fb85761a47999e3f7774b87d177965` found the sealed strict-tree AHU isomorphism capability but no merged exact general-graph-isomorphism implementation. The long-lived `scope-recovery-general-graph-isomorphism` branch was directly compared against live main and is stale (`ahead 0`), so it no longer represents active implementation work. The separate `scope-recovery-minimum-cycle-basis` branch is genuinely ahead (`ahead 1`) and remains untouched.

This slice changes proof model after optimal-BST / Knuth optimization: paired graph invariants plus individualization/backtracking, rather than another interval-DP or occupied cycle-basis surface.

## Production contract

- both inputs must be undirected `Graph` instances; directed input is rejected
- vertex counts may differ; unequal counts return `std::nullopt`
- self-loops and parallel edges are preserved exactly through adjacency multiplicity
- edge weights are intentionally ignored
- successful output returns a bijection in both directions plus an exact search-node diagnostic
- repeated execution is deterministic for the same stored adjacency structure
- empty graphs are isomorphic and return empty mappings

## Algorithm and invariant

Production first materializes the full adjacency-multiplicity matrix of each graph. It computes joint isomorphism-invariant vertex colors from loop count, multiplicity degree, sorted row multiplicities, then repeatedly refines colors by edge-multiplicity totals into every current color class. If corresponding refined color-class sizes differ, the graphs cannot be isomorphic.

Exactness does **not** rely on color refinement being complete. Search maintains a partial bijection. For every unassigned vertex, its dynamic signature consists of its stable refinement color plus exact edge multiplicities to every already individualized vertex. The next first-graph vertex is chosen from the smallest remaining compatible signature class; second-graph candidates are tried in increasing vertex id. Any branch whose dynamic-signature class cardinalities disagree is pruned. A complete mapping is accepted only after replaying every entry of the two adjacency-multiplicity matrices.

Every isomorphism preserves all refinement signatures and all multiplicities to individualized vertex pairs, so pruning cannot remove a valid isomorphism. Conversely, the leaf replay is an exact certificate. Correctness therefore follows from exhaustive individualization over every surviving image combined with exact witness replay; 1-WL-style refinement is only a sound pruning device, not an exactness assumption.

## Verification

Focused final candidate passed:

- GCC C++20 strict warnings-as-errors: 5/5
- Clang C++20 strict warnings-as-errors: 5/5
- actual GCC ASan+UBSan: 5/5

Committed evidence covers directed rejection; empty/singleton behavior; self-loop and parallel-edge multiplicity; weight invariance; explicit bijection replay; deterministic repeated execution; a `C6` versus two-disjoint-triangles regression where ordinary degree/color information is insufficient; and an 8-vertex complete-graph symmetry case.

The primary randomized oracle is structurally independent of production refinement/search heuristics: 450 fixed-seed undirected multigraph pairs with 0..7 vertices are decided by enumerating every vertex permutation and directly comparing complete adjacency-multiplicity matrices. Half the corpus is constructed by random relabeling and deliberately changes edge weights; the other half is generated independently. Production existence must match the permutation oracle exactly, and every returned mapping is independently replayed.

## Complexity / non-claims

For `n` vertices and `m` stored adjacency entries, multiplicity materialization uses `O(n^2 + m)` time/storage. Refinement is polynomial, but the individualization search is factorial in the worst case. A conservative direct bound is `O(n! * n^3 + m)` time and `O(n^2)` resident algorithm state plus `O(n)` recursion depth.

No polynomial-time or quasi-polynomial-time general-isomorphism bound, canonical labeling, automorphism-group enumeration, nauty/Traces-style partition-refinement sophistication, directed-graph support, or weighted/labeled isomorphism claim is made.

## Scope

The intended recovery checkpoint changes exactly four paths:

- `include/algorithms/graphs/general_graph_isomorphism.hpp`
- `tests/test_general_graph_isomorphism_cases.hpp`
- one include line in `tests/test_main.cpp`
- `docs/scope_recovery_general_graph_isomorphism.md`

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark, compiler/backend, minimum-cycle-basis, or temporary-file surface is touched.
