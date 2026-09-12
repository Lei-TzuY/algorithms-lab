#pragma once

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

enum class DynamicBipartiteOperationKind {
  AddEdge,
  RemoveEdge,
  QueryBipartite,
};

struct DynamicBipartiteOperation {
  DynamicBipartiteOperationKind kind;
  std::size_t first = 0;
  std::size_t second = 0;
};

struct OfflineDynamicBipartitenessResult {
  // Answers appear in the same order as QueryBipartite operations.
  std::vector<bool> query_answers;
};

namespace detail {

using BipartiteEdgeKey = std::pair<std::size_t, std::size_t>;

struct BipartiteActiveInterval {
  std::size_t begin;
  std::size_t end;
  BipartiteEdgeKey edge;
};

[[nodiscard]] inline BipartiteEdgeKey canonical_bipartite_edge(
    std::size_t first, std::size_t second) {
  if (first <= second) {
    return {first, second};
  }
  return {second, first};
}

// Rollback DSU for parity equations color(a) xor color(b) = 1.
// Path compression is deliberately absent so every mutation is reversible.
class RollbackParityDisjointSetUnion {
 public:
  using Snapshot = std::size_t;

  explicit RollbackParityDisjointSetUnion(std::size_t vertex_count)
      : parent_(vertex_count), component_size_(vertex_count, 1),
        parity_to_parent_(vertex_count, false) {
    for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
      parent_[vertex] = vertex;
    }
  }

  [[nodiscard]] Snapshot snapshot() const noexcept { return history_.size(); }
  [[nodiscard]] bool bipartite() const noexcept { return conflicts_ == 0; }

  void add_opposite_constraint(std::size_t first, std::size_t second) {
    const FindResult found_first = find(first);
    const FindResult found_second = find(second);

    if (found_first.root == found_second.root) {
      const bool inconsistent =
          (found_first.parity_to_root == found_second.parity_to_root);
      if (inconsistent && conflicts_ == std::numeric_limits<std::size_t>::max()) {
        throw std::overflow_error("rollback parity DSU conflict-count overflow");
      }

      history_.push_back(Change{false, 0, 0, 0, false, conflicts_});
      if (inconsistent) {
        ++conflicts_;
      }
      return;
    }

    std::size_t parent_root = found_first.root;
    std::size_t child_root = found_second.root;
    if (component_size_[parent_root] < component_size_[child_root]) {
      std::swap(parent_root, child_root);
    }
    if (component_size_[parent_root] >
        std::numeric_limits<std::size_t>::max() - component_size_[child_root]) {
      throw std::overflow_error("rollback parity DSU component-size overflow");
    }

    const bool child_to_parent_parity =
        (found_first.parity_to_root == found_second.parity_to_root);

    history_.push_back(Change{true,
                              child_root,
                              parent_root,
                              component_size_[parent_root],
                              parity_to_parent_[child_root],
                              conflicts_});
    parent_[child_root] = parent_root;
    parity_to_parent_[child_root] = child_to_parent_parity;
    component_size_[parent_root] += component_size_[child_root];
  }

  void rollback(Snapshot target) {
    if (target > history_.size()) {
      throw std::out_of_range(
          "rollback parity DSU snapshot is no longer reachable");
    }

    while (history_.size() > target) {
      const Change change = history_.back();
      history_.pop_back();
      conflicts_ = change.conflicts_before;
      if (!change.merged) {
        continue;
      }
      parent_[change.child_root] = change.child_root;
      parity_to_parent_[change.child_root] = change.child_parity_before;
      component_size_[change.parent_root] = change.parent_size_before;
    }
  }

 private:
  struct FindResult {
    std::size_t root;
    bool parity_to_root;
  };

  struct Change {
    bool merged;
    std::size_t child_root;
    std::size_t parent_root;
    std::size_t parent_size_before;
    bool child_parity_before;
    std::size_t conflicts_before;
  };

  [[nodiscard]] FindResult find(std::size_t vertex) const {
    bool parity = false;
    while (vertex != parent_[vertex]) {
      parity = parity != parity_to_parent_[vertex];
      vertex = parent_[vertex];
    }
    return FindResult{vertex, parity};
  }

  std::vector<std::size_t> parent_;
  std::vector<std::size_t> component_size_;
  std::vector<bool> parity_to_parent_;
  std::size_t conflicts_ = 0;
  std::vector<Change> history_;
};

inline void validate_bipartite_operation(
    std::size_t vertex_count, const DynamicBipartiteOperation& operation) {
  switch (operation.kind) {
    case DynamicBipartiteOperationKind::QueryBipartite:
      return;
    case DynamicBipartiteOperationKind::AddEdge:
    case DynamicBipartiteOperationKind::RemoveEdge:
      if (operation.first >= vertex_count || operation.second >= vertex_count) {
        throw std::out_of_range("dynamic-bipartite vertex out of range");
      }
      return;
  }
  throw std::invalid_argument("unknown dynamic-bipartite operation kind");
}

