#include "algorithms/graphs/shortest_paths.hpp"

#include <limits>
#include <stdexcept>
#include <utility>

#include "algorithms/data_structures/binary_heap.hpp"

namespace algorithms::graphs {
namespace {

Weight checked_add(Weight a, Weight b) {
  constexpr Weight max = std::numeric_limits<Weight>::max();
  constexpr Weight min = std::numeric_limits<Weight>::min();
  if (b > 0 && a > max - b) {
    throw std::overflow_error("shortest-path distance overflow");
  }
  if (b < 0 && a < min - b) {
    throw std::overflow_error("shortest-path distance underflow");
  }
  return a + b;
}

struct QueueEntry {
  Weight distance;
  Vertex vertex;
};

struct QueueEntryCompare {
  bool operator()(const QueueEntry& a, const QueueEntry& b) const noexcept {
    if (a.distance != b.distance) {
      return a.distance < b.distance;
    }
    return a.vertex < b.vertex;
  }
};

}  // namespace

ShortestPathResult dijkstra(const Graph& graph, Vertex source) {
  graph.validate_vertex(source);
  for (const auto& edges : graph.adjacency()) {
    for (const Edge& edge : edges) {
      if (edge.weight < 0) {
        throw std::invalid_argument(
            "dijkstra requires globally non-negative edge weights");
      }
    }
  }

  ShortestPathResult result;
  result.distance.resize(graph.vertex_count());
  result.parent.resize(graph.vertex_count());
  result.distance[source] = 0;

  data_structures::BinaryHeap<QueueEntry, QueueEntryCompare> queue;
  queue.push(QueueEntry{0, source});

  while (!queue.empty()) {
    const QueueEntry current = queue.pop();
    if (!result.distance[current.vertex].has_value() ||
        current.distance != *result.distance[current.vertex]) {
      continue;  // stale entry
    }

    for (const Edge& edge : graph.neighbors(current.vertex)) {
      const Weight candidate = checked_add(current.distance, edge.weight);
      if (!result.distance[edge.to].has_value() ||
          candidate < *result.distance[edge.to]) {
        result.distance[edge.to] = candidate;
        result.parent[edge.to] = current.vertex;
        queue.push(QueueEntry{candidate, edge.to});
      }
    }
  }
  return result;
}

BellmanFordResult bellman_ford(const Graph& graph, Vertex source) {
  graph.validate_vertex(source);
  BellmanFordResult result;
  result.distance.resize(graph.vertex_count());
  result.parent.resize(graph.vertex_count());
  result.distance[source] = 0;

  if (graph.vertex_count() > 0) {
    for (std::size_t pass = 1; pass < graph.vertex_count(); ++pass) {
      bool changed = false;
      for (Vertex from = 0; from < graph.vertex_count(); ++from) {
        if (!result.distance[from].has_value()) {
          continue;
        }
        for (const Edge& edge : graph.neighbors(from)) {
          const Weight candidate = checked_add(*result.distance[from], edge.weight);
          if (!result.distance[edge.to].has_value() ||
              candidate < *result.distance[edge.to]) {
            result.distance[edge.to] = candidate;
            result.parent[edge.to] = from;
            changed = true;
          }
        }
      }
      if (!changed) {
        break;
      }
    }
  }

  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    if (!result.distance[from].has_value()) {
      continue;
    }
    for (const Edge& edge : graph.neighbors(from)) {
      const Weight candidate = checked_add(*result.distance[from], edge.weight);
      if (!result.distance[edge.to].has_value() ||
          candidate < *result.distance[edge.to]) {
        result.has_reachable_negative_cycle = true;
        return result;
      }
    }
  }
  return result;
}

}  // namespace algorithms::graphs
