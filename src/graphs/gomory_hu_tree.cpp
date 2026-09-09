#include "algorithms/graphs/gomory_hu_tree.hpp"

#include <algorithm>
#include <limits>
#include <queue>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {
namespace {

void validate_input(std::size_t vertex_count,
                    std::span<const UndirectedCapacityEdge> edges) {
  for (const auto& edge : edges) {
    if (edge.first >= vertex_count || edge.second >= vertex_count) {
      throw std::out_of_range("Gomory-Hu edge endpoint is out of range");
    }
    if (edge.capacity < 0) {
      throw std::invalid_argument(
          "Gomory-Hu capacities must be non-negative");
    }
  }
}

[[nodiscard]] std::vector<CapacityEdge> directed_capacity_network(
    std::span<const UndirectedCapacityEdge> edges) {
  std::vector<CapacityEdge> directed;
  for (const auto& edge : edges) {
    if (edge.first == edge.second) {
      continue;
    }
    directed.push_back(CapacityEdge{edge.first, edge.second, edge.capacity});
    directed.push_back(CapacityEdge{edge.second, edge.first, edge.capacity});
  }
  return directed;
}

}  // namespace

GomoryHuTree::GomoryHuTree(std::vector<Vertex> parent,
                           std::vector<Capacity> cut_to_parent)
    : parent_(std::move(parent)), cut_to_parent_(std::move(cut_to_parent)) {}

std::size_t GomoryHuTree::vertex_count() const noexcept { return parent_.size(); }

const std::vector<Vertex>& GomoryHuTree::parent() const noexcept {
  return parent_;
}

const std::vector<Capacity>& GomoryHuTree::cut_to_parent() const noexcept {
  return cut_to_parent_;
}

Capacity GomoryHuTree::min_cut(Vertex first, Vertex second) const {
  if (first >= vertex_count() || second >= vertex_count()) {
    throw std::out_of_range("Gomory-Hu query vertex is out of range");
  }
  if (first == second) {
    throw std::invalid_argument("Gomory-Hu query vertices must differ");
  }

  std::vector<std::vector<std::pair<Vertex, Capacity>>> adjacency(vertex_count());
  for (Vertex vertex = 1; vertex < vertex_count(); ++vertex) {
    const Vertex ancestor = parent_[vertex];
    const Capacity capacity = cut_to_parent_[vertex];
    adjacency[vertex].push_back({ancestor, capacity});
    adjacency[ancestor].push_back({vertex, capacity});
  }

  std::vector<bool> seen(vertex_count(), false);
  std::vector<Capacity> bottleneck(
      vertex_count(), std::numeric_limits<Capacity>::max());
  std::queue<Vertex> queue;
  seen[first] = true;
  queue.push(first);

  while (!queue.empty()) {
    const Vertex vertex = queue.front();
    queue.pop();
    for (const auto& [next, capacity] : adjacency[vertex]) {
      if (seen[next]) {
        continue;
      }
      seen[next] = true;
      bottleneck[next] = std::min(bottleneck[vertex], capacity);
      if (next == second) {
        return bottleneck[next];
      }
      queue.push(next);
    }
  }

  throw std::logic_error("Gomory-Hu tree is disconnected");
}

GomoryHuTree build_gomory_hu_tree(
    std::size_t vertex_count,
    std::span<const UndirectedCapacityEdge> edges) {
  validate_input(vertex_count, edges);
  if (vertex_count == 0) {
    return GomoryHuTree({}, {});
  }

  const auto directed = directed_capacity_network(edges);
  std::vector<Vertex> parent(vertex_count, Vertex{0});
  std::vector<Capacity> cut_to_parent(vertex_count, Capacity{0});

  for (Vertex source = 1; source < vertex_count; ++source) {
    const Vertex sink = parent[source];
    const auto flow = dinic_max_flow(vertex_count, directed, source, sink);
    const auto& source_side = flow.source_side_min_cut;

    for (Vertex vertex = source + 1; vertex < vertex_count; ++vertex) {
      if (parent[vertex] == sink && source_side[vertex]) {
        parent[vertex] = source;
      }
    }

    if (sink != 0 && source_side[parent[sink]]) {
      parent[source] = parent[sink];
      parent[sink] = source;
      cut_to_parent[source] = cut_to_parent[sink];
      cut_to_parent[sink] = flow.value;
    } else {
      cut_to_parent[source] = flow.value;
    }
  }

  return GomoryHuTree(std::move(parent), std::move(cut_to_parent));
}

}  // namespace algorithms::graphs
