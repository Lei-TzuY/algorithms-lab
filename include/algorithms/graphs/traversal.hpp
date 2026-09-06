#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "algorithms/graphs/graph.hpp"

namespace algorithms::graphs {

struct BreadthFirstSearchResult {
  std::vector<Vertex> order;
  std::vector<std::optional<std::size_t>> distance;
  std::vector<std::optional<Vertex>> parent;
};

// BFS layer invariant: when vertex u is dequeued, distance[u] is the minimum
// number of edges from start to u; every newly discovered neighbor receives
// distance[u] + 1 and is discovered exactly once.
[[nodiscard]] BreadthFirstSearchResult breadth_first_search(const Graph& graph,
                                                            Vertex start);
[[nodiscard]] std::optional<std::vector<Vertex>> shortest_unweighted_path(
    const Graph& graph, Vertex start, Vertex target);

[[nodiscard]] std::vector<Vertex> depth_first_search(const Graph& graph,
                                                     Vertex start);
[[nodiscard]] std::vector<std::vector<Vertex>> connected_components(
    const Graph& graph);
[[nodiscard]] bool is_reachable(const Graph& graph, Vertex start,
                                Vertex target);
[[nodiscard]] bool has_cycle(const Graph& graph);

// Returns nullopt when a directed cycle prevents a topological ordering.
// Throws invalid_argument for undirected graphs.
[[nodiscard]] std::optional<std::vector<Vertex>> topological_sort(
    const Graph& graph);

}  // namespace algorithms::graphs
