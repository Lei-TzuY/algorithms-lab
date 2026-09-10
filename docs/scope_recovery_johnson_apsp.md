# Johnson all-pairs shortest paths

This recovered slice adds exact all-pairs shortest paths for graphs that may
contain negative edges but contain no negative cycle. It deliberately closes a
long-standing gap between the sealed single-source Dijkstra/Bellman-Ford
foundations without turning the repository into a shortest-path variant catalog.

## Contract

`johnson_all_pairs_shortest_paths(graph)` returns an `n x n` distance matrix and
an `n x n` parent matrix. A finite entry is an exact `int64_t` shortest-path
weight; unreachable pairs remain `std::nullopt` in both matrices. The source
entry is zero and has no parent. Empty graphs return empty matrices.

Any negative cycle anywhere in the graph makes finite all-pairs shortest paths
undefined, so the function returns `std::nullopt`. This is deliberately global:
a negative cycle in a component unreachable from some particular source is still
fatal to the all-pairs result.

The parent matrix is a replayable shortest-path witness. Parents are selected by
strict relaxation in deterministic adjacency/heap order, but they are not
claimed to be a globally canonical lexicographic shortest-path tree.

## Bellman-Ford potential invariant

A conceptual super-source has a zero-weight edge to every graph vertex. Instead
of materializing that vertex, the implementation initializes every potential to
zero and relaxes every original edge. If the process still changes on the
`V`-th full pass, a negative cycle exists and the all-pairs operation returns
`std::nullopt`.

Otherwise the final potential `h` satisfies, for every edge `(u,v,w)`,

`h[v] <= h[u] + w`.

Therefore the reweighted edge

`w' = w + h[u] - h[v]`

is non-negative, and reweighting preserves relative path order between fixed
endpoints because every path receives the same telescoping endpoint offset.

Potentials start at zero and only decrease, so every representable `h[v]` is
non-positive. If a required potential would leave the signed `Weight` domain,
the implementation fails closed with `std::overflow_error` rather than silently
wrapping.

## Unsigned reweighted Dijkstra

A valid reweighted edge can be larger than `INT64_MAX`: for example the accepted
signed input/potential boundary can produce exactly `UINT64_MAX`. Consequently
the implementation does not call the existing signed-weight `dijkstra` routine
after reweighting. It runs the same settled-distance algorithm over `uint64_t`
using the repository's first-principles `BinaryHeap`.

Non-negative reweighted path sums are accumulated with checked `uint64_t`
arithmetic. Overflowing candidate paths are skipped. A separate graph
reachability pass distinguishes a truly unreachable target from a reachable
target for which no representable reweighted shortest distance exists; the
latter fails closed with `std::overflow_error`.

For a representable reweighted distance `d'`, the original distance is restored
as

`d(u,v) = d'(u,v) - h[u] + h[v]`

with sign-aware checked arithmetic. Exact `INT64_MIN` and `INT64_MAX` answers are
accepted; only genuinely unrepresentable final finite distances are rejected.

## Complexity

Let `V` be the number of vertices and `E` the number of stored directed adjacency
edges (so an undirected input contributes both stored directions except a
self-loop). Potential construction is `O(VE)`. Each of the `V` sources then runs
binary-heap Dijkstra in `O((V+E) log V)` plus `O(V+E)` reachability/witness work.
The distance/parent result itself occupies `O(V^2)` space; per-source auxiliary
state is `O(V+E)`.

This is a first-principles Johnson baseline. It does not claim Fibonacci-heap
bounds, path-counting semantics, or a canonical shortest-path witness.

## Verification

Deterministic tests cover the classical negative-edge Johnson example,
unreachable components, an empty graph, global/disconnected negative cycles,
negative self-loops, the undirected negative-edge two-cycle, exact `INT64_MIN`
and `INT64_MAX` answers, a reweighted edge of exactly `UINT64_MAX`, and positive
and negative final-distance overflow rejection.

The primary independent oracle is Floyd-Warshall over small representable test
domains:

- 700 fixed-seed directed DAGs with negative edges and parallel edges;
- 450 fixed-seed directed cyclic graphs generated from non-negative reduced
  costs and explicit potentials. Because every cycle's original weight equals
  the sum of its generated reduced costs, these graphs may contain negative
  edges but cannot contain a negative cycle.

For every finite pair, tests also replay the returned parent chain against the
original graph and require each parent edge to preserve the reported distance.

A separate 250-graph non-negative corpus includes directed/undirected graphs,
self-loops, parallel edges, and disconnected shapes. Every Johnson source row is
cross-checked against the already sealed signed Dijkstra implementation. This is
secondary cross-algorithm integration evidence; Floyd-Warshall remains the
primary APSP oracle.
