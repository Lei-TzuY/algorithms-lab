#include "algorithms/graphs/heavy_light_decomposition.hpp"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

HeavyLightDecomposition::HeavyLightDecomposition(
    const Graph& tree, const std::vector<std::int64_t>& vertex_values,
    Vertex root)
    : root_(root),
      parent_(tree.vertex_count(), tree.vertex_count()),
      depth_(tree.vertex_count(), 0U),
      subtree_size_(tree.vertex_count(), 1U),
      heavy_child_(tree.vertex_count(), tree.vertex_count()),
      head_(tree.vertex_count(), tree.vertex_count()),
      position_(tree.vertex_count(), tree.vertex_count()) {
  if (tree.directed()) {
    throw std::invalid_argument(
        "heavy-light decomposition requires an undirected tree");
  }
  if (tree.vertex_count() == 0U) {
    throw std::invalid_argument(
        "heavy-light decomposition requires a non-empty tree");
  }
  if (vertex_values.size() != tree.vertex_count()) {
    throw std::invalid_argument(
        "heavy-light decomposition value count mismatch");
  }
  tree.validate_vertex(root);

  const std::size_t vertex_count = tree.vertex_count();
  const Vertex no_vertex = vertex_count;
  std::vector<bool> visited(vertex_count, false);
  std::vector<bool> parent_edge_seen(vertex_count, false);
  std::vector<Vertex> order;
  order.reserve(vertex_count);
  std::vector<Vertex> stack;
  stack.push_back(root);
  parent_[root] = root;
  visited[root] = true;

  while (!stack.empty()) {
    const Vertex vertex = stack.back();
    stack.pop_back();
    order.push_back(vertex);

    for (const Edge& edge : tree.neighbors(vertex)) {
      const Vertex neighbor = edge.to;
      if (neighbor == vertex) {
        throw std::invalid_argument(
            "heavy-light decomposition tree must not contain self-loops");
      }
      if (neighbor == parent_[vertex]) {
        if (vertex != root && parent_edge_seen[vertex]) {
          throw std::invalid_argument(
              "heavy-light decomposition tree must not contain parallel edges");
        }
        if (vertex != root) {
          parent_edge_seen[vertex] = true;
        }
        continue;
      }
      if (visited[neighbor]) {
        throw std::invalid_argument(
            "heavy-light decomposition tree must be acyclic and simple");
      }
      visited[neighbor] = true;
      parent_[neighbor] = vertex;
      depth_[neighbor] = depth_[vertex] + 1U;
      stack.push_back(neighbor);
    }
  }

  if (order.size() != vertex_count) {
    throw std::invalid_argument(
        "heavy-light decomposition tree must be connected");
  }

  for (std::size_t offset = order.size(); offset > 1U; --offset) {
    const Vertex vertex = order[offset - 1U];
    const Vertex parent = parent_[vertex];
    if (subtree_size_[parent] >
        std::numeric_limits<std::size_t>::max() - subtree_size_[vertex]) {
      throw std::length_error(
          "heavy-light decomposition subtree size overflow");
    }
    subtree_size_[parent] += subtree_size_[vertex];

    const Vertex current_heavy = heavy_child_[parent];
    if (current_heavy == no_vertex ||
        subtree_size_[vertex] > subtree_size_[current_heavy] ||
        (subtree_size_[vertex] == subtree_size_[current_heavy] &&
         vertex < current_heavy)) {
      heavy_child_[parent] = vertex;
    }
  }

  struct ChainTask {
    Vertex start;
    Vertex head;
  };

  std::vector<ChainTask> pending;
  pending.push_back(ChainTask{root, root});
  std::size_t next_position = 0U;
  while (!pending.empty()) {
    const ChainTask task = pending.back();
    pending.pop_back();

    Vertex vertex = task.start;
    while (vertex != no_vertex) {
      head_[vertex] = task.head;
      position_[vertex] = next_position;
      ++next_position;

      std::vector<Vertex> light_children;
      for (const Edge& edge : tree.neighbors(vertex)) {
        if (parent_[edge.to] == vertex && edge.to != heavy_child_[vertex]) {
          light_children.push_back(edge.to);
        }
      }
      for (auto it = light_children.rbegin(); it != light_children.rend();
           ++it) {
        pending.push_back(ChainTask{*it, *it});
      }
      vertex = heavy_child_[vertex];
    }
  }

  std::vector<std::int64_t> linear_values(vertex_count, 0);
  for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
    linear_values[position_[vertex]] = vertex_values[vertex];
  }
  sums_.emplace(linear_values);
}

std::size_t HeavyLightDecomposition::vertex_count() const noexcept {
  return parent_.size();
}

Vertex HeavyLightDecomposition::root() const noexcept { return root_; }

void HeavyLightDecomposition::validate_vertex(Vertex vertex) const {
  if (vertex >= parent_.size()) {
    throw std::out_of_range(
        "heavy-light decomposition vertex out of range");
  }
}

void HeavyLightDecomposition::assign(Vertex vertex, std::int64_t value) {
  validate_vertex(vertex);
  sums_->assign(position_[vertex], value);
}

std::int64_t HeavyLightDecomposition::checked_add(std::int64_t left,
                                                   std::int64_t right) {
  if ((right > 0 &&
       left > std::numeric_limits<std::int64_t>::max() - right) ||
      (right < 0 &&
       left < std::numeric_limits<std::int64_t>::min() - right)) {
    throw std::overflow_error("heavy-light path sum overflow");
  }
  return left + right;
}

std::int64_t HeavyLightDecomposition::path_sum(Vertex first,
                                                Vertex second) const {
  validate_vertex(first);
  validate_vertex(second);

  std::int64_t total = 0;
  while (head_[first] != head_[second]) {
    if (depth_[head_[first]] < depth_[head_[second]]) {
      std::swap(first, second);
    }
    const Vertex chain_head = head_[first];
    total = checked_add(
        total,
        sums_->range_sum(position_[chain_head], position_[first] + 1U));
    first = parent_[chain_head];
  }

  const std::size_t begin =
      std::min(position_[first], position_[second]);
  const std::size_t end =
      std::max(position_[first], position_[second]) + 1U;
  return checked_add(total, sums_->range_sum(begin, end));
}

std::int64_t HeavyLightDecomposition::subtree_sum(Vertex vertex) const {
  validate_vertex(vertex);
  return sums_->range_sum(position_[vertex],
                          position_[vertex] + subtree_size_[vertex]);
}

}  // namespace algorithms::graphs
