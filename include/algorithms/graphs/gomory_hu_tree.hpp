#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
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

struct WeightedGlobalMinCutResult {
  std::uint64_t weight;
  std::vector<bool> side;

  friend bool operator==(const WeightedGlobalMinCutResult&,
                         const WeightedGlobalMinCutResult&) = default;
};

// Computes an exact global minimum cut by the direct Stoer-Wagner
// maximum-adjacency/contraction algorithm rather than repeated max-flow.
//
// For fewer than two vertices there is no non-trivial bipartition and
// std::nullopt is returned. Otherwise side is non-trivial and normalized so
// side[0] == false. Parallel capacities add; self-loops are cut-neutral.
// Negative capacities and invalid endpoints are rejected.
//
// Internal arithmetic uses uint64_t after checked-summing all non-loop input
// capacities. This supports an exact cut larger than signed Capacity when the
// aggregate still fits uint64_t; aggregate overflow is rejected.
//
// Complexity: O(V^3 + E) time and O(V^2 + E) auxiliary storage.
[[nodiscard]] std::optional<WeightedGlobalMinCutResult>
stoer_wagner_global_min_cut(
    std::size_t vertex_count,
    std::span<const UndirectedCapacityEdge> edges);

}  // namespace algorithms::graphs
