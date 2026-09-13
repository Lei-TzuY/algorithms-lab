#pragma once

#include "algorithms/data_structures/binary_heap.hpp"
#include "algorithms/graphs/graph.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace algorithms::graphs {

struct GreedySpannerEdgeWitness {
  Vertex from;
  std::size_t adjacency_index;
  Vertex to;
  Weight weight;

  friend bool operator==(const GreedySpannerEdgeWitness&,
                         const GreedySpannerEdgeWitness&) = default;
};

struct GreedySpannerResult {
  Graph spanner;
  std::vector<GreedySpannerEdgeWitness> selected_edges;
  std::size_t logical_input_edge_count{};
  std::size_t considered_non_loop_edge_count{};
  std::uint64_t stretch{};
};

namespace greedy_spanner_detail {

struct QueueEntry {
  Weight distance;
  Vertex vertex;
};

struct QueueEntryCompare {
  [[nodiscard]] bool operator()(const QueueEntry& first,
                                const QueueEntry& second) const noexcept {
    return std::tie(first.distance, first.vertex) <
           std::tie(second.distance, second.vertex);
  }
};

struct LogicalEdge {
  Vertex from;
  std::size_t adjacency_index;
  Vertex to;
  Weight weight;
};

[[nodiscard]] inline Weight checked_stretch_threshold(Weight weight,
                                                      std::uint64_t stretch) {
  if (weight < 0) {
    throw std::invalid_argument("greedy spanner requires non-negative weights");
  }
  if (weight == 0) {
    return 0;
  }

  const auto unsigned_weight = static_cast<std::uint64_t>(weight);
  const auto public_max =
      static_cast<std::uint64_t>(std::numeric_limits<Weight>::max());
  if (stretch > public_max / unsigned_weight) {
    throw std::overflow_error("greedy spanner stretch threshold is not representable");
  }
  return static_cast<Weight>(stretch * unsigned_weight);
}

[[nodiscard]] inline bool has_path_within(const Graph& graph, Vertex source,
                                          Vertex target, Weight threshold) {
  if (source == target) {
    return true;
  }

  std::vector<std::optional<Weight>> distance(graph.vertex_count());
  algorithms::data_structures::BinaryHeap<QueueEntry, QueueEntryCompare> queue;
  distance[source] = 0;
  queue.push(QueueEntry{0, source});

  while (!queue.empty()) {
    const QueueEntry current = queue.pop();
    if (!distance[current.vertex].has_value() ||
        current.distance != *distance[current.vertex]) {
      continue;
    }
    if (current.vertex == target) {
      return true;
    }

    for (const Edge& edge : graph.neighbors(current.vertex)) {
      if (edge.weight < 0) {
        throw std::logic_error("greedy spanner internal graph has a negative edge");
      }
      if (edge.weight > threshold ||
          current.distance > threshold - edge.weight) {
        continue;
      }
      const Weight candidate = current.distance + edge.weight;
      if (!distance[edge.to].has_value() || candidate < *distance[edge.to]) {
        distance[edge.to] = candidate;
        queue.push(QueueEntry{candidate, edge.to});
      }
    }
  }
  return false;
}

[[nodiscard]] inline std::vector<LogicalEdge> extract_logical_edges(
    const Graph& graph) {
  std::vector<LogicalEdge> edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    const auto& adjacency = graph.neighbors(from);
    for (std::size_t index = 0; index < adjacency.size(); ++index) {
      const Edge& edge = adjacency[index];
      if (edge.weight < 0) {
        throw std::invalid_argument("greedy spanner requires non-negative weights");
      }
      if (from > edge.to) {
        continue;
      }
      edges.push_back(LogicalEdge{from, index, edge.to, edge.weight});
    }
  }

  std::sort(edges.begin(), edges.end(), [](const LogicalEdge& first,
                                           const LogicalEdge& second) {
    return std::tie(first.weight, first.from, first.to, first.adjacency_index) <
           std::tie(second.weight, second.from, second.to,
                    second.adjacency_index);
  });
  return edges;
}

}  // namespace greedy_spanner_detail

// Deterministic greedy integer-stretch spanner for undirected non-negative graphs.
//
// For every non-loop logical input edge (u,v,w), process edges in ascending
// (weight,u,v,adjacency-index) order. Add the edge exactly when the current
// spanner has no u-v path of length <= stretch*w. Self-loops are omitted because
// they cannot improve any shortest-path distance under non-negative weights.
//
// The direct baseline rejects a non-loop edge when stretch*w is not representable
// in Weight. This keeps every shortest-path comparison exact in the public signed
// domain rather than silently saturating arithmetic.
//
// If E is the number of logical input edges, repeated first-principles heap
// Dijkstra gives the conservative bound O(E * (E+V) log(E+V)) time after the
// O(E log E) deterministic sort, with O(V+E) result/working storage.
[[nodiscard]] inline GreedySpannerResult greedy_weighted_spanner(
    const Graph& graph, std::uint64_t stretch) {
  if (graph.directed()) {
    throw std::invalid_argument("greedy spanner requires an undirected graph");
  }
  if (stretch == 0) {
    throw std::invalid_argument("greedy spanner stretch must be positive");
  }

  const auto edges = greedy_spanner_detail::extract_logical_edges(graph);
  GreedySpannerResult result{Graph(graph.vertex_count(), false), {}, edges.size(),
                             0, stretch};
  result.selected_edges.reserve(edges.size());

  for (const auto& edge : edges) {
    if (edge.from == edge.to) {
      continue;
    }
    ++result.considered_non_loop_edge_count;
    const Weight threshold =
        greedy_spanner_detail::checked_stretch_threshold(edge.weight, stretch);
    if (greedy_spanner_detail::has_path_within(result.spanner, edge.from,
                                                edge.to, threshold)) {
      continue;
    }

    result.spanner.add_edge(edge.from, edge.to, edge.weight);
    result.selected_edges.push_back(GreedySpannerEdgeWitness{
        edge.from, edge.adjacency_index, edge.to, edge.weight});
  }

  return result;
}

}  // namespace algorithms::graphs
