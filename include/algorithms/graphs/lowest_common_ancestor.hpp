#pragma once

#include <cstddef>
#include <optional>
#include <vector>

#include "algorithms/graphs/graph.hpp"

namespace algorithms::graphs {

class LowestCommonAncestor {
 public:
  LowestCommonAncestor(const Graph& tree, Vertex root);

  [[nodiscard]] std::size_t vertex_count() const noexcept;
  [[nodiscard]] Vertex root() const noexcept;
  [[nodiscard]] std::size_t depth(Vertex vertex) const;
  [[nodiscard]] Vertex lca(Vertex first, Vertex second) const;
  [[nodiscard]] std::size_t distance_edges(Vertex first, Vertex second) const;
  [[nodiscard]] std::optional<Vertex> kth_ancestor(Vertex vertex,
                                                    std::size_t steps) const;

 private:
  void validate_vertex(Vertex vertex) const;
  [[nodiscard]] Vertex lift(Vertex vertex, std::size_t steps) const;

  Vertex root_;
  std::vector<std::size_t> depth_;
  std::vector<std::vector<Vertex>> up_;
};

}  // namespace algorithms::graphs
