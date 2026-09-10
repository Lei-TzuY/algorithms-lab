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

struct UndirectedEdgeWitness {
  Vertex first;
  Vertex second;
  Weight weight;

  friend bool operator==(const UndirectedEdgeWitness&,
                         const UndirectedEdgeWitness&) = default;
};

struct UndirectedLowLinkResult {
  std::vector<UndirectedEdgeWitness> bridges;
  std::vector<Vertex> articulation_vertices;
  std::vector<std::size_t> bridge_component_of;
  std::size_t bridge_component_count = 0;
};

// Tarjan low-link analysis for the repository's undirected multigraph model.
// Connectivity ignores edge weights but bridge witnesses retain them. Parallel
// edge copies are distinguished internally; self-loops can never be bridges.
// Bridge-component ids are assigned deterministically by increasing seed vertex
// after every bridge edge is removed. Throws invalid_argument for directed input.
[[nodiscard]] UndirectedLowLinkResult analyze_undirected_low_link(
    const Graph& graph);

// Returns nullopt when a directed cycle prevents a topological ordering.
// Throws invalid_argument for undirected graphs.
[[nodiscard]] std::optional<std::vector<Vertex>> topological_sort(
    const Graph& graph);

}  // namespace algorithms::graphs
