#include "algorithms/graphs/incremental_topological_order.hpp"

#include <algorithm>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::graphs {

IncrementalTopologicalOrder::IncrementalTopologicalOrder(
    std::size_t vertex_count)
    : graph_(vertex_count, true), order_(vertex_count), position_(vertex_count) {
  for (std::size_t vertex = 0; vertex < vertex_count; ++vertex) {
    order_[vertex] = vertex;
    position_[vertex] = vertex;
  }
}

std::size_t IncrementalTopologicalOrder::vertex_count() const noexcept {
  return graph_.vertex_count();
}

std::size_t IncrementalTopologicalOrder::edge_count() const noexcept {
  return edge_count_;
}

std::span<const Vertex> IncrementalTopologicalOrder::order() const noexcept {
  return order_;
}

std::size_t IncrementalTopologicalOrder::position(Vertex vertex) const {
  graph_.validate_vertex(vertex);
  return position_[vertex];
}

bool IncrementalTopologicalOrder::try_add_edge(Vertex from, Vertex to) {
  graph_.validate_vertex(from);
  graph_.validate_vertex(to);
  if (from == to) {
    return false;
  }

  if (position_[from] < position_[to]) {
    graph_.add_edge(from, to);
    ++edge_count_;
    return true;
  }

  const std::size_t upper_position = position_[from];
  std::vector<unsigned char> affected(vertex_count(), 0U);
  std::vector<Vertex> stack;
  stack.push_back(to);
  affected[to] = 1U;

  while (!stack.empty()) {
    const Vertex current = stack.back();
    stack.pop_back();
    if (current == from) {
      return false;
    }

    for (const Edge& edge : graph_.neighbors(current)) {
      const Vertex next = edge.to;
      if (position_[next] > upper_position || affected[next] != 0U) {
        continue;
      }
      affected[next] = 1U;
      stack.push_back(next);
    }
  }

  graph_.add_edge(from, to);
  ++edge_count_;

  std::vector<Vertex> moved;
  moved.reserve(order_.size());
  std::vector<Vertex> remaining;
  remaining.reserve(order_.size());
  for (const Vertex vertex : order_) {
    if (affected[vertex] != 0U) {
      moved.push_back(vertex);
    } else {
      remaining.push_back(vertex);
    }
  }

  const auto from_iterator =
      std::find(remaining.begin(), remaining.end(), from);
  if (from_iterator == remaining.end()) {
    throw std::logic_error("incremental topological order lost source vertex");
  }
  const std::size_t insertion_offset =
      static_cast<std::size_t>(from_iterator - remaining.begin()) + 1U;
  remaining.insert(
      remaining.begin() + static_cast<std::ptrdiff_t>(insertion_offset),
      moved.begin(), moved.end());
  order_ = std::move(remaining);

  for (std::size_t index = 0; index < order_.size(); ++index) {
    position_[order_[index]] = index;
  }
  return true;
}

}  // namespace algorithms::graphs
