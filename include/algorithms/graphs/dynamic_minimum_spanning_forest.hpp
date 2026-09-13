#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/graphs/graph.hpp"

namespace algorithms::graphs {

class DynamicMinimumSpanningForest {
  static_assert(std::numeric_limits<std::size_t>::digits <= 64,
                "dynamic MSF exact accumulator assumes at most 64-bit size_t");

 public:
  using EdgeHandle = std::size_t;

  struct EdgeState {
    EdgeHandle handle;
    Vertex first;
    Vertex second;
    Weight weight;
    bool active;
    bool in_forest;

    friend bool operator==(const EdgeState&, const EdgeState&) = default;
  };

  explicit DynamicMinimumSpanningForest(std::size_t vertex_count)
      : vertex_count_(vertex_count) {}

  [[nodiscard]] std::size_t vertex_count() const noexcept { return vertex_count_; }
  [[nodiscard]] std::size_t edge_count() const noexcept { return edges_.size(); }
  [[nodiscard]] std::size_t active_edge_count() const noexcept {
    return active_edge_count_;
  }
  [[nodiscard]] std::size_t forest_edge_count() const noexcept {
    return forest_edge_count_;
  }
  [[nodiscard]] std::size_t component_count() const noexcept {
    return vertex_count_ - forest_edge_count_;
  }

  [[nodiscard]] EdgeHandle add_edge(Vertex first, Vertex second, Weight weight) {
    validate_vertex(first);
    validate_vertex(second);
    if (edges_.size() == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("dynamic MSF edge-handle space exhausted");
    }

    const EdgeHandle handle = edges_.size();
    edges_.push_back(StoredEdge{first, second, weight, true, false});
    ++active_edge_count_;

    if (first == second) {
      return handle;
    }

    const auto path = forest_path(first, second);
    if (!path.has_value()) {
      edges_[handle].in_forest = true;
      ++forest_edge_count_;
      return handle;
    }

    EdgeHandle heaviest = path->front();
    for (const EdgeHandle candidate : *path) {
      if (edge_key_less(heaviest, candidate)) {
        heaviest = candidate;
      }
    }
    if (weight < edges_[heaviest].weight) {
      edges_[heaviest].in_forest = false;
      edges_[handle].in_forest = true;
    }
    return handle;
  }

  void remove_edge(EdgeHandle handle) {
    StoredEdge& removed = edge_mut(handle);
    if (!removed.active) {
      throw std::invalid_argument("dynamic MSF edge handle is inactive");
    }

    const bool was_tree_edge = removed.in_forest;
    removed.active = false;
    removed.in_forest = false;
    --active_edge_count_;
    if (!was_tree_edge) {
      return;
    }

    --forest_edge_count_;
    const auto side = forest_component_mask(removed.first);

    std::optional<EdgeHandle> replacement;
    for (EdgeHandle candidate = 0; candidate < edges_.size(); ++candidate) {
      const StoredEdge& edge = edges_[candidate];
      if (!edge.active || edge.in_forest || edge.first == edge.second) {
        continue;
      }
      if (side[edge.first] == side[edge.second]) {
        continue;
      }
      if (!replacement.has_value() || edge_key_less(candidate, *replacement)) {
        replacement = candidate;
      }
    }
    if (replacement.has_value()) {
      edges_[*replacement].in_forest = true;
      ++forest_edge_count_;
    }
  }

  [[nodiscard]] bool connected(Vertex first, Vertex second) const {
    validate_vertex(first);
    validate_vertex(second);
    if (first == second) {
      return true;
    }
    return forest_path(first, second).has_value();
  }

  [[nodiscard]] bool active(EdgeHandle handle) const { return edge(handle).active; }
  [[nodiscard]] bool in_forest(EdgeHandle handle) const {
    return edge(handle).in_forest;
  }

  [[nodiscard]] EdgeState edge_state(EdgeHandle handle) const {
    const StoredEdge& edge_value = edge(handle);
    return EdgeState{handle, edge_value.first, edge_value.second, edge_value.weight,
                     edge_value.active, edge_value.in_forest};
  }

  [[nodiscard]] std::vector<EdgeState> forest_edges() const {
    std::vector<EdgeState> result;
    result.reserve(forest_edge_count_);
    for (EdgeHandle handle = 0; handle < edges_.size(); ++handle) {
      if (edges_[handle].active && edges_[handle].in_forest) {
        result.push_back(edge_state(handle));
      }
    }
    return result;
  }

