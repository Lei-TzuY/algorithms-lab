#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "algorithms/graphs/max_flow.hpp"

namespace algorithms::graphs {

struct UndirectedCapacityEdge {
  Vertex first;
  Vertex second;
  Capacity capacity;

  friend bool operator==(const UndirectedCapacityEdge&,
                         const UndirectedCapacityEdge&) = default;
};

class GomoryHuTree {
 public:
  [[nodiscard]] std::size_t vertex_count() const noexcept;

  // Rooted at vertex 0 when non-empty. parent()[0] == 0 and
  // cut_to_parent()[0] == 0. For every v > 0, (v, parent()[v]) is a tree
  // edge whose weight is cut_to_parent()[v].
  [[nodiscard]] const std::vector<Vertex>& parent() const noexcept;
  [[nodiscard]] const std::vector<Capacity>& cut_to_parent() const noexcept;

  // Returns the exact minimum cut value between two distinct vertices.
  // Baseline query cost is O(V) by walking the cut-equivalent tree.
  [[nodiscard]] Capacity min_cut(Vertex first, Vertex second) const;

 private:
  friend GomoryHuTree build_gomory_hu_tree(
      std::size_t, std::span<const UndirectedCapacityEdge>);

  GomoryHuTree(std::vector<Vertex> parent,
               std::vector<Capacity> cut_to_parent);

  std::vector<Vertex> parent_;
  std::vector<Capacity> cut_to_parent_;
};

// Builds a Gomory-Hu cut-equivalent tree for an undirected capacitated
// multigraph using V-1 calls to the sealed Dinic implementation.
//
// Preconditions:
// - all endpoints are in [0, vertex_count)
// - capacities are non-negative
//
// Parallel edges and zero-capacity edges are supported. Self-loops are
// accepted and are cut-neutral. Throws std::overflow_error if a required
// s-t cut value is not representable as Capacity.
[[nodiscard]] GomoryHuTree build_gomory_hu_tree(
    std::size_t vertex_count,
    std::span<const UndirectedCapacityEdge> edges);

}  // namespace algorithms::graphs
