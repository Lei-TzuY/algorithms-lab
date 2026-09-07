#pragma once

#include "algorithms/data_structures/segment_tree.hpp"
#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace algorithms::graphs {

// Heavy-light decomposition for vertex-weighted path/subtree sums on a strict
// undirected tree. Construction validates connectivity, acyclicity, and the
// absence of self-loops/parallel edges before exposing query state.
class HeavyLightDecomposition {
 public:
  HeavyLightDecomposition(const Graph& tree,
                          const std::vector<std::int64_t>& vertex_values,
                          Vertex root);

  [[nodiscard]] std::size_t vertex_count() const noexcept;
  [[nodiscard]] Vertex root() const noexcept;

  // Replaces one vertex value. O(log V), with SegmentTree transactional
  // overflow semantics.
  void assign(Vertex vertex, std::int64_t value);

  // Inclusive vertex sum along the unique path. O(log^2 V).
  [[nodiscard]] std::int64_t path_sum(Vertex first, Vertex second) const;

  // Sum of the rooted subtree, including vertex itself. O(log V).
  [[nodiscard]] std::int64_t subtree_sum(Vertex vertex) const;

 private:
  static std::int64_t checked_add(std::int64_t left, std::int64_t right);
  void validate_vertex(Vertex vertex) const;

  Vertex root_;
  std::vector<Vertex> parent_;
  std::vector<std::size_t> depth_;
  std::vector<std::size_t> subtree_size_;
  std::vector<Vertex> heavy_child_;
  std::vector<Vertex> head_;
  std::vector<std::size_t> position_;
  std::optional<algorithms::data_structures::SegmentTree> sums_;
};

}  // namespace algorithms::graphs