  [[nodiscard]] Weight total_weight() const {
    WideSigned total;
    for (const StoredEdge& edge_value : edges_) {
      if (edge_value.active && edge_value.in_forest) {
        total.add(edge_value.weight);
      }
    }
    return total.to_int64();
  }

  // Expensive diagnostic: replays forest acyclicity/component equivalence and
  // the MST cycle optimality condition for every active non-tree edge.
  [[nodiscard]] bool valid_structure() const {
    if (forest_edge_count_ > vertex_count_) {
      return false;
    }
    std::size_t counted_active = 0;
    std::size_t counted_forest = 0;
    Disjoint replay(vertex_count_);
    for (EdgeHandle handle = 0; handle < edges_.size(); ++handle) {
      const StoredEdge& edge_value = edges_[handle];
      if (edge_value.active) {
        ++counted_active;
      }
      if (!edge_value.in_forest) {
        continue;
      }
      if (!edge_value.active || edge_value.first == edge_value.second) {
        return false;
      }
      ++counted_forest;
      if (!replay.unite(edge_value.first, edge_value.second)) {
        return false;
      }
    }
    if (counted_active != active_edge_count_ || counted_forest != forest_edge_count_) {
      return false;
    }

    for (const StoredEdge& edge_value : edges_) {
      if (!edge_value.active || edge_value.first == edge_value.second) {
        continue;
      }
      if (replay.find(edge_value.first) != replay.find(edge_value.second)) {
        return false;
      }
    }

    for (EdgeHandle handle = 0; handle < edges_.size(); ++handle) {
      const StoredEdge& edge_value = edges_[handle];
      if (!edge_value.active || edge_value.in_forest ||
          edge_value.first == edge_value.second) {
        continue;
      }
      const auto path = forest_path(edge_value.first, edge_value.second);
      if (!path.has_value() || path->empty()) {
        return false;
      }
      Weight maximum = edges_[path->front()].weight;
      for (const EdgeHandle tree_handle : *path) {
        maximum = std::max(maximum, edges_[tree_handle].weight);
      }
      if (maximum > edge_value.weight) {
        return false;
      }
    }
    return true;
  }

 private:
  struct StoredEdge {
    Vertex first;
    Vertex second;
    Weight weight;
    bool active;
    bool in_forest;
  };

  class Disjoint {
   public:
    explicit Disjoint(std::size_t count) : parent_(count), size_(count, 1) {
      for (std::size_t index = 0; index < count; ++index) {
        parent_[index] = index;
      }
    }
    std::size_t find(std::size_t value) {
      while (parent_[value] != value) {
        value = parent_[value];
      }
      return value;
    }
    bool unite(std::size_t first, std::size_t second) {
      first = find(first);
      second = find(second);
      if (first == second) {
        return false;
      }
      if (size_[first] < size_[second]) {
        std::swap(first, second);
      }
      parent_[second] = first;
      size_[first] += size_[second];
      return true;
    }
   private:
    std::vector<std::size_t> parent_;
    std::vector<std::size_t> size_;
  };

  struct WideSigned {
    bool negative{false};
    std::uint64_t high{0};
    std::uint64_t low{0};

    static std::pair<std::uint64_t, std::uint64_t> magnitude(Weight value) {
      if (value >= 0) {
        return {0, static_cast<std::uint64_t>(value)};
      }
      const std::uint64_t mag =
          static_cast<std::uint64_t>(-(value + 1)) + std::uint64_t{1};
      return {0, mag};
    }

    static int compare(std::uint64_t left_high, std::uint64_t left_low,
                       std::uint64_t right_high, std::uint64_t right_low) {
      if (left_high != right_high) {
        return left_high < right_high ? -1 : 1;
      }
      if (left_low != right_low) {
        return left_low < right_low ? -1 : 1;
      }
      return 0;
    }

    void add(Weight value) {
      if (value == 0) {
        return;
      }
      const bool value_negative = value < 0;
      const auto [value_high, value_low] = magnitude(value);
      if (high == 0 && low == 0) {
        negative = value_negative;
        high = value_high;
        low = value_low;
        return;
      }
      if (negative == value_negative) {
        const std::uint64_t old_low = low;
        low += value_low;
        high += value_high + (low < old_low ? 1U : 0U);
        return;
      }
      const int order = compare(high, low, value_high, value_low);
      if (order == 0) {
        negative = false;
        high = 0;
        low = 0;
        return;
      }
      if (order > 0) {
        const std::uint64_t old_low = low;
        low -= value_low;
        high -= value_high + (old_low < value_low ? 1U : 0U);
        return;
      }
      const std::uint64_t old_low = value_low;
      const std::uint64_t new_low = value_low - low;
      const std::uint64_t borrow = old_low < low ? 1U : 0U;
      high = value_high - high - borrow;
      low = new_low;
      negative = value_negative;
    }

