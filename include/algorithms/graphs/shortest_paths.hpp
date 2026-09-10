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

struct AllPairsShortestPathResult {
  std::vector<std::vector<std::optional<Weight>>> distance;
  std::vector<std::vector<std::optional<Vertex>>> parent;
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

// Johnson all-pairs shortest paths. Negative edges are supported, but any
// negative cycle anywhere in the graph makes all-pairs shortest paths
// undefined and returns std::nullopt.
//
// A conceptual zero-weight super-source is used to compute Bellman-Ford
// potentials. Reweighted edges are represented exactly in uint64_t because a
// valid edge can be as large as UINT64_MAX even when every original Weight is
// int64_t. Each source is then solved by the Dijkstra settled-distance
// invariant over those non-negative reweighted edges.
//
// Throws overflow_error if a required Bellman-Ford potential or final finite
// shortest-path distance is not representable in Weight. Unreachable pairs are
// represented by std::nullopt in both distance and parent matrices.
[[nodiscard]] std::optional<AllPairsShortestPathResult>
johnson_all_pairs_shortest_paths(const Graph& graph);

}  // namespace algorithms::graphs
