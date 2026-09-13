# Scope recovery: Robbins strong orientation

## Coverage decision

A fresh post-Phase-69 live-state audit at exact `main@b7bf2119cc1f80ca36df52d04f665ad7f7b4de0e` found no Robbins / strong-orientation implementation in default-branch code, pull-request history, or the branch namespace. The currently occupied `scope-recovery-minimum-cycle-basis`, `scope-recovery-bfprt-selection`, and `scope-recovery-subset-convolution` surfaces are deliberately untouched.

Prospective work is governed by `docs/scope_recovery_after_phase69.md`; the historical compiler/backend ROADMAP headings remain non-authoritative and are not resumed.

This slice changes proof model again after the recent numerical and geometry recovery work. It adds the constructive side of Robbins' theorem for the repository's undirected multigraph abstraction rather than another numerical method, geometric primitive, or already-occupied recovery topic.

## Production contract

`strong_orientation(graph)` accepts only an undirected `Graph` and preserves the repository's deterministic multigraph semantics:

- directed input is rejected with `std::invalid_argument`;
- empty and singleton graphs are orientable;
- self-loops are retained and oriented to themselves;
- parallel edge copies remain distinct logical edges;
- edge weights are retained in the witness but are topologically irrelevant;
- disconnected input returns `StrongOrientationStatus::disconnected`;
- connected input with at least one bridge returns `StrongOrientationStatus::bridge` and one deterministic blocking-bridge witness;
- otherwise every logical edge is oriented exactly once and the result contains both a replayable orientation table and the constructed directed `Graph`.

Logical edges are reconstructed deterministically by scanning source vertices in increasing order. A self-loop is kept at its single adjacency occurrence; a non-loop undirected adjacency copy is kept only at its smaller endpoint. The resulting logical-edge index is therefore stable for a fixed input graph.

## Algorithm and invariants

Production is a first-principles iterative DFS/low-link construction. It does not call the sealed low-link or SCC implementations.

For each non-loop logical edge:

- a DFS tree edge is oriented parent to child when the child is first discovered;
- a non-tree edge is oriented on its first encounter, from the current descendant toward the already discovered endpoint;
- the exact parent *edge id*, rather than merely the parent vertex, is skipped while processing a child, so a second parallel edge to the parent remains a valid back edge;
- `low[v]` is the smallest discovery index reachable from `v` by zero or more DFS-tree edges followed by at most one non-tree edge;
- a DFS tree edge `(parent,v)` is a bridge exactly when `low[v] > discovery[parent]`.

When the graph is connected and no tree edge is a bridge, tree edges give a directed path from the root to every vertex. Every non-root DFS subtree has an oriented escape through a non-tree edge to an ancestor, and repeated escapes yield a directed route back toward the root. Robbins' theorem identifies this connected-and-bridgeless condition as exactly the existence criterion for a strongly connected orientation.

Tests are executable evidence for this implementation and theorem boundary; they do not constitute a proof of Robbins' theorem.

## Verification

Focused pre-upload verification used the exact live `Graph` and repository test-framework contracts and passed:

- GCC C++20 strict warnings-as-errors: 4/4;
- Clang C++20 strict warnings-as-errors: 4/4;
- GCC ASan+UBSan with leak detection and halt-on-error: 4/4.

Deterministic cases cover directed-input rejection, empty/singleton graphs, self-loops, signed weights, disconnected graphs, a concrete bridge obstruction, a triangle, and the two-parallel-edge graph whose strong orientation requires opposite directions.

The primary randomized oracle is structurally independent of the production recurrence. For 600 fixed-seed undirected multigraphs with `0..6` vertices and at most eight logical edges, it exhaustively enumerates every orientation of all non-loop logical edges and checks strong connectivity by independent forward/reverse breadth-first reachability. The production result must agree exactly with this existence oracle. Successful results additionally replay every logical edge and verify the directed witness is strongly connected; failed connected results must expose an edge whose removal independently disconnects the graph.

The sealed low-link and SCC implementations remain available as full-repository integration neighbors, but they are not used as the primary focused oracle.

## Complexity and non-claims

Let `E` denote logical undirected edge copies, including parallel edges and self-loops. Logical-edge reconstruction, incidence construction, iterative DFS/low-link propagation, and final witness construction each take `O(V + E)` time and `O(V + E)` auxiliary storage. The production DFS is iterative, so it does not consume recursion-stack space on deep graphs.

This slice does not claim weighted orientation optimization, minimum-cost orientation, strong orientation after adding the minimum number of edges, dynamic bridge maintenance, ear decomposition, or any stronger directed-connectivity objective. It is the direct constructive Robbins baseline for the existing static undirected multigraph model.
