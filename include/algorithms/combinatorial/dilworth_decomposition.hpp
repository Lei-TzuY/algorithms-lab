#pragma once

#include <cstddef>
#include <vector>

#include "algorithms/graphs/graph.hpp"

namespace algorithms::combinatorial {

struct DilworthDecompositionResult {
  std::size_t width = 0;
  std::size_t matching_cardinality = 0;
  std::vector<std::vector<std::size_t>> minimum_chain_decomposition;
  std::vector<std::size_t> maximum_antichain;
};

// Interprets reachability in a directed acyclic graph as a strict partial order.
// Edge weights are ignored and parallel arcs do not change comparability.
// Returns a minimum chain decomposition and a maximum antichain whose common
// cardinality is the poset width. Throws invalid_argument for undirected or
// cyclic inputs.
[[nodiscard]] DilworthDecompositionResult dilworth_decomposition(
    const graphs::Graph& dag);

}  // namespace algorithms::combinatorial
