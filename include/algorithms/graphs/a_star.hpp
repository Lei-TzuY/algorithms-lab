#pragma once

#include "algorithms/data_structures/binary_heap.hpp"
#include "algorithms/graphs/graph.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::graphs {

struct AStarPathEdge {
  Vertex from;
  std::size_t adjacency_index;
  Vertex to;
  Weight weight;

  friend bool operator==(const AStarPathEdge&, const AStarPathEdge&) = default;
};

struct AStarPathResult {
  std::optional<Weight> distance;
  std::vector<Vertex> path;
  std::vector<AStarPathEdge> path_edges;
  std::vector<Vertex> expansion_order;
};

namespace a_star_detail {

struct WideDistance {
  std::uint64_t high = 0U;
  std::uint64_t low = 0U;

  friend bool operator==(const WideDistance&, const WideDistance&) = default;
};

[[nodiscard]] inline bool wide_less(const WideDistance& left,
                                    const WideDistance& right) noexcept {
  return left.high < right.high ||
         (left.high == right.high && left.low < right.low);
}

[[nodiscard]] inline WideDistance add_u64(WideDistance value,
                                          std::uint64_t increment) {
  const std::uint64_t old_low = value.low;
  value.low += increment;
  if (value.low < old_low) {
    if (value.high == std::numeric_limits<std::uint64_t>::max()) {
      throw std::overflow_error("A* internal wide-distance overflow");
    }
    ++value.high;
  }
  return value;
}

[[nodiscard]] inline WideDistance add_weight(WideDistance value, Weight weight) {
  return add_u64(value, static_cast<std::uint64_t>(weight));
}

[[nodiscard]] inline WideDistance add_heuristic(WideDistance value,
                                                 Weight heuristic) {
  return add_u64(value, static_cast<std::uint64_t>(heuristic));
}

struct QueueEntry {
  WideDistance estimate;
  WideDistance distance;
  Vertex vertex;
};

struct QueueEntryCompare {
  [[nodiscard]] bool operator()(const QueueEntry& left,
                                const QueueEntry& right) const noexcept {
    if (wide_less(left.estimate, right.estimate)) return true;
    if (wide_less(right.estimate, left.estimate)) return false;
    if (wide_less(left.distance, right.distance)) return true;
    if (wide_less(right.distance, left.distance)) return false;
    return left.vertex < right.vertex;
  }
};

inline void validate_contract(const Graph& graph, Vertex target,
                              std::span<const Weight> heuristic) {
  const std::size_t n = graph.vertex_count();
  if (heuristic.size() != n) {
    throw std::invalid_argument("A* heuristic size must equal graph vertex count");
  }
  for (const Weight value : heuristic) {
    if (value < 0) {
      throw std::invalid_argument("A* heuristic values must be non-negative");
    }
  }
  if (heuristic[target] != 0) {
    throw std::invalid_argument("A* target heuristic must be zero");
  }
  for (Vertex from = 0U; from < n; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (edge.weight < 0) {
        throw std::invalid_argument("A* requires globally non-negative edge weights");
      }
      const auto rhs = static_cast<std::uint64_t>(edge.weight) +
                       static_cast<std::uint64_t>(heuristic[edge.to]);
      if (static_cast<std::uint64_t>(heuristic[from]) > rhs) {
        throw std::invalid_argument("A* heuristic is not consistent on every graph edge");
      }
    }
  }
}

struct ParentRef {
  Vertex from;
  std::size_t adjacency_index;
};

