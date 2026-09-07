#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

#include "algorithms/graphs/graph.hpp"

namespace algorithms::graphs {

struct KargerMinCutResult {
  std::size_t best_cut_size;
  std::vector<bool> side;
  std::uint64_t seed;
  std::size_t requested_trials;
  std::size_t trials_executed;
  std::optional<std::size_t> winning_trial;
};

// Randomized best-observed global edge cut for an undirected unweighted
// multigraph. Parallel edges retain multiplicity, self-loops and weights are
// ignored. A finite trial budget is not a deterministic exactness guarantee.
// Throws std::invalid_argument for directed input or zero requested trials.
[[nodiscard]] KargerMinCutResult karger_randomized_global_min_cut(
    const Graph& graph, std::uint64_t seed, std::size_t trials);

}  // namespace algorithms::graphs
