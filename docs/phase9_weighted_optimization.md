# Phase 9 — weighted combinatorial optimization

Phase 9 begins by adding an explicit cost dimension to the Phase-6 flow substrate. The first slice is minimum-cost maximum flow: maximize the integral `s-t` flow value first, then minimize total signed edge cost among flows with that maximum value.

## Network and supported-domain contract

`min_cost_max_flow(vertex_count, edges, source, sink)` accepts directed edges with non-negative `int64_t` capacities and signed `int64_t` costs. Parallel and antiparallel edges are independent. Non-negative-cost self-loops are canonicalized to zero flow because they cannot improve either objective. `INT64_MIN` edge cost is rejected because the residual reverse cost would not be representable.

This slice deliberately requires the positive-capacity input residual network to contain no negative-cost directed cycle. Validation is global, not only source-reachable: an all-vertex Bellman-Ford relaxation is equivalent to adding a zero-cost super-source, so a disconnected negative cycle is rejected too. This is a supported-domain restriction for the successive-shortest-path implementation, not a claim that capacitated min-cost flow with negative cycles is mathematically undefined. Zero-capacity edges do not participate in the residual network and therefore do not form a residual negative cycle.

## Successive shortest augmenting paths

The solver stores an explicit residual pair for every non-self-loop input edge. Initial all-vertex Bellman-Ford distances form feasible vertex potentials `p` satisfying

`c(u,v) + p(u) - p(v) >= 0`

for every positive-residual arc. Each augmentation then runs Dijkstra on these non-negative reduced costs using the Phase-1 first-principles `BinaryHeap`, updates potentials on the source-reachable set, traces the predecessor chain, and sends the bottleneck residual capacity. Reverse residual arcs can therefore reroute earlier choices rather than freezing the first shortest path.

If `A` is the number of augmentations, runtime is `O(VE + A E log V)` under the binary-heap implementation, including Bellman-Ford potential construction/final certification up to constant factors. Residual storage is `O(V + E)`.

## Maximum-flow and minimum-cost certificates

Termination occurs only when the residual network has no `s-t` path. The implementation then exposes the residual source-reachable set and replays the input-order edge flows. It checks:

- every edge flow lies in `[0, capacity]`;
- flow conservation holds at every non-terminal vertex;
- source/sink balances equal the reported maximum-flow value;
- the original-capacity cut induced by residual reachability equals the reported flow value;
- replaying `sum(flow * cost)` equals the reported total cost.

The max-flow/min-cut equality certifies maximality. For cost optimality at that fixed flow value, a fresh all-vertex Bellman-Ford pass on the **final** residual network constructs a global feasible potential. Every positive-residual arc is checked to have non-negative reduced cost. Therefore the final residual network contains no negative-cost cycle, the standard cycle-optimality certificate for minimum cost at the fixed flow value.

## Checked arithmetic

Flow values, costs, potentials, balances, cut capacities, and replayed costs use checked `int64_t` arithmetic. Edge cost `INT64_MIN` is rejected before residual construction, while a total cost exactly equal to `INT64_MIN` remains accepted when it is representable (for example two units at cost `INT64_MIN / 2`). Unrepresentable path, potential, flow, or total-cost arithmetic throws `std::overflow_error` instead of wrapping.

## Independent verification

Deterministic tests cover signed-cost parallel paths, residual rerouting through a reverse arc, global disconnected negative-cycle rejection, zero-capacity negative-cycle neutrality, `INT64_MIN` edge-cost rejection, exact representability of total cost `INT64_MIN`, and overflowing total cost.

Three hundred fixed-seed random DAG networks use 2–5 vertices, at most seven edges (parallel edges allowed), capacities 0–2, and costs in `[-5, 8]`. The production result is compared against an independent exhaustive oracle that enumerates every edge-flow assignment, filters by capacity/conservation, maximizes source-sink flow, and then minimizes cost among assignments at that maximum value. The oracle does not use residual graphs, shortest paths, potentials, or the production solver.

Test-side witness replay also independently checks the returned flow/cut/cost identities and evaluates the returned residual-potential inequalities for every forward or reverse residual direction implied by the reported edge flows.

## Frontier

This completes the first ordered Phase-9 slice. Phase 9 remains active. The next roadmap frontier is the assignment problem via Hungarian algorithm, with a min-cost-flow reduction used as cross-implementation integration evidence rather than as the primary oracle.
