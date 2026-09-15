# Scope recovery: exact minimum fill-in / chordal completion

## Coverage decision

A fresh post-Phase-69 live audit at exact green `main@164f3ba996b4ff68264cefd66bf2681494c7addf` found no minimum-fill / minimum-chordal-completion implementation in default-branch code, pull-request history, or the branch namespace. The repository already contains exact treewidth and chordal recognition, but those solve different objectives: treewidth minimizes the largest elimination bag, while minimum fill-in minimizes the number of edges added by elimination. The latest merged recovery checkpoint is exact alpha-beta minimax, so this slice deliberately changes proof model again instead of extending game-tree search, dense numerical linear algebra, or separation-pair analysis.

Prospective scope follows `docs/scope_recovery_after_phase69.md`; the historical compiler/backend ROADMAP remains frozen and is not modified.

## Production contract

`exact_minimum_fill_in(graph)` accepts an undirected repository `Graph` with at most 20 vertices and returns:

- the exact minimum number of fill edges needed to make the graph chordal;
- the lexicographically smallest optimal elimination order;
- every fill edge in the order it is first introduced;
- a per-step replay witness containing the eliminated vertex, its current filled-neighbor set, and exactly the edges added to clique that neighbor set.

The repository multigraph is projected to its underlying simple graph: self-loops are ignored, parallel copies collapse, and stored weights do not affect the objective. Directed input and instances above the explicit exact-exponential limit are rejected.

## Set-torso recurrence and proof obligation

For an eliminated vertex set `S`, the graph induced on the remaining vertices after eliminating exactly `S` is independent of the order used inside `S`. Two remaining vertices are adjacent in this torso exactly when the original graph contains a path between them whose internal vertices all lie in `S`.

Consequently the local cost of eliminating a remaining vertex `v` depends only on `S`: it is the number of missing edges among `v`'s torso neighbors. If `c(S,v)` is that number, the exact recurrence is

`F(S) = min_{v notin S} (c(S,v) + F(S union {v}))`, with `F(V)=0`.

Each missing neighbor-pair edge is introduced exactly once: after introduction it remains a torso edge for every suffix state. Therefore the sum of local costs equals the completion size of that elimination order. The classical equivalence between chordal completions and perfect elimination orders then makes minimizing the recurrence equivalent to minimum fill-in.

Reconstruction scans candidate vertices in ascending order and picks the first transition attaining the optimum, yielding the lexicographically smallest optimal elimination order. Replaying the returned step edges makes every later-neighbor set a clique, so the order itself is an executable perfect-elimination certificate for the completed graph.

These torso and chordal-completion equivalences are mathematical proof obligations; finite tests are implementation evidence rather than theorem substitutes.

## Verification

Focused final candidate verification passes the repository warning policy under:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- actual GCC ASan+UBSan with fail-fast/leak detection: 4/4.

Deterministic evidence covers empty/singleton input, directed and size-limit rejection, chordal paths/trees, `C4` and `C5` exact fill witnesses, parallel edges with differing weights, ignored self-loops/weights, repeated deterministic execution, and complete per-step witness replay.

The primary randomized oracle is intentionally independent of the subset recurrence: for 180 fixed-seed undirected multigraphs with 0..7 vertices it enumerates **every vertex permutation**, mutates an ordinary adjacency matrix by cliquing current neighbors at each elimination, counts the actually inserted edges, and selects the first lexicographic minimum. Production fill count and complete elimination order must match exactly. Every production step is separately replayed against a mutable graph, including the current-neighbor and newly-added-edge vectors.

## Complexity and non-claims

For every subset state, production builds the set torso and evaluates all local fill costs with fixed-width masks in `O(n^2)` work. The direct bounded implementation therefore uses `O(n^2 2^n)` time and `O(2^n+n)` auxiliary DP state, excluding the returned `O(n^2)` worst-case witness.

This slice does not claim an FPT minimum-fill algorithm, minimal triangulation enumeration, weighted fill-in, sparse large-instance heuristics, chordal sandwich constraints, polynomial-time solvability, or a canonical optimum independent of vertex ids. Standard-library containers/bit operations provide storage and ordering only; they do not implement the minimum-fill algorithm.

## Scope

Exactly four paths differ from the exact green base:

- `include/algorithms/graphs/minimum_fill_in.hpp`;
- `tests/test_minimum_fill_in_cases.hpp`;
- one include line in `tests/test_main.cpp`;
- this recovery proof document.

No CMake, README, historical ROADMAP, recovery-authority, workflow, benchmark, frozen compiler/backend, minimum-cycle-basis, or unrelated recovery surface is modified.
