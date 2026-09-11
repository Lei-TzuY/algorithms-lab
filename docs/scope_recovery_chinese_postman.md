# Scope recovery: exact undirected Chinese postman

## Coverage decision

The post-Phase-69 recovery requires a fresh algorithm/data-structure proof model,
not continuation of the frozen compiler/backend surface and not repetition of an
already-sealed algorithm. Live code and PR-history searches found no Chinese
postman / route-inspection solver. The closest existing capability is the sealed
Hierholzer Eulerian-trail slice in PR #166; that PR explicitly states that it is
not route optimization or a Chinese-postman solver.

This slice therefore adds exact minimum-cost edge-covering closed walks for
connected undirected weighted multigraphs. It deliberately follows the exact
Steiner-tree checkpoint with a different proof obligation: parity correction via
minimum perfect matching in a shortest-path metric, followed by Eulerization.

## Production contract

`minimum_chinese_postman_tour(const Graph&)` accepts the existing undirected
`Graph` abstraction with globally non-negative signed-64 edge weights.

- directed input is rejected;
- any negative edge is rejected globally;
- parallel edges remain distinct logical edge copies;
- self-loops are supported and contribute degree two;
- isolated vertices do not affect active-edge connectivity;
- multiple active-edge components return `std::nullopt`;
- empty graphs return an empty closed walk; non-empty edgeless graphs return
  the canonical `{0}` walk;
- if the exact optimum exceeds `INT64_MAX`, `std::overflow_error` is thrown;
- the result exposes original-edge weight, duplicated augmentation weight,
  exact total weight, a closed vertex walk, replayable original adjacency-edge
  references for every traversal, and the odd-vertex shortest-path pairings.

The original adjacency references are canonical representatives of logical
undirected edge copies. They keep parallel copies distinguishable even when
endpoints and weights agree.

## Algorithm and invariants

Let `O` be the set of odd-degree active vertices. The implementation first
normalizes the repository's symmetric undirected adjacency representation into
logical edges, checks active-edge connectivity, and computes the original edge
weight exactly up to the public signed-64 boundary.

For every vertex in `O`, a first-principles binary-heap Dijkstra computes the
non-negative shortest-path metric plus reconstructable original edge parents.
The matching stage is an exact subset dynamic program. For a mask of still
unpaired odd vertices, its lowest set bit is paired with each possible remaining
vertex; the recurrence chooses the minimum shortest-path cost plus the already
solved remainder. Saturating `INT64_MAX + 1` internal costs ensure overflowing
non-optimal pairings cannot poison a representable optimum.

The chosen metric pairs are expanded back into original shortest-path edge
copies. Duplicating those paths makes every active degree even. A deterministic
first-principles Hierholzer pass over the augmented edge occurrences then emits
a closed walk. Production replays the occurrence costs and rejects any internal
mismatch between the DP optimum and the returned walk.

## Correctness boundary

Every feasible closed postman walk traverses each original edge at least once.
Remove one mandatory copy of every original edge. The parity of the remaining
multiset is odd exactly at the original odd-degree vertices, so its walk
components induce pairings of those odd vertices. Replacing each induced path by
a shortest path cannot increase cost. Therefore every feasible tour pays at
least the minimum perfect-matching cost in the odd-vertex shortest-path metric.

Conversely, duplicating shortest paths for any perfect matching of the odd
vertices makes all active degrees even without disconnecting the graph. An
Eulerian circuit of this augmentation is therefore a feasible postman tour.
Choosing the minimum metric matching attains the lower bound, proving optimality.

Tests are implementation evidence for these obligations; they are not presented
as a substitute for the theorem above.

## Independent verification

Deterministic regressions cover empty/edgeless input, one-edge duplication,
already-Eulerian graphs, four-odd-vertex pairing, parallel edges, self-loops,
insertion orientation, repeated deterministic execution, disconnected active
components, directed and negative-weight rejection, exact `INT64_MAX`
representability, and both original-sum and augmentation overflow.

The primary randomized oracle does not use odd-vertex pairing, Dijkstra metric
closure, or Hierholzer feasibility. For 500 fixed-seed undirected multigraphs
with 0-6 vertices and at most 8 logical edge copies, it solves a different exact
state-space problem: Dijkstra over `(covered_edge_mask, current_vertex)`, where
any incident original edge may be traversed repeatedly and a bit records whether
that logical edge has ever been covered. The optimum is the cheapest state that
covers all edges and returns to the canonical active start vertex.

Every production result is additionally replayed against the original `Graph`:
walk endpoints must agree with each edge reference, every logical edge must occur
at least once, augmentation paths must connect their paired odd endpoints with
the advertised cost, and original plus duplicated weight must equal the closed
walk total.

Focused pre-upload builds passed under GCC C++20 strict warnings-as-errors,
Clang C++20 strict warnings-as-errors, and actual GCC ASan+UBSan.

## Complexity and non-claims

Let `k = |O|`. The direct educational baseline performs `k` Dijkstra runs and an
exact pairing DP with `2^k` states, giving
`O(k (E+V) log V + 2^k k + E + L)` time, where `L` is the returned augmented
walk length. Storage is `O(kV + 2^k + E + L)`.

This is an exact exponential-in-odd-vertex-count route-inspection baseline. It is
not a polynomial weighted-matching implementation, directed Chinese postman,
rural postman, mixed postman, negative-edge solver, arbitrary-precision solver,
or practical large-`k` performance claim.
