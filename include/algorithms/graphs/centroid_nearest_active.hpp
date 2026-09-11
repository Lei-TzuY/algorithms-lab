#pragma once

#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <optional>
#include <set>
#include <utility>
#include <vector>

namespace algorithms::graphs {

struct NearestActiveVertex {
  Vertex vertex;
  std::size_t distance;

  friend bool operator==(const NearestActiveVertex&,
                         const NearestActiveVertex&) = default;
};

// Immutable centroid-decomposition index over a strict undirected tree.
// Edge weights are ignored: distances are measured in number of edges.
class CentroidNearestActiveIndex {
 public:
  explicit CentroidNearestActiveIndex(const Graph& tree);

  [[nodiscard]] std::size_t vertex_count() const noexcept;
  [[nodiscard]] bool is_active(Vertex vertex) const;

  // Idempotent state changes. Returns true exactly when state changed.
  bool activate(Vertex vertex);
  bool deactivate(Vertex vertex);

  // Minimum edge distance; ties choose the smallest active vertex id.
  [[nodiscard]] std::optional<NearestActiveVertex> nearest_active(
      Vertex vertex) const;

  // Structural witness for the centroid tree. Exactly one non-empty-tree
  // vertex has no parent; every other entry names its centroid-tree parent.
  [[nodiscard]] const std::vector<std::optional<Vertex>>& centroid_parent()
      const noexcept;

 private:
  struct CentroidPathEntry {
    Vertex centroid;
    std::size_t distance;
  };

  std::size_t vertex_count_ = 0;
  std::vector<std::vector<Vertex>> adjacency_;
  std::vector<bool> removed_;
  std::vector<std::optional<Vertex>> centroid_parent_;
  std::vector<std::vector<CentroidPathEntry>> centroid_paths_;
  std::vector<bool> active_;
  std::vector<std::multiset<std::pair<std::size_t, Vertex>>> active_by_centroid_;
  std::vector<Vertex> scratch_parent_;
  std::vector<std::size_t> scratch_subtree_size_;

  void validate_vertex(Vertex vertex) const;
  void build_decomposition(Vertex start, std::optional<Vertex> parent);
  [[nodiscard]] std::vector<Vertex> collect_component(Vertex start) const;
  [[nodiscard]] Vertex choose_centroid(const std::vector<Vertex>& component,
                                       Vertex root);
  void append_centroid_distances(Vertex centroid);
};

}  // namespace algorithms::graphs
