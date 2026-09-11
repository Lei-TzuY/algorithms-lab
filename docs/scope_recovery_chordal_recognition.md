# Scope recovery — exact chordal-graph recognition via MCS

## Coverage decision

A fresh live-code, PR-history, and branch audit after exact planarity reached merged-main green found no chordal-graph, perfect-elimination-ordering, or Maximum Cardinality Search capability. The recovery authority freezes the Phase-45–69 compiler/backend surface. `scope-recovery-minimum-cycle-basis`, general graph isomorphism, and other recovery branches already exist, so this slice deliberately avoids those occupied surfaces.

This slice changes proof model again: instead of another cycle basis, embedding enumerator, or backend variant, it adds graph-structure recognition through an elimination ordering theorem.

## Production contract

`recognize_chordal_graph(const Graph&)` accepts the repository's undirected multigraph abstraction and interprets chordality on the underlying simple graph:

- directed inputs are rejected;
- self-loops are ignored;
- parallel edge copies collapse to one adjacency relation;
- edge weights are ignored;
- MCS selection is deterministic: maximum current cardinality score, then smallest vertex id;
- `candidate_elimination_order` is the reverse MCS order;
- chordal results return an exact perfect elimination ordering and exact maximum-clique size;
- non-chordal results return the first replayable PEO violation triple `(v, a, b)` where `a` and `b` are later neighbors of `v` but are not adjacent.

The violation triple certifies that the returned MCS-derived candidate is not a PEO. It is intentionally **not** advertised as an induced-cycle certificate.

## Correctness / proof boundary

Maximum Cardinality Search supplies the candidate ordering. The classical MCS characterization states that an undirected graph is chordal exactly when the reverse of an MCS ordering is a perfect elimination ordering.

A perfect elimination ordering requires every vertex's later neighbors to form a clique. Production checks that obligation directly. If every check succeeds, each vertex together with its later neighbors forms a clique, and the largest such set has size `omega(G)` for a chordal graph. The public `maximum_clique_size` therefore follows from the PEO structure rather than by calling the repository's exponential clique solver.

The MCS characterization and PEO theorem are the proof obligations. Tests provide implementation evidence rather than replacing those theorems.

## Verification

Focused exact candidate passed before upload under:

- GCC C++20 strict warnings-as-errors;
- Clang C++20 strict warnings-as-errors;
- actual GCC ASan+UBSan with leak detection.

Deterministic coverage includes empty input, trees, complete graphs, chordless 4- and 5-cycles, a chorded 4-cycle, disconnected graphs, directed rejection, repeated execution, self-loops, parallel copies, signed weights, and multigraph/weight invariance.

Primary randomized oracle is structurally independent of MCS and PEO verification. For 700 fixed-seed undirected multigraphs with 0–10 vertices, the test enumerates every vertex subset and rejects the graph exactly when the induced subgraph is a connected 2-regular graph on at least four vertices—that is, a chordless cycle. This uses the forbidden-induced-cycle characterization of chordal graphs rather than reconstructing production's ordering logic.

Every production result is replayed. Chordal candidates are independently checked as PEOs. Non-chordal violation triples are checked against the simplified adjacency relation and ordering positions. As secondary cross-integration only, every randomized chordal result compares its PEO-derived clique number against the sealed exact Bron–Kerbosch maximum-clique implementation.

## Complexity / non-claims

This educational baseline materializes a `V x V` adjacency matrix, performs a direct `O(V^2)` MCS selection pass, and validates later-neighbor cliques by pairwise checks. A conservative bound for the exact implementation is therefore:

- time: `O(V^3 + E)`;
- auxiliary/result storage: `O(V^2)`.

No linear-time chordal-recognition claim, minimal triangulation, chordal completion, clique tree / junction tree construction, interval-graph recognition, induced-cycle witness extraction, or dynamic chordality claim is made.
