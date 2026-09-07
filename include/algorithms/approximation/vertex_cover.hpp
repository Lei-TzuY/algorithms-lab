#pragma once

#include "algorithms/graphs/graph.hpp"

#include <utility>
#include <vector>

namespace algorithms::approximation {

struct VertexCoverApproximationResult {
  std::vector<graphs::Vertex> vertices;
  std::vector<graphs::Vertex> forced_self_loop_vertices;
  std::vector<std::pair<graphs::Vertex, graphs::Vertex>> maximal_matching_edges;
};

// Deterministic 2-approximation for minimum vertex cover on an undirected
// multigraph. Every self-loop vertex is mandatory and is included first;
// then both endpoints of a maximal matching in the residual uncovered graph
// are added. Edge weights are irrelevant.
[[nodiscard]] VertexCoverApproximationResult approximate_minimum_vertex_cover(
    const graphs::Graph& graph);

}  // namespace algorithms::approximation
