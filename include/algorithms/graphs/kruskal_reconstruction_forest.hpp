#pragma once

#include "algorithms/data_structures/disjoint_set_union.hpp"
#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace algorithms::graphs {

struct MinimaxConnectivityResult {
  bool connected{false};
  // Absent for disconnected pairs and the zero-edge path from a vertex to itself.
  // `connected` distinguishes those cases. For distinct connected vertices this
  // is the minimum threshold t such that edges with weight <= t connect them.
  std::optional<Weight> bottleneck;

  friend bool operator==(const MinimaxConnectivityResult&,
                         const MinimaxConnectivityResult&) = default;
};

// Immutable Kruskal reconstruction forest for an undirected weighted multigraph.
//
// Construction processes non-loop edge copies in nondecreasing (weight,u,v)
// order and creates one internal node for every successful DSU merge. Leaves
// 0..V-1 are the original graph vertices. Along every leaf-to-root route merge
// weights are nondecreasing. Equal-weight merges may refine one threshold level
// into several binary nodes, but all public threshold semantics are unchanged.
//
// Construction: O(E log E + V log V) direct work plus sealed DSU operations.
// Resident storage: O(V log V) for the reconstruction forest and jump table.
// Each public query below is O(log V).
class KruskalReconstructionForest {
 public:
  explicit KruskalReconstructionForest(const Graph& graph)
      : vertex_count_(graph.vertex_count()) {
    if (graph.directed()) {
      throw std::invalid_argument(
          "Kruskal reconstruction forest requires an undirected graph");
    }
    if (vertex_count_ >
        (std::numeric_limits<std::size_t>::max() - 1U) / 2U) {
      throw std::length_error(
          "Kruskal reconstruction forest node count overflows size_t");
    }

    std::vector<LogicalEdge> edges;
    std::size_t serial = 0;
    for (Vertex from = 0; from < vertex_count_; ++from) {
      for (const Edge& edge : graph.neighbors(from)) {
        if (from < edge.to) {
          edges.push_back(LogicalEdge{edge.weight, from, edge.to, serial});
          ++serial;
        }
      }
    }
    std::sort(edges.begin(), edges.end(),
              [](const LogicalEdge& lhs, const LogicalEdge& rhs) {
                return std::tie(lhs.weight, lhs.first, lhs.second, lhs.serial) <
                       std::tie(rhs.weight, rhs.first, rhs.second, rhs.serial);
              });

    const std::size_t reserve_nodes =
        vertex_count_ == 0U ? 0U : (2U * vertex_count_ - 1U);
    merge_weight_.reserve(reserve_nodes);
    component_size_.reserve(reserve_nodes);
    parent_.reserve(reserve_nodes);
    std::vector<std::pair<std::size_t, std::size_t>> children;
    children.reserve(reserve_nodes);
    for (std::size_t vertex = 0; vertex < vertex_count_; ++vertex) {
      merge_weight_.push_back(std::nullopt);
      component_size_.push_back(1U);
      parent_.push_back(kNoNode);
      children.emplace_back(kNoNode, kNoNode);
    }

    algorithms::data_structures::DisjointSetUnion dsu(vertex_count_);
    std::vector<std::size_t> component_node(vertex_count_);
    for (std::size_t vertex = 0; vertex < vertex_count_; ++vertex) {
      component_node[vertex] = vertex;
    }

    for (const LogicalEdge& edge : edges) {
      const std::size_t first_root = dsu.find(edge.first);
      const std::size_t second_root = dsu.find(edge.second);
      if (first_root == second_root) {
        continue;
      }

      const std::size_t first_node = component_node[first_root];
      const std::size_t second_node = component_node[second_root];
      const std::size_t merge_node = merge_weight_.size();
      merge_weight_.push_back(edge.weight);
      component_size_.push_back(component_size_[first_node] +
                                component_size_[second_node]);
      parent_.push_back(kNoNode);
      children.emplace_back(first_node, second_node);
      parent_[first_node] = merge_node;
      parent_[second_node] = merge_node;

      static_cast<void>(dsu.unite(first_root, second_root));
      component_node[dsu.find(first_root)] = merge_node;
    }

    const std::size_t node_count = merge_weight_.size();
    depth_.assign(node_count, 0U);
    root_.assign(node_count, kNoNode);
    std::vector<std::size_t> stack;
    stack.reserve(node_count);
    for (std::size_t node = 0; node < node_count; ++node) {
      if (parent_[node] != kNoNode) {
        continue;
      }
      root_[node] = node;
      stack.push_back(node);
      while (!stack.empty()) {
        const std::size_t current = stack.back();
        stack.pop_back();
        const auto [left, right] = children[current];
        for (const std::size_t child : {left, right}) {
          if (child == kNoNode) {
            continue;
          }
          depth_[child] = depth_[current] + 1U;
          root_[child] = node;
          stack.push_back(child);
        }
      }
    }

    const std::size_t levels =
        node_count == 0U
            ? 0U
            : static_cast<std::size_t>(std::bit_width(node_count));
    up_.assign(levels, std::vector<std::size_t>(node_count, kNoNode));
    if (!up_.empty()) {
      up_[0] = parent_;
      for (std::size_t level = 1; level < levels; ++level) {
        for (std::size_t node = 0; node < node_count; ++node) {
          const std::size_t half = up_[level - 1U][node];
          if (half != kNoNode) {
            up_[level][node] = up_[level - 1U][half];
          }
        }
      }
    }
  }

