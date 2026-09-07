#include "algorithms/graphs/lowest_common_ancestor.hpp"

#include <bit>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace algorithms::graphs {

LowestCommonAncestor::LowestCommonAncestor(const Graph& tree, Vertex root)
    : root_(root), depth_(tree.vertex_count(), 0) {
  if (tree.directed()) {
    throw std::invalid_argument("LCA requires an undirected tree");
  }
  tree.validate_vertex(root);

  const std::size_t vertex_count = tree.vertex_count();
  const Vertex no_parent = vertex_count;
  std::vector<Vertex> parent(vertex_count, no_parent);
  std::vector<bool> visited(vertex_count, false);
  std::vector<bool> parent_edge_seen(vertex_count, false);
  std::vector<Vertex> order;
  order.reserve(vertex_count);
  std::vector<Vertex> stack;
  stack.push_back(root);
  visited[root] = true;
  parent[root] = root;

  while (!stack.empty()) {
    const Vertex vertex = stack.back();
    stack.pop_back();
    order.push_back(vertex);
    for (const auto& edge : tree.neighbors(vertex)) {
      const Vertex neighbor = edge.to;
      if (neighbor == vertex) {
        throw std::invalid_argument("LCA tree must not contain self-loops");
      }
      if (neighbor == parent[vertex]) {
        if (vertex != root && parent_edge_seen[vertex]) {
          throw std::invalid_argument("LCA tree must not contain parallel edges");
        }
        if (vertex != root) {
          parent_edge_seen[vertex] = true;
        }
        continue;
      }
      if (visited[neighbor]) {
        throw std::invalid_argument("LCA tree must be acyclic and simple");
      }
      visited[neighbor] = true;
      parent[neighbor] = vertex;
      depth_[neighbor] = depth_[vertex] + 1;
      stack.push_back(neighbor);
    }
  }
  if (order.size() != vertex_count) {
    throw std::invalid_argument("LCA tree must be connected");
  }

  const std::size_t level_count =
      static_cast<std::size_t>(std::bit_width(vertex_count));
  up_.assign(level_count, std::vector<Vertex>(vertex_count, root));
  up_[0] = parent;
  for (std::size_t level = 1; level < level_count; ++level) {
    for (Vertex vertex = 0; vertex < vertex_count; ++vertex) {
      up_[level][vertex] = up_[level - 1][up_[level - 1][vertex]];
    }
  }
}

std::size_t LowestCommonAncestor::vertex_count() const noexcept {
  return depth_.size();
}

Vertex LowestCommonAncestor::root() const noexcept { return root_; }

void LowestCommonAncestor::validate_vertex(Vertex vertex) const {
  if (vertex >= depth_.size()) {
    throw std::out_of_range("LCA vertex out of range");
  }
}

std::size_t LowestCommonAncestor::depth(Vertex vertex) const {
  validate_vertex(vertex);
  return depth_[vertex];
}

Vertex LowestCommonAncestor::lift(Vertex vertex, std::size_t steps) const {
  std::size_t level = 0;
  while (steps != 0) {
    if ((steps & std::size_t{1}) != 0) {
      vertex = up_[level][vertex];
    }
    steps >>= 1U;
    ++level;
  }
  return vertex;
}

std::optional<Vertex> LowestCommonAncestor::kth_ancestor(
    Vertex vertex, std::size_t steps) const {
  validate_vertex(vertex);
  if (steps > depth_[vertex]) {
    return std::nullopt;
  }
  return lift(vertex, steps);
}

Vertex LowestCommonAncestor::lca(Vertex first, Vertex second) const {
  validate_vertex(first);
  validate_vertex(second);
  if (depth_[first] < depth_[second]) {
    const Vertex temporary = first;
    first = second;
    second = temporary;
  }
  first = lift(first, depth_[first] - depth_[second]);
  if (first == second) {
    return first;
  }
  for (std::size_t level = up_.size(); level > 0; --level) {
    const std::size_t index = level - 1;
    if (up_[index][first] != up_[index][second]) {
      first = up_[index][first];
      second = up_[index][second];
    }
  }
  return up_[0][first];
}

std::size_t LowestCommonAncestor::distance_edges(Vertex first,
                                                 Vertex second) const {
  const Vertex ancestor = lca(first, second);
  return (depth_[first] - depth_[ancestor]) +
         (depth_[second] - depth_[ancestor]);
}

}  // namespace algorithms::graphs
