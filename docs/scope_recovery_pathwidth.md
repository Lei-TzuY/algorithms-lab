# Scope recovery — exact bounded pathwidth

## Coverage decision

A fresh live audit at `main@3f7ae871cad2be27fa225dda2197aecc45c56b7a` selected exact graph pathwidth / vertex separation after the Levenshtein BK-tree checkpoint reached merged-main green.

Default-branch code search, pull-request history, and branch search found no pathwidth, vertex-separation-number, or path-decomposition implementation. Existing exact treewidth and minimum-fill checkpoints solve different objectives: treewidth minimizes the maximum filled elimination neighborhood, while pathwidth asks for a linear vertex order whose prefix frontier is as small as possible. The historical compiler/backend frontier remains frozen; prospective work follows `docs/scope_recovery_after_phase69.md` plus the fresh coverage audit.

## Production contract

`exact_pathwidth(const Graph&)` is an exact exponential baseline for undirected graphs with at most 20 vertices.

- directed input is rejected;
- self-loops and stored weights are ignored;
- parallel copies collapse to one structural adjacency relation;
- the empty graph has pathwidth zero by explicit API convention;
- the result contains the exact pathwidth;
- the result contains the lexicographically smallest optimal vertex ordering for the repository's dense vertex ids;
- `prefix_boundary_sizes[k]` records the number of vertices among the first `k` ordering positions that still have a neighbor in the suffix;
- `bags[i]` is a replayable path-decomposition bag for ordering position `i`.

No isomorphism-canonical ordering is claimed; lexicographic canonicalization is only with respect to the caller's existing vertex ids.

## Vertex-separation DP and proof obligation

For a vertex set `S`, let

`boundary(S) = |{u in S : u has a neighbor in V \\ S}|`.

For every prefix set `S`, production stores the best completion value

`F(S) = min_{v notin S} max(boundary(S), F(S union {v}))`,

with `F(V)=0`.

Every complete ordering corresponds to exactly one chain of prefix sets, and the maximum `boundary(S)` along that chain is its vertex-separation number. Therefore the subset recurrence minimizes vertex separation over every ordering. Correctness then relies on the classical theorem that graph pathwidth equals vertex-separation number.

The lexicographic reconstruction deliberately keeps the **global** optimum `W=F(empty)` fixed. At prefix `S`, it chooses the smallest vertex `v` for which `F(S union {v}) <= W`. It must not insist on locally attaining `F(S)`: an earlier prefix may already have paid width `W`, while a later suffix can have a smaller local optimum. A committed regression locks this distinction.

## Path-decomposition witness

For ordering `v_0,...,v_{n-1}` and prefix `P_i={v_0,...,v_{i-1}}`, production returns

`B_i = boundary_vertices(P_i) union {v_i}`.

Every graph edge is covered because its earlier endpoint remains in the boundary until the later endpoint is introduced. A vertex appears from its own introduction through the last bag before all of its later neighbors are introduced, so its bag occurrences are contiguous. Since `|B_i|-1 = boundary(P_i)`, the maximum returned bag width equals the returned vertex-separation optimum.

## Independent verification

Focused candidate verification passed before upload under the exact live `Graph` contract:

- GCC C++20 with repository strict warnings-as-errors: 5/5;
- Clang C++20 with repository strict warnings-as-errors: 5/5;
- actual GCC ASan+UBSan with leak detection: 5/5.

Deterministic coverage includes directed and size-limit rejection, empty/singleton semantics, paths, cycles, cliques, ignored self-loops/weights, parallel copies, deterministic replay, and the reconstruction regression where a locally better suffix would otherwise defeat the globally lexicographically smallest optimum order.

The primary randomized oracle is structurally independent from the subset DP: 300 fixed-seed undirected multigraphs with 0..7 vertices enumerate **every vertex permutation**, directly compute each ordering's prefix-separation width from a plain adjacency matrix, and select the first lexicographic optimum. Production width and complete ordering must match exactly. Every returned prefix boundary and path-decomposition bag is independently replayed, including edge coverage and the contiguous-bag property for every vertex.

The finite corpus is implementation evidence, not a proof of the pathwidth/vertex-separation theorem.

## Complexity and non-claims

For `n <= 20`, structural adjacency normalization is `O(V+E)` over input edge copies. Boundary precomputation and the subset recurrence each use `O(n 2^n)` time; reconstruction and witness materialization add `O(n^2)`. The DP uses `O(2^n+n^2)` auxiliary/result storage.

This checkpoint does not claim an FPT pathwidth algorithm, large-instance scalability, weighted/directed pathwidth, path-decomposition optimization under a different convention, treewidth equivalence, minimum-fill equivalence, or canonical labeling independent of vertex ids.

## Scope

Exactly four paths change from the green BK-tree main checkpoint:

- `include/algorithms/graphs/pathwidth.hpp`;
- `tests/test_pathwidth_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this focused proof/coverage document.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark, frozen compiler/backend, or temporary-file churn is part of this slice.
