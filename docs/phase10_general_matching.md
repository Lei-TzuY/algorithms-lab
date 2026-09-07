# Phase 10 — general graph matching

Phase 10 promotes the matching surface beyond bipartite graphs. The first executable slice is maximum-cardinality matching in an arbitrary undirected graph through Edmonds' blossom contraction algorithm. This is a structural extension of the sealed Phase-6 Hopcroft-Karp capability: odd cycles are now supported rather than excluded by a bipartition assumption.

## Contract

`edmonds_blossom_maximum_matching(const Graph&)` accepts an undirected `Graph` and returns:

- the maximum matching cardinality;
- a symmetric `mate[v]` witness for every matched vertex.

Edge weights are irrelevant to the cardinality objective. Self-loops are ignored because a vertex cannot match itself. Parallel edges are collapsed to one endpoint pair during preprocessing because multiplicity does not change feasibility for ordinary matching. Directed input is rejected with `std::invalid_argument` rather than silently reinterpreted.

The implementation is deterministic for a fixed graph insertion order: roots are explored in increasing vertex order and adjacency retains first-seen endpoint order. The returned maximum matching is not claimed to be a canonical matching among all optima.

## Alternating forest and blossom invariant

For one unmatched root, the search maintains an alternating forest:

- `parent[v]` is the predecessor that first exposed `v` in the alternating search;
- `base[v]` names the current contracted blossom representative;
- queued/`used` vertices are outer vertices eligible to scan unmatched edges;
- `match[v]` is either the symmetric mate of `v` or the unmatched sentinel.

When an examined edge joins two outer vertices in the same alternating search tree, it closes an odd alternating cycle. `lowest_common_base` finds the common blossom base, both paths to that base are marked, and every vertex in the marked blossom is contracted to the same `base`. Search then continues from the contracted super-vertex without discarding augmenting paths that enter or leave the odd cycle.

When an exposed unmatched terminal is reached, `augment` flips matched/unmatched status along the predecessor chain. Each successful augmentation therefore increases cardinality by exactly one.

Correctness relies on two mathematical facts rather than on testing alone: Berge's augmenting-path criterion for maximum matching, and Edmonds' odd-blossom contraction preserving the existence of augmenting paths. The randomized suite supplies executable evidence for this implementation; it is not presented as a proof of those theorems.

## Complexity boundary

The current educational implementation deliberately uses a dense `V x V` seen matrix to collapse parallel endpoint pairs before search. It therefore uses `O(V^2 + E)` storage rather than claiming sparse-optimal memory.

For runtime, this repository makes the conservative direct-code bound `O(V^2 E)` for the repeated augmenting searches, including linear-time blossom-base/path work triggered while scanning edges. Since the preprocessed simple graph has `E = O(V^2)`, this is at most `O(V^4)`. No stronger textbook bound is claimed until the implementation is reorganized to justify it directly.

## Verification

Deterministic adversarial coverage includes:

- triangle and 5-cycle odd blossoms;
- an odd blossom connected to an augmenting tail;
- the Petersen graph with a perfect matching;
- self-loop and parallel-edge semantics;
- directed-input rejection;
- repeat execution on the same graph to check deterministic witness construction.

The primary randomized oracle is structurally independent. One thousand fixed-seed undirected multigraphs with 0–9 vertices are projected to endpoint adjacency, then an exhaustive recursive oracle enumerates every legal matching choice and returns the exact maximum cardinality. The production witness is replayed independently for symmetric mates, endpoint existence, distinctness, and reported cardinality.

As cross-phase integration evidence, five hundred fixed-seed bipartite graphs are represented simultaneously as a general `Graph` and as Phase-6 `BipartiteEdge` input. Edmonds blossom and the sealed Hopcroft-Karp implementation must return identical maximum cardinality. Hopcroft-Karp is not the primary oracle; exhaustive enumeration remains independent of both production algorithms.

## Frontier

This slice is the only committed Phase-10 hypothesis. After exact PR CI and merged-main CI, Phase 10 should receive an architecture/sealing audit before any weighted-blossom or related matching variant is considered.
