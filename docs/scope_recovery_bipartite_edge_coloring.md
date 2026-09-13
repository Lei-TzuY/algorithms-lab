# Scope recovery: exact bipartite multigraph edge coloring

## Coverage decision

A fresh live-state audit at `main@fed06e0399ee00f8e96805400a6404dfc5863745`
after the deterministic weighted-spanner and exact tournament-feedback-arc
checkpoints found no bipartite edge-coloring / line-coloring implementation in
default-branch code, pull-request history, or the branch namespace.

This slice follows `docs/scope_recovery_after_phase69.md`: the historical
Phase-45--69 compiler/backend sequence remains frozen, the stale historical
`ROADMAP.md` frontier is not normalized, and prospective work is selected from
fresh algorithm/data-structure coverage gaps.

The recovered proof model is Kőnig's line-coloring theorem for bipartite
multigraphs rather than another ordering, cut, or approximation variant.

## Production contract

`minimum_bipartite_edge_coloring(const Graph&)` accepts the repository's
undirected multigraph abstraction and returns:

- the exact minimum color count `Delta`, equal to the maximum logical degree;
- one deterministic bipartition witness for every input vertex;
- one colored witness for every logical undirected edge in deterministic
  `(first vertex, first-seen adjacency copy)` enumeration order;
- each witness preserves the original edge weight as provenance even though
  weights are intentionally irrelevant to cardinality edge coloring.

Parallel edges are distinct logical edges and therefore consume distinct colors
at their common endpoints. Self-loops are rejected because no proper edge color
can satisfy a loop; directed and non-bipartite inputs are rejected explicitly.
Isolated vertices remain valid.

## Construction and invariants

1. Breadth-first search constructs a deterministic bipartition and rejects an
   odd-cycle conflict or self-loop.
2. Original logical edges are mapped to left/right indices while preserving
   stable original edge ids.
3. Let `Delta` be the maximum original degree. The two sides are padded with
   dummy vertices to equal cardinality `N`.
4. Each vertex receives exactly enough dummy parallel edges to raise its degree
   to `Delta`. Because both sides then have `N` vertices and the same original
   edge count, their total deficits are equal.
5. The resulting balanced multigraph is `Delta`-regular. In each color round,
   sealed Hopcroft--Karp is run on the active support graph. Hall's condition
   follows from regularity: for every left subset `S`, its `Delta|S|` incident
   edge copies must enter `N(S)`, whose vertices can absorb at most
   `Delta|N(S)|`; hence `|S| <= |N(S)|` and a perfect matching exists.
6. Removing one perfect matching decreases every remaining vertex degree by
   exactly one. Repeating for colors `0..Delta-1` decomposes the regularized
   multigraph into `Delta` perfect matchings. Original edge copies consume the
   color of the matched bucket copy; dummy copies are discarded.

Every two edges sharing an endpoint therefore receive different colors. The
maximum-degree lower bound requires at least `Delta` colors at a maximum-degree
vertex, so the returned coloring is minimum.

## Verification

Deterministic regressions cover:

- empty/isolated graphs and a single edge;
- multiple parallel edges with different ignored weights;
- disconnected components and `K_{3,3}`;
- repeated execution for deterministic witnesses;
- directed input, self-loops, and an odd cycle as rejected domains.

A fixed-seed 700-instance corpus generates small bipartite multigraphs with at
most eight logical edges. Every returned witness is replayed for endpoint/color
conflicts, bipartition validity, stable edge identity, and preserved weights.
An independent backtracking edge-coloring oracle then proves that `Delta-1`
colors are impossible while `Delta` colors are feasible for every non-empty
instance. The oracle does not use the production regularization or matching
decomposition.

Focused GCC and Clang strict-warning builds plus GCC ASan+UBSan all pass the
recovery-specific suite before remote promotion; repository-wide CI remains the
integration gate.

## Complexity boundary

Let `V` and `E` denote original vertices/logical edges, and let `Delta` be the
maximum degree. Regularization creates `O(V+E)` distinct endpoint buckets even
when a bucket contains many parallel copies. Each of `Delta` rounds rebuilds the
active support and runs Hopcroft--Karp on `O(V)` padded vertices and `O(V+E)`
support edges. A conservative bound for the direct implementation is therefore

`O(Delta * (V + E) * sqrt(V + 1) + (V + E) log(V + E))`

time and `O(V+E)` auxiliary/result storage, excluding the already-stored input
adjacency. No optimal edge-coloring claim is made outside bipartite multigraphs.
