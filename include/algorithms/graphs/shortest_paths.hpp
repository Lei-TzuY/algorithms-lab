#pragma once

#include <optional>
#include <vector>

#include "algorithms/graphs/graph.hpp"

namespace algorithms::graphs {

struct ShortestPathResult {
  std::vector<std::optional<Weight>> distance;
  std::vector<std::optional<Vertex>> parent;
};

struct BellmanFordResult : ShortestPathResult {
  bool has_reachable_negative_cycle = false;
};

// Preconditions: all graph edge weights must be non-negative. The function
// validates this globally and throws invalid_argument if any negative edge is
// present, reachable or not.
//
// Settled-distance invariant: when u is removed as the current minimum with a
// non-stale distance, no future relaxation can produce a shorter path to u.
[[nodiscard]] ShortestPathResult dijkstra(const Graph& graph, Vertex source);

// Supports negative edges and detects negative cycles reachable from source.
[[nodiscard]] BellmanFordResult bellman_ford(const Graph& graph, Vertex source);

}  // namespace algorithms::graphs
