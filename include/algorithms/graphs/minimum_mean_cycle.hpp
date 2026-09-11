#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace algorithms::graphs {

struct MeanCycleEdge {
  Vertex from{};
  std::size_t adjacency_index{};
  Vertex to{};
  Weight weight{};

  friend bool operator==(const MeanCycleEdge&, const MeanCycleEdge&) = default;
};

struct MinimumMeanCycleResult {
  // Reduced exact rational mean_numerator / mean_denominator.
  std::int64_t mean_numerator{};
  std::uint64_t mean_denominator{1};

  // Exact witness cycle. Each edge identifies one concrete adjacency entry, so
  // parallel edges remain distinguishable and replayable.
  std::int64_t cycle_weight{};
  std::vector<MeanCycleEdge> cycle;

  // Karp DP state that attained the selected min-max ratio. These fields are
  // diagnostics, not additional correctness assumptions for the witness.
  Vertex karp_terminal_vertex{};
  std::size_t karp_reference_length{};

  friend bool operator==(const MinimumMeanCycleResult&,
                         const MinimumMeanCycleResult&) = default;
};

// Returns the exact minimum-mean directed cycle, or nullopt when no directed
// cycle exists. The graph must be directed. Signed weights, self-loops, and
// parallel edges are supported.
//
// This implementation deliberately fails closed with overflow_error when an
// intermediate exact Karp/shifted-cost computation is not int64-representable.
[[nodiscard]] std::optional<MinimumMeanCycleResult> minimum_mean_cycle(
    const Graph& graph);

}  // namespace algorithms::graphs
