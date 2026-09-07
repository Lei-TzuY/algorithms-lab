#pragma once

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

}  // namespace algorithms::graphs
