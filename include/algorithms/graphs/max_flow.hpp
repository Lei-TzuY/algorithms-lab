#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "algorithms/graphs/graph.hpp"

namespace algorithms::graphs {

using Capacity = std::int64_t;

struct CapacityEdge {
  Vertex from;
  Vertex to;
  Capacity capacity;

  friend bool operator==(const CapacityEdge&, const CapacityEdge&) = default;
};

struct FlowEdge {
  Vertex from;
  Vertex to;
  Capacity capacity;
  Capacity flow;

  friend bool operator==(const FlowEdge&, const FlowEdge&) = default;
};

struct MaxFlowResult {
  Capacity value;
  Capacity cut_capacity;
  std::vector<FlowEdge> edges;
  std::vector<bool> source_side_min_cut;
};

// Computes a maximum s-t flow with Dinic's algorithm.
//
// Preconditions:
// - source and sink are distinct vertices in [0, vertex_count)
// - every edge endpoint is in range
// - capacities are non-negative and individually representable as Capacity
//
// Self-loops, parallel edges, antiparallel edges, and zero-capacity edges are
// supported. Result edges preserve input order. Throws std::overflow_error if
// the maximum-flow value is not representable as Capacity.
[[nodiscard]] MaxFlowResult dinic_max_flow(
    std::size_t vertex_count, std::span<const CapacityEdge> edges,
    Vertex source, Vertex sink);

}  // namespace algorithms::graphs
