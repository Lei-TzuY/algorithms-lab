# Phase 10 sealing audit — general graph matching

Phase 10 is sealed at integrated `main@e12c0ace903a5c3b4ef474fae69e31e794abd5eb`. Pull request #37 passed the repository GCC Release, Clang Release, and GCC ASan+UBSan jobs on exact candidate `fd54a0c295799c069483d84b072d250a5ad59a39`; merged-main workflow run `34084129539` then passed the same three-way matrix.

## Architecture boundary

The phase closes one structural matching gap rather than collecting variants. Phase-6 Hopcroft-Karp covers bipartite graphs and exposes a König cover witness; Phase 10 adds maximum-cardinality matching for arbitrary undirected graphs by contracting odd blossoms. This strictly expands the supported graph structure while keeping the objective unchanged.

Weighted blossom is not automatically promoted inside this phase. Introducing edge-weight objectives would create a materially different optimization subsystem with new dual structures, arithmetic boundaries, and verification obligations. Adding it merely because it is related by name would violate the repository's depth-over-count rule.

## Correctness and evidence audit

The implementation exposes a replayable symmetric mate witness and validates directed-vs-undirected semantics at the API boundary. Self-loops are ignored and parallel endpoint pairs are collapsed because neither can create an additional ordinary matching pair.

The alternating-forest implementation records parent, current blossom base, outer/queued state, and symmetric matching state. Odd cycles are contracted around a common base and each successful augment increases cardinality by one. Correctness depends on Berge's augmenting-path criterion and Edmonds' blossom-contraction theorem; the repository explicitly treats those as mathematical facts rather than claiming that randomized testing proves them.

The primary executable oracle is structurally independent: 1,000 fixed-seed general graphs with at most nine vertices are solved by exhaustive recursive matching enumeration. Returned witnesses are separately replayed for symmetry, endpoint existence, distinctness, and cardinality. Five hundred bipartite graphs additionally cross-check cardinality against sealed Hopcroft-Karp, but that cross-implementation comparison is secondary evidence rather than the primary oracle. Deterministic regressions include triangles, a five-cycle, an augmenting odd-cycle tail, the Petersen graph, multigraph/self-loop semantics, repeat-run determinism, and directed rejection.

## Complexity / claim audit

The implementation deliberately uses a dense endpoint-seen matrix, so the documented storage bound remains `O(V^2 + E)`. The code documents the conservative direct-implementation runtime bound `O(V^2 E)` (at most `O(V^4)` after simplification) instead of borrowing a stronger textbook implementation bound that this concrete organization has not established. No CI wall-clock number is presented as a benchmark or asymptotic result.

## Seal decision and next frontier

No correctness or integration blocker remains, and continuing Phase 10 with matching variants would now be breadth farming rather than closing the original structural gap. Phase 10 is therefore sealed.

A repository-wide coverage audit identifies randomized algorithms and explicit probabilistic contracts as a substantial missing concept. Phase 11 is promoted narrowly with one executable hypothesis: Karger-style randomized contraction for undirected global minimum cut. That slice must expose seed/trial semantics and a replayable cut witness, compare small instances with an exhaustive exact cut oracle, and distinguish "best cut observed" from any deterministic exactness claim. Further randomized algorithms are not precommitted.
