#pragma once

#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/max_flow.hpp"

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace algorithms::graphs {

using FlowCost = std::int64_t;

struct CostCapacityEdge {
  Vertex from;
  Vertex to;
  Capacity capacity;
  FlowCost cost;

  friend bool operator==(const CostCapacityEdge&,
                         const CostCapacityEdge&) = default;
};

struct MinCostFlowEdge {
  Vertex from;
  Vertex to;
  Capacity capacity;
  FlowCost cost;
  Capacity flow;

  friend bool operator==(const MinCostFlowEdge&,
                         const MinCostFlowEdge&) = default;
};

struct MinCostMaxFlowResult {
  Capacity value;
  FlowCost cost;
  Capacity cut_capacity;
  std::vector<MinCostFlowEdge> edges;
  std::vector<bool> source_side_min_cut;
  std::vector<FlowCost> residual_potential;
};

// Computes a minimum-cost maximum s-t flow using successive shortest
// augmenting paths with feasible vertex potentials and reduced-cost Dijkstra.
//
// Preconditions:
// - source and sink are distinct vertices in [0, vertex_count)
// - edge endpoints are in range and capacities are non-negative
// - edge cost INT64_MIN is rejected because its residual reverse cost is not
//   representable
// - the positive-capacity input network has no negative-cost directed cycle;
//   this is validated globally before augmentation
//
// Self-loops with non-negative cost are connectivity-neutral and receive zero
// flow. Parallel and antiparallel edges are supported. Throws overflow_error if
// required flow/cost/potential arithmetic is not representable as int64_t.
[[nodiscard]] MinCostMaxFlowResult min_cost_max_flow(
    std::size_t vertex_count, std::span<const CostCapacityEdge> edges,
    Vertex source, Vertex sink);

}  // namespace algorithms::graphs
