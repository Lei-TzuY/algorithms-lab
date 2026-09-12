#pragma once

#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

#include "algorithms/data_structures/radix_heap.hpp"
#include "algorithms/graphs/graph.hpp"

namespace algorithms::graphs {

struct RadixShortestPathResult {
  std::vector<std::optional<Weight>> distance;
  std::vector<std::optional<Vertex>> parent;
};

[[nodiscard]] inline RadixShortestPathResult radix_heap_dijkstra(
    const Graph& graph, Vertex source) {
  graph.validate_vertex(source);
  for (const auto& edges : graph.adjacency()) {
    for (const Edge& edge : edges) {
      if (edge.weight < 0) {
        throw std::invalid_argument(
            "radix_heap_dijkstra requires globally non-negative edge weights");
      }
    }
  }

  RadixShortestPathResult result;
  result.distance.resize(graph.vertex_count());
  result.parent.resize(graph.vertex_count());
  result.distance[source] = Weight{0};

  data_structures::RadixHeap<Vertex> queue;
  queue.push(0U, source);

  while (!queue.empty()) {
    const auto current = queue.pop();
    const Weight current_distance = static_cast<Weight>(current.key);
    if (!result.distance[current.value].has_value() ||
        current_distance != *result.distance[current.value]) {
      continue;
    }

    for (const Edge& edge : graph.neighbors(current.value)) {
      if (edge.weight > 0 &&
          current_distance > std::numeric_limits<Weight>::max() - edge.weight) {
        throw std::overflow_error("radix Dijkstra distance overflow");
      }
      const Weight candidate = current_distance + edge.weight;
      if (!result.distance[edge.to].has_value() ||
          candidate < *result.distance[edge.to]) {
        result.distance[edge.to] = candidate;
        result.parent[edge.to] = current.value;
        queue.push(static_cast<std::uint64_t>(candidate), edge.to);
      }
    }
  }
  return result;
}

}  // namespace algorithms::graphs