inline void add_bipartite_interval(
    std::vector<std::vector<BipartiteEdgeKey>>& segment_tree,
    std::size_t node, std::size_t left, std::size_t right,
    std::size_t query_left, std::size_t query_right,
    const BipartiteEdgeKey& edge) {
  if (query_right <= left || right <= query_left) {
    return;
  }
  if (query_left <= left && right <= query_right) {
    segment_tree[node].push_back(edge);
    return;
  }

  const std::size_t middle = left + (right - left) / 2;
  add_bipartite_interval(segment_tree, node * 2, left, middle, query_left,
                         query_right, edge);
  add_bipartite_interval(segment_tree, node * 2 + 1, middle, right,
                         query_left, query_right, edge);
}

inline void evaluate_bipartite_segment_tree(
    const std::vector<std::vector<BipartiteEdgeKey>>& segment_tree,
    std::size_t node, std::size_t left, std::size_t right,
    std::span<const DynamicBipartiteOperation> operations,
    RollbackParityDisjointSetUnion& dsu, std::vector<bool>& query_answers) {
  const auto snapshot = dsu.snapshot();
  for (const auto& edge : segment_tree[node]) {
    dsu.add_opposite_constraint(edge.first, edge.second);
  }

  if (right - left == 1) {
    if (operations[left].kind == DynamicBipartiteOperationKind::QueryBipartite) {
      query_answers.push_back(dsu.bipartite());
    }
  } else {
    const std::size_t middle = left + (right - left) / 2;
    evaluate_bipartite_segment_tree(segment_tree, node * 2, left, middle,
                                    operations, dsu, query_answers);
    evaluate_bipartite_segment_tree(segment_tree, node * 2 + 1, middle, right,
                                    operations, dsu, query_answers);
  }

  dsu.rollback(snapshot);
}

}  // namespace detail

// Answers whether the complete active undirected multigraph is bipartite at
// each QueryBipartite point in an offline add/remove timeline.
//
// Each AddEdge opens one active copy of a canonical undirected edge. Each
// RemoveEdge closes one currently active copy (LIFO among identical copies);
// removing an inactive edge is rejected. Parallel copies preserve multiplicity.
// Any active self-loop makes the graph non-bipartite.
[[nodiscard]] inline OfflineDynamicBipartitenessResult
    offline_dynamic_bipartiteness(
        std::size_t vertex_count,
        std::span<const DynamicBipartiteOperation> operations) {
  for (const auto& operation : operations) {
    detail::validate_bipartite_operation(vertex_count, operation);
  }
  if (operations.empty()) {
    return {};
  }

  std::map<detail::BipartiteEdgeKey, std::vector<std::size_t>> open_additions;
  std::vector<detail::BipartiteActiveInterval> intervals;
  intervals.reserve(operations.size());
  std::size_t query_count = 0;

  for (std::size_t time = 0; time < operations.size(); ++time) {
    const auto& operation = operations[time];
    if (operation.kind == DynamicBipartiteOperationKind::QueryBipartite) {
      ++query_count;
      continue;
    }

    const auto edge =
        detail::canonical_bipartite_edge(operation.first, operation.second);
    if (operation.kind == DynamicBipartiteOperationKind::AddEdge) {
      open_additions[edge].push_back(time);
      continue;
    }

    auto active = open_additions.find(edge);
    if (active == open_additions.end() || active->second.empty()) {
      throw std::invalid_argument("removing an inactive dynamic-bipartite edge");
    }
    const std::size_t begin = active->second.back();
    active->second.pop_back();
    intervals.push_back(detail::BipartiteActiveInterval{begin, time, edge});
    if (active->second.empty()) {
      open_additions.erase(active);
    }
  }

  for (const auto& [edge, starts] : open_additions) {
    for (const std::size_t begin : starts) {
      intervals.push_back(
          detail::BipartiteActiveInterval{begin, operations.size(), edge});
    }
  }

  if (operations.size() > std::numeric_limits<std::size_t>::max() / 4) {
    throw std::length_error("dynamic-bipartite timeline is too large");
  }
  std::vector<std::vector<detail::BipartiteEdgeKey>> segment_tree(
      operations.size() * 4);
  for (const auto& interval : intervals) {
    if (interval.begin < interval.end) {
      detail::add_bipartite_interval(segment_tree, 1, 0, operations.size(),
                                     interval.begin, interval.end,
                                     interval.edge);
    }
  }

  detail::RollbackParityDisjointSetUnion dsu(vertex_count);
  std::vector<bool> answers;
  answers.reserve(query_count);
  detail::evaluate_bipartite_segment_tree(segment_tree, 1, 0,
                                           operations.size(), operations, dsu,
                                           answers);
  return OfflineDynamicBipartitenessResult{std::move(answers)};
}

}  // namespace algorithms::graphs