  [[nodiscard]] std::size_t vertex_count() const noexcept {
    return vertex_count_;
  }
  [[nodiscard]] std::size_t reconstruction_node_count() const noexcept {
    return merge_weight_.size();
  }

  [[nodiscard]] MinimaxConnectivityResult minimax_connectivity(
      Vertex first, Vertex second) const {
    validate_vertex(first);
    validate_vertex(second);
    if (first == second) {
      return MinimaxConnectivityResult{true, std::nullopt};
    }
    if (root_[first] != root_[second]) {
      return MinimaxConnectivityResult{false, std::nullopt};
    }
    const std::size_t ancestor = lca(first, second);
    if (ancestor == kNoNode || !merge_weight_[ancestor].has_value()) {
      throw std::logic_error(
          "Kruskal reconstruction forest has malformed connected witness");
    }
    return MinimaxConnectivityResult{true, merge_weight_[ancestor]};
  }

  [[nodiscard]] bool connected_at_most(Vertex first, Vertex second,
                                       Weight threshold) const {
    const MinimaxConnectivityResult result =
        minimax_connectivity(first, second);
    if (!result.connected) {
      return false;
    }
    return !result.bottleneck.has_value() || *result.bottleneck <= threshold;
  }

  [[nodiscard]] std::size_t component_size_at_most(Vertex vertex,
                                                   Weight threshold) const {
    validate_vertex(vertex);
    std::size_t node = vertex;
    for (std::size_t level = up_.size(); level-- > 0U;) {
      const std::size_t ancestor = up_[level][node];
      if (ancestor != kNoNode && merge_weight_[ancestor].has_value() &&
          *merge_weight_[ancestor] <= threshold) {
        node = ancestor;
      }
    }
    return component_size_[node];
  }

 private:
  struct LogicalEdge {
    Weight weight;
    Vertex first;
    Vertex second;
    std::size_t serial;
  };

  static constexpr std::size_t kNoNode = static_cast<std::size_t>(-1);

  void validate_vertex(Vertex vertex) const {
    if (vertex >= vertex_count_) {
      throw std::out_of_range("Kruskal reconstruction vertex out of range");
    }
  }

  [[nodiscard]] std::size_t lca(std::size_t first,
                                std::size_t second) const {
    if (root_[first] != root_[second]) {
      return kNoNode;
    }
    if (depth_[first] < depth_[second]) {
      std::swap(first, second);
    }
    std::size_t difference = depth_[first] - depth_[second];
    for (std::size_t level = 0; difference != 0U; ++level) {
      if ((difference & 1U) != 0U) {
        first = up_[level][first];
      }
      difference >>= 1U;
    }
    if (first == second) {
      return first;
    }
    for (std::size_t level = up_.size(); level-- > 0U;) {
      if (up_[level][first] != up_[level][second]) {
        first = up_[level][first];
        second = up_[level][second];
      }
    }
    return parent_[first];
  }

  std::size_t vertex_count_{0};
  std::vector<std::optional<Weight>> merge_weight_;
  std::vector<std::size_t> component_size_;
  std::vector<std::size_t> parent_;
  std::vector<std::size_t> depth_;
  std::vector<std::size_t> root_;
  std::vector<std::vector<std::size_t>> up_;
};

}  // namespace algorithms::graphs
