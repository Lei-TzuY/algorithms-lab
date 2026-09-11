#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "algorithms/graphs/graph.hpp"

namespace algorithms::graphs {

struct PostmanEdgeReference {
  Vertex from = 0;
  std::size_t adjacency_index = 0;
  Vertex to = 0;
  Weight weight = 0;

  friend bool operator==(const PostmanEdgeReference&,
                         const PostmanEdgeReference&) = default;
};

struct PostmanAugmentation {
  Vertex first = 0;
  Vertex second = 0;
  Weight distance = 0;
  std::vector<PostmanEdgeReference> path;

  friend bool operator==(const PostmanAugmentation&,
                         const PostmanAugmentation&) = default;
};

struct PostmanTraversalStep {
  PostmanEdgeReference edge;
  bool duplicated = false;

  friend bool operator==(const PostmanTraversalStep&,
                         const PostmanTraversalStep&) = default;
};

struct ChinesePostmanResult {
  Weight total_weight = 0;
  Weight original_edge_weight = 0;
  Weight duplicated_edge_weight = 0;
  std::vector<Vertex> vertices;
  std::vector<PostmanTraversalStep> edge_walk;
  std::vector<PostmanAugmentation> augmentations;

  friend bool operator==(const ChinesePostmanResult&,
                         const ChinesePostmanResult&) = default;
};

// Exact minimum-cost closed walk covering every logical edge at least once in
// an undirected non-negative weighted multigraph. Isolated vertices are ignored
// for connectivity. Returns nullopt when the active-edge subgraph is
// disconnected. Throws for directed input, negative weights, unrepresentable
// subset state, or an optimum whose total weight exceeds int64_t.
[[nodiscard]] std::optional<ChinesePostmanResult> minimum_chinese_postman_tour(
    const Graph& graph);

}  // namespace algorithms::graphs
