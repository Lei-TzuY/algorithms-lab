# Phase 2 — greedy reasoning and deeper graph structure

Phase 2 promotes the lab from basic traversal/shortest paths into two kinds of reasoning that recur throughout algorithms: exchange/cut arguments for greedy choices and structural decomposition of directed graphs.

## Interval scheduling

**Problem.** Given half-open intervals `[start, finish)`, choose a maximum-cardinality compatible subset. Inputs must satisfy `start <= finish`; touching endpoints are compatible.

**Greedy choice.** Process intervals by nondecreasing finish time and accept the next interval whose start is at least the last selected finish.

**Invariant / exchange argument.** After each selection, the greedy schedule finishes no later than any schedule containing the same number of intervals from the processed prefix. If an optimal schedule starts with a later-finishing interval, replacing it with the greedy earliest-finishing interval preserves feasibility and cannot reduce the number of later choices.

**Complexity.** Sorting dominates: `O(n log n)` time and `O(n)` auxiliary storage.

**Evidence.** Deterministic boundary/invalid-input cases plus fixed-seed random instances are checked against exhaustive subset enumeration for small inputs.

## Minimum spanning forest

Both implementations require an undirected graph. Disconnected inputs intentionally return a minimum spanning **forest**. Parallel edges and negative weights are valid; self-loops are never useful. Total-weight accumulation is checked for signed overflow.

### Kruskal

Edges are considered in nondecreasing weight order. DSU tracks current forest components.

**Invariant.** Chosen edges are acyclic and are extendable to a minimum spanning forest. When the next edge joins two DSU components, it is a lightest edge crossing the cut induced by those components and is safe by the cut property.

**Complexity.** `O(E log E)` time and `O(V + E)` storage.

### Prim

Each connected component is grown from an arbitrary unvisited root. The implementation deliberately reuses the lab's first-principles binary heap rather than `std::priority_queue`.

**Invariant.** The visited set is connected by the selected tree edges. The next accepted frontier edge is the lightest crossing edge from visited to unvisited vertices, so the cut property makes it safe. Stale heap entries whose endpoint is already visited are discarded.

**Complexity.** With edge candidates and no decrease-key, `O(E log E)` time and `O(V + E)` storage.

**Differential evidence.** Fixed-seed randomized undirected multigraphs include disconnected components, negative/zero weights, self-loops, and parallel edges. Kruskal and Prim must agree on total weight and component count, and each returned edge set is independently checked to be an acyclic forest of `V - components` edges.

## Strongly connected components

SCC APIs require directed graphs.

### Tarjan

`index[v]` records DFS discovery order. `lowlink[v]` records the smallest discovery index reachable while remaining within the active DFS stack.

**Root condition.** `lowlink[v] == index[v]` means `v` roots one maximal SCC; popping until `v` extracts exactly that component.

**Complexity.** `O(V + E)` time and `O(V)` auxiliary storage, excluding recursion stack.

### Kosaraju

The first DFS records finish order. The second DFS runs in reverse finish order on the transposed graph.

**Proof obligation.** The SCC condensation is a DAG. Reverse finish order selects a source SCC in the remaining transposed condensation, so the second-pass DFS cannot escape into another unassigned SCC and discovers exactly one component.

**Complexity.** `O(V + E)` time and `O(V + E)` auxiliary storage including the transpose.

### Condensation DAG

`condensation_graph` contracts each SCC to one vertex and removes duplicate inter-component edges. Weights are intentionally discarded: this abstraction represents reachability structure, not weighted-path semantics. Deterministic duplicate removal costs `O(E log E)`.

**Integration evidence.** Every randomized condensation is fed into the existing Phase-1 topological sort and must be accepted as a DAG. Tarjan and Kosaraju partitions are compared pairwise, then checked against the independent definition `u` and `v` are in the same SCC iff each is reachable from the other.

## Known limits

- Tarjan and Kosaraju currently use recursive DFS; extremely deep graphs can exhaust the process stack. A later infrastructure hardening pass may replace recursion with explicit frames without changing the algorithms' semantics.
- Component IDs are algorithm-assigned and are not a canonical labeling. Tests compare partitions rather than numeric IDs.
- The MST API returns one minimum forest; equal-weight graphs can have multiple equally valid answers.
