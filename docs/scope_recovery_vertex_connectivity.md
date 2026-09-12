# Scope recovery — exact vertex connectivity and minimum separators

## Coverage decision

After Reed-Solomon Berlekamp-Welch decoding reached merged `main`, a fresh live
code, pull-request, branch, and recovery-document audit found no exact global
vertex-connectivity, Menger vertex-separator, separation-pair, triconnected, or
SPQR capability. The sealed vertex-biconnected block-cut implementation
explicitly lists triconnected/SPQR decomposition as outside its contract.

This slice fills the exact vertex-connectivity / separator boundary without
claiming a full SPQR-tree decomposition. It deliberately changes proof model
again after coding theory: undirected structural connectivity is reduced to
vertex-capacitated flow and independently checked by exhaustive vertex removal.

The frozen Phase-45–69 compiler/backend surface remains untouched under
`docs/scope_recovery_after_phase69.md`.

## Production contract

`exact_vertex_connectivity(graph)` accepts the repository's undirected
multigraph `Graph` and returns:

- exact global vertex connectivity `kappa(G)`;
- a minimum vertex separator witness;
- for connected non-complete graphs, a non-adjacent vertex pair separated by
  that witness;
- explicit `input_connected` and `complete_graph` diagnostics.

Structural semantics are intentional:

- directed input is rejected;
- self-loops are ignored;
- parallel edge copies collapse to one adjacency relation;
- edge weights are ignored;
- the empty graph and singleton have connectivity zero;
- a disconnected graph has connectivity zero and returns the empty separator
  plus a replayable already-disconnected pair;
- a complete graph `K_n`, `n >= 2`, has connectivity `n-1` and returns a
  canonical separator leaving one vertex.

## Menger / vertex-splitting obligation

For a connected non-complete graph, production enumerates non-adjacent pairs
`(s,t)`. Every vertex is split into `v_in -> v_out`; ordinary vertices receive
capacity one, while `s` and `t` receive an effectively infinite capacity
`n+1`. Every structural undirected edge becomes two directed infinite-capacity
arcs between split endpoints.

Because `s` and `t` are non-adjacent, removing all other vertices is a finite
separator of size at most `n-2`; therefore a minimum split-network cut never
needs an infinite-capacity structural or terminal arc. The cut capacity is
exactly the number of ordinary vertex arcs crossed, and those crossed arcs
reconstruct an explicit `s-t` vertex separator.

By the vertex form of Menger's theorem, this local minimum equals the maximum
number of internally vertex-disjoint `s-t` paths. For a connected non-complete
undirected graph, global vertex connectivity is the minimum local connectivity
over non-adjacent pairs. Complete graphs are handled by the standard `n-1`
convention separately.

Production reuses the sealed Dinic max-flow implementation rather than adding a
second residual-network engine. Every returned non-complete separator is replayed
against the original structural graph before the API returns.

## Verification

Focused final candidate passes:

- GCC C++20 strict warnings-as-errors;
- Clang C++20 strict warnings-as-errors;
- actual GCC ASan+UBSan.

Deterministic evidence covers empty/singleton graphs, paths, complete graphs,
disconnected graphs, directed rejection, self-loops, parallel copies, ignored
signed weights, witness replay, and repeatability.

The primary randomized oracle is structurally independent of Menger/max-flow.
For 900 fixed-seed undirected multigraphs with `0..8` vertices, it enumerates
vertex-removal subsets in increasing cardinality and performs ordinary BFS on the
surviving graph. The first subset that disconnects the graph (or leaves at most
one vertex, for the complete-graph convention) gives the exact optimum. Production
must match that optimum exactly, while its returned witness is independently
replayed.

## Complexity / non-claims

The direct educational baseline materializes a structural `V x V` adjacency
matrix and solves a vertex-split max-flow instance for at most `O(V^2)`
non-adjacent pairs. Using the repository's generic Dinic bound on each split
network yields a deliberately loose conservative polynomial bound of
`O(V^4 (V+E))` time and `O(V^2+E)` transient/storage scale for this implementation.
No optimized all-pairs vertex-cut algorithm is claimed.

This slice does **not** claim SPQR-tree construction, triconnected-component
enumeration, dynamic vertex connectivity, weighted/multiplicity vertex cuts,
edge-connectivity replacement, process-scale graph scalability, or preservation
of loop/parallel multiplicity in the structural connectivity relation.