    Weight to_int64() const {
      if (high != 0) {
        throw std::overflow_error("dynamic MSF total weight is outside int64");
      }
      const std::uint64_t negative_limit = std::uint64_t{1} << 63U;
      if (!negative) {
        if (low > static_cast<std::uint64_t>(std::numeric_limits<Weight>::max())) {
          throw std::overflow_error("dynamic MSF total weight is outside int64");
        }
        return static_cast<Weight>(low);
      }
      if (low > negative_limit) {
        throw std::overflow_error("dynamic MSF total weight is outside int64");
      }
      if (low == negative_limit) {
        return std::numeric_limits<Weight>::min();
      }
      return -static_cast<Weight>(low);
    }
  };

  [[nodiscard]] const StoredEdge& edge(EdgeHandle handle) const {
    if (handle >= edges_.size()) {
      throw std::out_of_range("dynamic MSF edge handle out of range");
    }
    return edges_[handle];
  }
  [[nodiscard]] StoredEdge& edge_mut(EdgeHandle handle) {
    if (handle >= edges_.size()) {
      throw std::out_of_range("dynamic MSF edge handle out of range");
    }
    return edges_[handle];
  }
  void validate_vertex(Vertex vertex) const {
    if (vertex >= vertex_count_) {
      throw std::out_of_range("dynamic MSF vertex out of range");
    }
  }
  [[nodiscard]] bool edge_key_less(EdgeHandle left, EdgeHandle right) const {
    if (edges_[left].weight != edges_[right].weight) {
      return edges_[left].weight < edges_[right].weight;
    }
    return left < right;
  }

  [[nodiscard]] std::vector<std::vector<std::pair<Vertex, EdgeHandle>>>
  forest_adjacency() const {
    std::vector<std::vector<std::pair<Vertex, EdgeHandle>>> adjacency(vertex_count_);
    for (EdgeHandle handle = 0; handle < edges_.size(); ++handle) {
      const StoredEdge& edge_value = edges_[handle];
      if (!edge_value.active || !edge_value.in_forest) {
        continue;
      }
      adjacency[edge_value.first].push_back({edge_value.second, handle});
      adjacency[edge_value.second].push_back({edge_value.first, handle});
    }
    return adjacency;
  }

  [[nodiscard]] std::optional<std::vector<EdgeHandle>> forest_path(
      Vertex source, Vertex target) const {
    if (source == target) {
      return std::vector<EdgeHandle>{};
    }
    const auto adjacency = forest_adjacency();
    const EdgeHandle no_edge = std::numeric_limits<EdgeHandle>::max();
    const Vertex no_vertex = std::numeric_limits<Vertex>::max();
    std::vector<Vertex> parent(vertex_count_, no_vertex);
    std::vector<EdgeHandle> parent_edge(vertex_count_, no_edge);
    std::queue<Vertex> queue;
    parent[source] = source;
    queue.push(source);
    while (!queue.empty() && parent[target] == no_vertex) {
      const Vertex current = queue.front();
      queue.pop();
      for (const auto& [next, handle] : adjacency[current]) {
        if (parent[next] != no_vertex) {
          continue;
        }
        parent[next] = current;
        parent_edge[next] = handle;
        queue.push(next);
      }
    }
    if (parent[target] == no_vertex) {
      return std::nullopt;
    }
    std::vector<EdgeHandle> path;
    for (Vertex current = target; current != source; current = parent[current]) {
      path.push_back(parent_edge[current]);
    }
    std::reverse(path.begin(), path.end());
    return path;
  }

  [[nodiscard]] std::vector<bool> forest_component_mask(Vertex source) const {
    const auto adjacency = forest_adjacency();
    std::vector<bool> visited(vertex_count_, false);
    std::queue<Vertex> queue;
    visited[source] = true;
    queue.push(source);
    while (!queue.empty()) {
      const Vertex current = queue.front();
      queue.pop();
      for (const auto& [next, handle] : adjacency[current]) {
        static_cast<void>(handle);
        if (!visited[next]) {
          visited[next] = true;
          queue.push(next);
        }
      }
    }
    return visited;
  }

  std::size_t vertex_count_{0};
  std::vector<StoredEdge> edges_;
  std::size_t active_edge_count_{0};
  std::size_t forest_edge_count_{0};
};

}  // namespace algorithms::graphs
