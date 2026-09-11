#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace algorithms::graphs {

struct SteinerTreeEdge {
  Vertex from = 0;
  std::size_t adjacency_index = 0;
  Vertex to = 0;
  Weight weight = 0;

  friend bool operator==(const SteinerTreeEdge&, const SteinerTreeEdge&) = default;
};

struct SteinerTreeResult {
  Weight total_weight = 0;
  std::vector<Vertex> terminals;
  std::vector<SteinerTreeEdge> edges;

  friend bool operator==(const SteinerTreeResult&, const SteinerTreeResult&) = default;
};

// Exact Dreyfus-Wagner terminal-subset dynamic programming baseline.
//
// Preconditions:
// - graph is undirected;
// - every edge weight is non-negative;
// - every terminal is a valid graph vertex.
//
// Duplicate terminals are canonicalized away. Empty and singleton terminal sets
// have zero cost. If the terminals cannot be connected, returns std::nullopt.
// If the exact optimum exceeds signed 64-bit Weight, throws std::overflow_error.
[[nodiscard]] std::optional<SteinerTreeResult> minimum_steiner_tree(
    const Graph& graph, std::vector<Vertex> terminals);

}  // namespace algorithms::graphs
