#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "algorithms/graphs/graph.hpp"

namespace algorithms::graphs {

struct GeneralMatchingResult {
  std::size_t cardinality;
  std::vector<std::optional<Vertex>> mate;
};

// Maximum-cardinality matching in an arbitrary undirected graph via Edmonds'
// blossom algorithm. Edge weights are ignored. Self-loops are ignored and
// parallel edges are collapsed by endpoint pair for matching purposes.
// Throws std::invalid_argument for directed input.
[[nodiscard]] GeneralMatchingResult edmonds_blossom_maximum_matching(
    const Graph& graph);

}  // namespace algorithms::graphs
