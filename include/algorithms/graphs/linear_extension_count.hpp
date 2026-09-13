#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

inline constexpr std::size_t kMaxExactLinearExtensionVertices = 20;

struct LinearExtensionCountResult {
  std::uint64_t count{};
  std::size_t vertex_count{};
  std::uint64_t ideals_evaluated{};
  std::vector<Vertex> lexicographically_smallest_order;

  friend bool operator==(const LinearExtensionCountResult&,
                         const LinearExtensionCountResult&) = default;
};

// Count the linear extensions of the partial order induced by a directed DAG.
// Parallel arcs collapse to one precedence constraint and edge weights are
// intentionally ignored. Cyclic input is rejected because it does not define a
// partial order. The explicit n <= 20 bound keeps the exact count within
// uint64_t because every DAG has at most n! linear extensions and 20! fits.
[[nodiscard]] inline LinearExtensionCountResult count_dag_linear_extensions(
    const Graph& graph) {
  if (!graph.directed()) {
    throw std::invalid_argument("linear-extension counting requires directed input");
  }

  const std::size_t n = graph.vertex_count();
  if (n > kMaxExactLinearExtensionVertices) {
    throw std::length_error("linear-extension vertex count exceeds exact bound");
  }

  std::vector<std::uint64_t> predecessors(n, 0);
  for (Vertex from = 0; from < n; ++from) {
    const std::uint64_t from_bit = std::uint64_t{1} << from;
    for (const Edge& edge : graph.neighbors(from)) {
      predecessors[edge.to] |= from_bit;
    }
  }

  std::vector<Vertex> canonical_order;
  canonical_order.reserve(n);
  std::uint64_t placed = 0;
  for (std::size_t position = 0; position < n; ++position) {
    bool found = false;
    for (Vertex vertex = 0; vertex < n; ++vertex) {
      const std::uint64_t bit = std::uint64_t{1} << vertex;
      if ((placed & bit) != 0) {
        continue;
      }
      if ((predecessors[vertex] & ~placed) == 0) {
        canonical_order.push_back(vertex);
        placed |= bit;
        found = true;
        break;
      }
    }
    if (!found) {
      throw std::invalid_argument("linear-extension counting requires an acyclic graph");
    }
  }

  const std::size_t state_count = std::size_t{1} << n;
  std::vector<std::uint64_t> dp(state_count, 0);
  dp[0] = 1;
  std::uint64_t ideals_evaluated = 0;

  for (std::size_t mask = 0; mask < state_count; ++mask) {
    const std::uint64_t ways = dp[mask];
    if (ways == 0) {
      continue;
    }
    ++ideals_evaluated;
    const std::uint64_t placed_mask = static_cast<std::uint64_t>(mask);
    for (Vertex vertex = 0; vertex < n; ++vertex) {
      const std::uint64_t bit = std::uint64_t{1} << vertex;
      if ((placed_mask & bit) != 0) {
        continue;
      }
      if ((predecessors[vertex] & ~placed_mask) != 0) {
        continue;
      }
      const std::size_t next = mask | static_cast<std::size_t>(bit);
      if (std::numeric_limits<std::uint64_t>::max() - dp[next] < ways) {
        throw std::overflow_error("linear-extension count overflow");
      }
      dp[next] += ways;
    }
  }

  return LinearExtensionCountResult{dp.back(), n, ideals_evaluated,
                                    std::move(canonical_order)};
}

}  // namespace algorithms::graphs
