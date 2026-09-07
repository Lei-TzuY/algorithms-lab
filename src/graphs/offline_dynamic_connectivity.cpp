#include "algorithms/graphs/offline_dynamic_connectivity.hpp"

#include "algorithms/data_structures/rollback_disjoint_set_union.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <map>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

struct EdgeKey {
  std::size_t first;
  std::size_t second;

  friend bool operator<(const EdgeKey& left, const EdgeKey& right) {
    return left.first < right.first ||
           (left.first == right.first && left.second < right.second);
  }
};

struct ActiveInterval {
  std::size_t begin;
  std::size_t end;
  EdgeKey edge;
};

[[nodiscard]] EdgeKey canonical_edge(std::size_t first, std::size_t second) {
  if (first <= second) {
    return EdgeKey{first, second};
  }
  return EdgeKey{second, first};
}

void validate_operation(std::size_t vertex_count,
                        const DynamicConnectivityOperation& operation) {
  if (operation.first >= vertex_count || operation.second >= vertex_count) {
    throw std::out_of_range("dynamic-connectivity vertex out of range");
  }
  switch (operation.kind) {
    case DynamicConnectivityOperationKind::AddEdge:
    case DynamicConnectivityOperationKind::RemoveEdge:
    case DynamicConnectivityOperationKind::QueryConnected:
      return;
  }
  throw std::invalid_argument("unknown dynamic-connectivity operation kind");
}

void add_interval(std::vector<std::vector<EdgeKey>>& segment_tree,
                  std::size_t node, std::size_t left, std::size_t right,
                  std::size_t query_left, std::size_t query_right,
                  const EdgeKey& edge) {
  if (query_right <= left || right <= query_left) {
    return;
  }
  if (query_left <= left && right <= query_right) {
    segment_tree[node].push_back(edge);
    return;
  }

  const std::size_t middle = left + (right - left) / 2;
  add_interval(segment_tree, node * 2, left, middle, query_left, query_right,
               edge);
  add_interval(segment_tree, node * 2 + 1, middle, right, query_left,
               query_right, edge);
}

void evaluate_segment_tree(
    const std::vector<std::vector<EdgeKey>>& segment_tree, std::size_t node,
    std::size_t left, std::size_t right,
    std::span<const DynamicConnectivityOperation> operations,
    algorithms::data_structures::RollbackDisjointSetUnion& dsu,
    std::vector<bool>& query_answers) {
  const auto snapshot = dsu.snapshot();
  for (const auto& edge : segment_tree[node]) {
    (void)dsu.unite(edge.first, edge.second);
  }

  if (right - left == 1) {
    const auto& operation = operations[left];
    if (operation.kind == DynamicConnectivityOperationKind::QueryConnected) {
      query_answers.push_back(dsu.connected(operation.first, operation.second));
    }
  } else {
    const std::size_t middle = left + (right - left) / 2;
    evaluate_segment_tree(segment_tree, node * 2, left, middle, operations, dsu,
                          query_answers);
    evaluate_segment_tree(segment_tree, node * 2 + 1, middle, right,
                          operations, dsu, query_answers);
  }

  dsu.rollback(snapshot);
}

}  // namespace

OfflineDynamicConnectivityResult offline_dynamic_connectivity(
    std::size_t vertex_count,
    std::span<const DynamicConnectivityOperation> operations) {
  for (const auto& operation : operations) {
    validate_operation(vertex_count, operation);
  }
  if (operations.empty()) {
    return {};
  }

  std::map<EdgeKey, std::vector<std::size_t>> open_additions;
  std::vector<ActiveInterval> intervals;
  intervals.reserve(operations.size());
  std::size_t query_count = 0;

  for (std::size_t time = 0; time < operations.size(); ++time) {
    const auto& operation = operations[time];
    if (operation.kind == DynamicConnectivityOperationKind::QueryConnected) {
      ++query_count;
      continue;
    }

    const EdgeKey edge = canonical_edge(operation.first, operation.second);
    if (operation.kind == DynamicConnectivityOperationKind::AddEdge) {
      open_additions[edge].push_back(time);
      continue;
    }

    auto active = open_additions.find(edge);
    if (active == open_additions.end() || active->second.empty()) {
      throw std::invalid_argument("removing an inactive dynamic edge");
    }
    const std::size_t begin = active->second.back();
    active->second.pop_back();
    intervals.push_back(ActiveInterval{begin, time, edge});
    if (active->second.empty()) {
      open_additions.erase(active);
    }
  }

  for (const auto& [edge, starts] : open_additions) {
    for (const std::size_t begin : starts) {
      intervals.push_back(ActiveInterval{begin, operations.size(), edge});
    }
  }

  if (operations.size() > std::numeric_limits<std::size_t>::max() / 4) {
    throw std::length_error("dynamic-connectivity timeline is too large");
  }
  std::vector<std::vector<EdgeKey>> segment_tree(operations.size() * 4);
  for (const auto& interval : intervals) {
    if (interval.begin < interval.end) {
      add_interval(segment_tree, 1, 0, operations.size(), interval.begin,
                   interval.end, interval.edge);
    }
  }

  algorithms::data_structures::RollbackDisjointSetUnion dsu(vertex_count);
  std::vector<bool> answers;
  answers.reserve(query_count);
  evaluate_segment_tree(segment_tree, 1, 0, operations.size(), operations, dsu,
                        answers);
  return OfflineDynamicConnectivityResult{std::move(answers)};
}

}  // namespace algorithms::graphs
