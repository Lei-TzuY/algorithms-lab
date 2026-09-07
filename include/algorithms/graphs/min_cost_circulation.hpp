#pragma once

#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/max_flow.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace algorithms::graphs {

using CirculationCost = std::int64_t;

struct LowerBoundCostEdge {
  Vertex from;
  Vertex to;
  Capacity lower;
  Capacity upper;
  CirculationCost cost;
  friend bool operator==(const LowerBoundCostEdge&, const LowerBoundCostEdge&) = default;
};

struct MinCostCirculationEdge {
  Vertex from;
  Vertex to;
  Capacity lower;
  Capacity upper;
  CirculationCost cost;
  Capacity flow;
  friend bool operator==(const MinCostCirculationEdge&, const MinCostCirculationEdge&) = default;
};

struct MinCostCirculationResult {
  CirculationCost cost;
  std::vector<MinCostCirculationEdge> edges;
  std::vector<CirculationCost> residual_potential;
};

// Finds a minimum-cost feasible circulation with lower/upper edge bounds and
// per-vertex demand. demand[v] means required net inflow minus net outflow.
// Returns nullopt when the balance/bound constraints are infeasible.
//
// Preconditions:
// - demand.size() == vertex_count and sum(demand) == 0
// - endpoints are in range and 0 <= lower <= upper
// - edge cost INT64_MIN is rejected because reverse residual cost is not
//   representable
//
// Feasibility is reduced to Dinic on residual upper-lower capacities. Cost is
// then optimized by cancelling negative-cost residual cycles until none remain.
// With K cycle cancellations, the optimization phase is O(KVE); K is
// pseudo-polynomial for integral capacities rather than strongly polynomial.
[[nodiscard]] std::optional<MinCostCirculationResult> min_cost_circulation(
    std::size_t vertex_count, std::span<const LowerBoundCostEdge> edges,
    std::span<const Capacity> demand);

}  // namespace algorithms::graphs
