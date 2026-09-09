#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace algorithms::graphs {

using ArborescenceVertex = std::size_t;
using ArborescenceCost = std::int64_t;

struct DirectedArborescenceEdge {
  ArborescenceVertex from;
  ArborescenceVertex to;
  ArborescenceCost cost;
};

struct MinimumArborescenceResult {
  ArborescenceCost total_cost;
  std::vector<std::size_t> edge_indices;
  std::vector<std::optional<std::size_t>> incoming_edge_index;
};

// Exact minimum-cost spanning arborescence rooted at `root`.
//
// Preconditions / domain:
// - vertex_count > 0 and root < vertex_count
// - every endpoint is in [0, vertex_count)
// - every vertex is reachable from root using directed non-self-loop edges
//
// Parallel edges and signed costs are supported. Self-loops are accepted but
// cannot belong to an arborescence and are ignored by optimization. Ties are
// resolved by the smallest original input edge index.
//
// Throws std::out_of_range for an invalid root/endpoint,
// std::invalid_argument when root cannot reach every vertex, and
// std::overflow_error only when the exact total cost of the chosen optimum is
// not representable as ArborescenceCost.
[[nodiscard]] MinimumArborescenceResult chu_liu_edmonds_minimum_arborescence(
    std::size_t vertex_count,
    std::span<const DirectedArborescenceEdge> edges,
    ArborescenceVertex root);

}  // namespace algorithms::graphs
