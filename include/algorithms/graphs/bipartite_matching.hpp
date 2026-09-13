#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

namespace algorithms::graphs {

struct BipartiteEdge {
  std::size_t left;
  std::size_t right;

  friend bool operator==(const BipartiteEdge&, const BipartiteEdge&) = default;
};

struct BipartiteMatchingResult {
  std::size_t cardinality;
  std::vector<std::optional<std::size_t>> left_match;
  std::vector<std::optional<std::size_t>> right_match;
  std::vector<bool> left_in_min_vertex_cover;
  std::vector<bool> right_in_min_vertex_cover;
};

// Maximum-cardinality matching for a bipartite graph with left vertices
// [0,left_count) and right vertices [0,right_count). Parallel edges are
// accepted and preserve deterministic first-seen adjacency order.
[[nodiscard]] BipartiteMatchingResult hopcroft_karp(
    std::size_t left_count, std::size_t right_count,
    std::span<const BipartiteEdge> edges);

struct ColoredUndirectedEdge {
  std::size_t edge_id;
  Vertex first;
  Vertex second;
  Weight weight;
  std::size_t color;

  friend bool operator==(const ColoredUndirectedEdge&,
                         const ColoredUndirectedEdge&) = default;
};

struct BipartiteEdgeColoringResult {
  std::size_t color_count;
  std::vector<bool> left_partition;
  std::vector<ColoredUndirectedEdge> edges;

  friend bool operator==(const BipartiteEdgeColoringResult&,
                         const BipartiteEdgeColoringResult&) = default;
};

// Exact minimum edge coloring for an undirected bipartite multigraph.
//
// The returned color count is the maximum degree Delta. Parallel edges are
// distinct logical edges and therefore receive distinct colors when incident
// to the same endpoints. Edge weights are preserved as witness provenance but
// do not affect the cardinality-coloring problem. Self-loops, directed graphs,
// and non-bipartite inputs are rejected.
[[nodiscard]] BipartiteEdgeColoringResult minimum_bipartite_edge_coloring(
    const Graph& graph);

}  // namespace algorithms::graphs
