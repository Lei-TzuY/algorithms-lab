#pragma once

#include "algorithms/graphs/graph.hpp"

#include <optional>
#include <vector>

namespace algorithms::graphs {

struct TreeIsomorphismResult {
  std::vector<Vertex> first_to_second;
  std::vector<Vertex> second_to_first;
  std::vector<Vertex> first_centers;
  std::vector<Vertex> second_centers;
};

// Exact isomorphism of strict undirected trees. Edge weights are intentionally
// ignored. Empty trees are accepted and are isomorphic only to other empty
// trees. Directed graphs, self-loops, parallel edges, cycles, and disconnected
// non-empty inputs are rejected with std::invalid_argument.
[[nodiscard]] std::optional<TreeIsomorphismResult> exact_tree_isomorphism(
    const Graph& first, const Graph& second);

}  // namespace algorithms::graphs
