#include "algorithms/dynamic_programming/tree_independent_set.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::dynamic_programming {
namespace {

std::int64_t checked_add(std::int64_t left, std::int64_t right) {
  if (right > 0 &&
      left > std::numeric_limits<std::int64_t>::max() - right) {
    throw std::overflow_error("tree-DP independent-set weight overflow");
  }
  if (right < 0 &&
      left < std::numeric_limits<std::int64_t>::min() - right) {
    throw std::overflow_error("tree-DP independent-set weight overflow");
  }
  return left + right;
}

}  // namespace

TreeIndependentSetResult maximum_weight_independent_set_tree(
    const graphs::Graph& tree,
    const std::vector<std::int64_t>& vertex_weights, graphs::Vertex root) {
  if (tree.directed()) {
    throw std::invalid_argument("tree DP requires an undirected graph");
  }

  const std::size_t vertex_count = tree.vertex_count();
  if (vertex_weights.size() != vertex_count) {
    throw std::invalid_argument("vertex weight count must match tree size");
  }
  if (vertex_count == 0U) {
    return TreeIndependentSetResult{};
  }
  tree.validate_vertex(root);

  const graphs::Vertex no_parent = vertex_count;
  std::vector<graphs::Vertex> parent(vertex_count, no_parent);
  std::vector<bool> visited(vertex_count, false);
  std::vector<bool> parent_edge_seen(vertex_count, false);
  std::vector<graphs::Vertex> order;
  order.reserve(vertex_count);
  std::vector<graphs::Vertex> stack;
  stack.push_back(root);
  visited[root] = true;

  while (!stack.empty()) {
    const graphs::Vertex vertex = stack.back();
    stack.pop_back();
    order.push_back(vertex);

    for (const auto& edge : tree.neighbors(vertex)) {
      const graphs::Vertex neighbor = edge.to;
      if (neighbor == vertex) {
        throw std::invalid_argument("tree must not contain self-loops");
      }
      if (neighbor == parent[vertex]) {
        if (parent_edge_seen[vertex]) {
          throw std::invalid_argument("tree must not contain parallel edges");
        }
        parent_edge_seen[vertex] = true;
        continue;
      }
      if (visited[neighbor]) {
        throw std::invalid_argument("tree must be acyclic");
      }
      visited[neighbor] = true;
      parent[neighbor] = vertex;
      stack.push_back(neighbor);
    }
  }

  if (order.size() != vertex_count) {
    throw std::invalid_argument("tree must be connected");
  }

  std::vector<std::int64_t> take(vertex_count, 0);
  std::vector<std::int64_t> skip(vertex_count, 0);

  for (std::size_t position = order.size(); position > 0U; --position) {
    const graphs::Vertex vertex = order[position - 1U];
    std::int64_t take_value = vertex_weights[vertex];
    std::int64_t skip_value = 0;

    for (const auto& edge : tree.neighbors(vertex)) {
      const graphs::Vertex child = edge.to;
      if (parent[child] != vertex) {
        continue;
      }
      take_value = checked_add(take_value, skip[child]);
      const std::int64_t child_best =
          take[child] > skip[child] ? take[child] : skip[child];
      skip_value = checked_add(skip_value, child_best);
    }

    take[vertex] = take_value;
    skip[vertex] = skip_value;
  }

  std::vector<bool> selected(vertex_count, false);
  for (const graphs::Vertex vertex : order) {
    const bool parent_selected =
        parent[vertex] != no_parent && selected[parent[vertex]];
    if (!parent_selected && take[vertex] > skip[vertex]) {
      selected[vertex] = true;
    }
  }

  TreeIndependentSetResult result;
  result.maximum_weight = take[root] > skip[root] ? take[root] : skip[root];
  for (graphs::Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    if (selected[vertex]) {
      result.vertices.push_back(vertex);
    }
  }
  return result;
}

}  // namespace algorithms::dynamic_programming
