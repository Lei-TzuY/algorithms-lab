#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <vector>

namespace algorithms::graphs {

struct MaximumCliqueResult {
  std::vector<Vertex> vertices;
  std::size_t recursive_calls = 0;
  std::size_t maximal_cliques_examined = 0;
};

// Returns the lexicographically smallest sorted maximum-cardinality clique.
// The graph must be undirected. Self-loops are ignored and parallel edges
// collapse to one adjacency relation for clique semantics.
[[nodiscard]] MaximumCliqueResult maximum_clique_bron_kerbosch(const Graph& graph);

}  // namespace algorithms::graphs