inline void reconstruct_path(const Graph& graph, Vertex source, Vertex target,
                             const std::vector<std::optional<ParentRef>>& parent,
                             AStarPathResult& result) {
  std::vector<Vertex> reversed_vertices;
  std::vector<AStarPathEdge> reversed_edges;
  Vertex current = target;
  while (true) {
    reversed_vertices.push_back(current);
    if (current == source) break;
    if (!parent[current].has_value()) {
      throw std::logic_error("A* parent chain is incomplete");
    }
    const ParentRef ref = *parent[current];
    const auto& edges = graph.neighbors(ref.from);
    if (ref.adjacency_index >= edges.size()) {
      throw std::logic_error("A* parent edge index is invalid");
    }
    const auto& edge = edges[ref.adjacency_index];
    if (edge.to != current) {
      throw std::logic_error("A* parent edge target is inconsistent");
    }
    reversed_edges.push_back(
        AStarPathEdge{ref.from, ref.adjacency_index, edge.to, edge.weight});
    current = ref.from;
  }
  result.path.assign(reversed_vertices.rbegin(), reversed_vertices.rend());
  result.path_edges.assign(reversed_edges.rbegin(), reversed_edges.rend());
}

}  // namespace a_star_detail

// Exact A* shortest path under an executable consistent-heuristic contract.
// All graph weights and heuristic values must be non-negative, h(target)=0,
// and h(u) <= w(u,v)+h(v) must hold on every edge. These conditions are
// validated globally before search.
//
// Internal path/estimate arithmetic uses a two-limb unsigned distance so an
// overflowing non-optimal candidate cannot falsely reject a representable
// optimum. overflow_error is raised only if the final finite shortest distance
// itself cannot be represented by Weight.
//
// With a consistent heuristic, every expanded vertex is settled at its exact
// shortest distance. This stale-entry binary-heap baseline can retain O(E) queue
// entries, so its direct bound is O(V+E + E log(E+1)) time and O(V+E)
// auxiliary/result scale beyond the input graph.
[[nodiscard]] inline AStarPathResult a_star_shortest_path(
    const Graph& graph, Vertex source, Vertex target,
    std::span<const Weight> heuristic) {
  graph.validate_vertex(source);
  graph.validate_vertex(target);
  a_star_detail::validate_contract(graph, target, heuristic);

  const std::size_t n = graph.vertex_count();
  std::vector<std::optional<a_star_detail::WideDistance>> distance(n);
  std::vector<std::optional<a_star_detail::ParentRef>> parent(n);
  std::vector<unsigned char> closed(n, 0U);
  algorithms::data_structures::BinaryHeap<a_star_detail::QueueEntry,
                                          a_star_detail::QueueEntryCompare>
      queue;

  const a_star_detail::WideDistance zero{};
  distance[source] = zero;
  queue.push(a_star_detail::QueueEntry{
      a_star_detail::add_heuristic(zero, heuristic[source]), zero, source});

  AStarPathResult result;
  while (!queue.empty()) {
    const auto entry = queue.pop();
    if (closed[entry.vertex] != 0U || !distance[entry.vertex].has_value() ||
        !(entry.distance == *distance[entry.vertex])) {
      continue;
    }

    closed[entry.vertex] = 1U;
    result.expansion_order.push_back(entry.vertex);
    if (entry.vertex == target) {
      if (entry.distance.high != 0U ||
          entry.distance.low > static_cast<std::uint64_t>(
                                   std::numeric_limits<Weight>::max())) {
        throw std::overflow_error("A* shortest distance is not representable");
      }
      result.distance = static_cast<Weight>(entry.distance.low);
      a_star_detail::reconstruct_path(graph, source, target, parent, result);
      return result;
    }

    const auto& outgoing = graph.neighbors(entry.vertex);
    for (std::size_t edge_index = 0U; edge_index < outgoing.size(); ++edge_index) {
      const auto& edge = outgoing[edge_index];
      if (closed[edge.to] != 0U) continue;
      const auto candidate = a_star_detail::add_weight(entry.distance, edge.weight);
      if (!distance[edge.to].has_value() ||
          a_star_detail::wide_less(candidate, *distance[edge.to])) {
        distance[edge.to] = candidate;
        parent[edge.to] = a_star_detail::ParentRef{entry.vertex, edge_index};
        queue.push(a_star_detail::QueueEntry{
            a_star_detail::add_heuristic(candidate, heuristic[edge.to]),
            candidate, edge.to});
      }
    }
  }
  return result;
}

}  // namespace algorithms::graphs
