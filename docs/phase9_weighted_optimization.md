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

## Rectangular assignment via Hungarian augmentations

The second slice adds `hungarian_min_assignment(rows, columns, cost_matrix)` for a complete rectangular cost matrix with `rows <= columns`. Every row is assigned to one distinct column and the returned `column_for_row` witness is deterministic under ties. Empty row sets return an empty zero-cost assignment; non-empty requests require at least as many columns as rows and an exact row-major matrix shape.

The implementation uses the rectangular shortest-augmenting-path form of the Hungarian algorithm. Row and column potentials maintain non-negative reduced slacks on the current alternating-tree frontier. Each outer iteration augments the matching by one row; with `R` rows and `C` columns the implementation runs in `O(R^2 C)` time and uses `O(R + C)` auxiliary state beyond the input/result. Potential, slack, reduced-cost, and replayed total-cost arithmetic is checked `int64_t`; an unrepresentable intermediate fails closed with `std::overflow_error`.

Deterministic tests cover rectangular matrices, negative costs, equal-cost tie behavior, malformed shapes, infeasible `rows > columns`, and adversarial arithmetic. Five hundred fixed-seed random matrices use 1–4 rows, `rows`–5 columns, and costs in `[-20, 20]`. The primary oracle exhaustively enumerates every injective row-to-column assignment and compares the exact minimum cost. As cross-layer integration evidence, every same random matrix is independently reduced to the first Phase-9 min-cost-max-flow API and must produce the same optimum. The flow reduction is deliberately not the primary oracle.

## Lower-bounded / demand min-cost circulation

The third slice adds `min_cost_circulation(vertex_count, edges, demand)`. Every edge has integral lower/upper capacity bounds and signed cost; `demand[v]` is the required net inflow minus net outflow at vertex `v`. Feasibility is a first-class result: the API returns `std::nullopt` when no bounded flow can satisfy all vertex demands.

The solver first fixes every lower bound, adjusts the remaining per-vertex balance, and reduces feasibility to a super-source/super-sink network over residual `upper-lower` capacities. The repository's Phase-6 Dinic implementation must saturate the complete auxiliary demand before the original-edge flow is accepted. This is an actual cross-phase reuse rather than a second private max-flow implementation.

Unlike the first Phase-9 min-cost-max-flow slice, feasible circulation may legitimately benefit from negative-cost residual cycles. Therefore this implementation does **not** impose the earlier no-negative-cycle input restriction. Starting from the feasible bounded circulation, it repeatedly finds a negative residual cycle with all-vertex Bellman-Ford and augments the cycle by its bottleneck residual capacity. When no negative cycle remains, the final Bellman-Ford distances are returned as a feasible residual potential and every positive-residual arc is independently checked to have non-negative reduced cost.

The cycle-cancelling phase is intentionally documented as pseudo-polynomial rather than strongly polynomial. With `K` cycle cancellations its cost is `O(KVE)` after the Dinic feasibility transformation; each cancellation changes an integral edge flow by at least one unit. This bounded educational implementation favors an explicit optimality proof obligation over hiding a stronger complexity claim.

Deterministic tests cover lower bounds, non-zero demands, infeasibility, negative two-edge cycles, negative self-loops, empty instances, extreme fixed self-loop capacity, exact representability of total cost `INT64_MIN`, and overflow rejection. Three hundred fixed-seed random instances use 1–4 vertices and at most six edges with tiny integral bounds/costs. The primary oracle enumerates every bounded edge-flow assignment, filters by the exact demand equations, and selects the minimum total cost. The oracle does not use max flow, residual graphs, Bellman-Ford, or cycle cancellation.

## Sealed boundary and next frontier

Phase 9 is **SEALED**. All three ordered slices are merged and verified on exact `main`: minimum-cost maximum flow, rectangular Hungarian assignment, and lower-bounded / demand minimum-cost circulation.

The phase-level audit found no correctness, oracle-independence, or complexity-claim blocker. The differing negative-cycle contracts are intentional: the successive-shortest-path max-flow slice is restricted to a globally cycle-feasible input domain, while circulation explicitly optimizes general feasible bounded flows by cancelling negative residual cycles.

No further weighted-flow or assignment variants are implied by this seal. The next promoted frontier is Phase 10 general graph matching, beginning with maximum-cardinality matching in arbitrary undirected graphs via Edmonds blossom.
