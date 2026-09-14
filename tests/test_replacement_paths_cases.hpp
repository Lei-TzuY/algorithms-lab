#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <queue>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

#include "algorithms/graphs/replacement_paths.hpp"

namespace replacement_paths_test_detail {

using algorithms::graphs::EdgeReplacementPathsResult;
using algorithms::graphs::Graph;
using algorithms::graphs::ReplacementLogicalEdgeWitness;
using algorithms::graphs::Vertex;
using algorithms::graphs::Weight;

struct LogicalEdge {
  Vertex first;
  Vertex second;
  Weight weight;
  ReplacementLogicalEdgeWitness witness;
};

[[nodiscard]] inline std::vector<LogicalEdge> logical_edges(const Graph& graph) {
  std::vector<LogicalEdge> result;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    const auto& edges = graph.neighbors(from);
    for (std::size_t adjacency_index = 0; adjacency_index < edges.size();
         ++adjacency_index) {
      const auto& edge = edges[adjacency_index];
      if (edge.to < from) {
        continue;
      }
      result.push_back(LogicalEdge{
          from, edge.to, edge.weight,
          ReplacementLogicalEdgeWitness{from, edge.to, adjacency_index,
                                        edge.weight}});
    }
  }
  return result;
}

[[nodiscard]] inline std::size_t witness_id(
    const Graph& graph, const ReplacementLogicalEdgeWitness& witness) {
  const auto edges = logical_edges(graph);
  for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
    if (edges[edge_id].witness == witness) {
      return edge_id;
    }
  }
  throw std::runtime_error("replacement-path witness is not an input edge");
}

[[nodiscard]] inline std::optional<std::uint64_t> failure_dijkstra(
    const Graph& graph, Vertex source, Vertex target,
    std::optional<std::size_t> failed_edge) {
  const auto edges = logical_edges(graph);
  std::vector<std::vector<std::pair<Vertex, std::size_t>>> adjacency(
      graph.vertex_count());
  for (std::size_t edge_id = 0; edge_id < edges.size(); ++edge_id) {
    adjacency[edges[edge_id].first].push_back({edges[edge_id].second, edge_id});
    if (edges[edge_id].first != edges[edge_id].second) {
      adjacency[edges[edge_id].second].push_back({edges[edge_id].first, edge_id});
    }
  }

  constexpr std::uint64_t unreachable =
      std::numeric_limits<std::uint64_t>::max();
  std::vector<std::uint64_t> distance(graph.vertex_count(), unreachable);
  distance[source] = 0;
  using QueueEntry = std::pair<std::uint64_t, Vertex>;
  std::priority_queue<QueueEntry, std::vector<QueueEntry>,
                      std::greater<QueueEntry>>
      queue;
  queue.push({0, source});

  while (!queue.empty()) {
    const auto [current_distance, vertex] = queue.top();
    queue.pop();
    if (current_distance != distance[vertex]) {
      continue;
    }
    for (const auto& [next, edge_id] : adjacency[vertex]) {
      if (failed_edge.has_value() && edge_id == *failed_edge) {
        continue;
      }
      const auto weight = static_cast<std::uint64_t>(edges[edge_id].weight);
      const std::uint64_t candidate =
          current_distance > unreachable - weight
              ? unreachable
              : current_distance + weight;
      if (candidate < distance[next]) {
        distance[next] = candidate;
        queue.push({candidate, next});
      }
    }
  }

  if (distance[target] == unreachable) {
    return std::nullopt;
  }
  return distance[target];
}

inline void verify_result_against_oracle(const Graph& graph, Vertex source,
                                         Vertex target) {
  const auto actual =
      algorithms::graphs::undirected_edge_replacement_paths(graph, source,
                                                             target);
  const auto baseline = failure_dijkstra(graph, source, target, std::nullopt);
  if (!baseline.has_value()) {
    REQUIRE(!actual.has_value());
    return;
  }

  REQUIRE(actual.has_value());
  REQUIRE_EQ(static_cast<std::uint64_t>(actual->shortest_distance), *baseline);
  REQUIRE(!actual->shortest_path_vertices.empty());
  REQUIRE_EQ(actual->shortest_path_vertices.front(), source);
  REQUIRE_EQ(actual->shortest_path_vertices.back(), target);
  REQUIRE_EQ(actual->shortest_path_edges.size() + 1,
             actual->shortest_path_vertices.size());
  REQUIRE_EQ(actual->replacements.size(), actual->shortest_path_edges.size());

  std::uint64_t replayed_path_cost = 0;
  for (std::size_t index = 0; index < actual->shortest_path_edges.size();
       ++index) {
    const auto& witness = actual->shortest_path_edges[index];
    const auto edge_id = witness_id(graph, witness);
    const auto edges = logical_edges(graph);
    const auto& edge = edges[edge_id];
    const Vertex from = actual->shortest_path_vertices[index];
    const Vertex to = actual->shortest_path_vertices[index + 1];
    REQUIRE((edge.first == from && edge.second == to) ||
            (edge.first == to && edge.second == from));
    replayed_path_cost += static_cast<std::uint64_t>(edge.weight);

    const auto expected = failure_dijkstra(graph, source, target, edge_id);
    const auto& replacement = actual->replacements[index];
    REQUIRE_EQ(replacement.failed_edge, witness);
    if (!expected.has_value()) {
      REQUIRE(!replacement.distance.has_value());
      REQUIRE(!replacement.detour_edge.has_value());
      REQUIRE(!replacement.detour_from.has_value());
      REQUIRE(!replacement.detour_to.has_value());
      continue;
    }

    REQUIRE(*expected <=
            static_cast<std::uint64_t>(std::numeric_limits<Weight>::max()));
    REQUIRE(replacement.distance.has_value());
    REQUIRE_EQ(static_cast<std::uint64_t>(*replacement.distance), *expected);
    REQUIRE(replacement.detour_edge.has_value());
    REQUIRE(replacement.detour_from.has_value());
    REQUIRE(replacement.detour_to.has_value());
    const auto detour_id = witness_id(graph, *replacement.detour_edge);
    REQUIRE(detour_id != edge_id);
    const auto& detour = edges[detour_id];
    REQUIRE((detour.first == *replacement.detour_from &&
             detour.second == *replacement.detour_to) ||
            (detour.first == *replacement.detour_to &&
             detour.second == *replacement.detour_from));
  }
  REQUIRE_EQ(replayed_path_cost, *baseline);
}

}  // namespace replacement_paths_test_detail

using algorithms::graphs::Graph;

TEST_CASE(replacement_paths_validation_and_boundary_semantics) {
  using algorithms::graphs::Graph;
  using algorithms::graphs::undirected_edge_replacement_paths;

  Graph directed(2, true);
  directed.add_edge(0, 1, 1);
  REQUIRE_THROWS_AS(undirected_edge_replacement_paths(directed, 0, 1),
                    std::invalid_argument);

  Graph zero(2, false);
  zero.add_edge(0, 1, 0);
  REQUIRE_THROWS_AS(undirected_edge_replacement_paths(zero, 0, 1),
                    std::invalid_argument);

  Graph negative(2, false);
  negative.add_edge(0, 1, -1);
  REQUIRE_THROWS_AS(undirected_edge_replacement_paths(negative, 0, 1),
                    std::invalid_argument);

  Graph disconnected(3, false);
  disconnected.add_edge(0, 1, 4);
  REQUIRE(!undirected_edge_replacement_paths(disconnected, 0, 2).has_value());

  Graph same(3, false);
  same.add_edge(0, 1, 3);
  same.add_edge(1, 2, 4);
  const auto same_result = undirected_edge_replacement_paths(same, 1, 1);
  REQUIRE(same_result.has_value());
  REQUIRE_EQ(same_result->shortest_distance, 0);
  REQUIRE_EQ(same_result->shortest_path_vertices,
             std::vector<algorithms::graphs::Vertex>{1});
  REQUIRE(same_result->shortest_path_edges.empty());
  REQUIRE(same_result->replacements.empty());
}

TEST_CASE(replacement_paths_parallel_bridge_and_detour_witnesses) {
  using replacement_paths_test_detail::verify_result_against_oracle;

  Graph parallel(3, false);
  parallel.add_edge(0, 1, 5);
  parallel.add_edge(0, 1, 2);
  parallel.add_edge(1, 2, 3);
  parallel.add_edge(0, 2, 10);
  parallel.add_edge(1, 1, 7);
  verify_result_against_oracle(parallel, 0, 2);

  Graph bridge(4, false);
  bridge.add_edge(0, 1, 1);
  bridge.add_edge(1, 2, 1);
  bridge.add_edge(2, 3, 1);
  const auto bridge_result =
      algorithms::graphs::undirected_edge_replacement_paths(bridge, 0, 3);
  REQUIRE(bridge_result.has_value());
  REQUIRE_EQ(bridge_result->replacements.size(), 3U);
  for (const auto& replacement : bridge_result->replacements) {
    REQUIRE(!replacement.distance.has_value());
    REQUIRE(!replacement.detour_edge.has_value());
  }

  Graph detours(5, false);
  detours.add_edge(0, 1, 2);
  detours.add_edge(1, 4, 2);
  detours.add_edge(0, 2, 3);
  detours.add_edge(2, 3, 1);
  detours.add_edge(3, 4, 3);
  detours.add_edge(1, 2, 2);
  verify_result_against_oracle(detours, 0, 4);
}

TEST_CASE(replacement_paths_checked_distance_boundaries) {
  using algorithms::graphs::Weight;
  using algorithms::graphs::undirected_edge_replacement_paths;
  constexpr Weight max = std::numeric_limits<Weight>::max();

  Graph exact_max(2, false);
  exact_max.add_edge(0, 1, 5);
  exact_max.add_edge(0, 1, max);
  const auto exact = undirected_edge_replacement_paths(exact_max, 0, 1);
  REQUIRE(exact.has_value());
  REQUIRE_EQ(exact->replacements.size(), 1U);
  REQUIRE_EQ(exact->replacements[0].distance, std::optional<Weight>{max});

  Graph replacement_overflow(3, false);
  replacement_overflow.add_edge(0, 2, 5);
  replacement_overflow.add_edge(0, 1, max);
  replacement_overflow.add_edge(1, 2, 1);
  REQUIRE_THROWS_AS(
      undirected_edge_replacement_paths(replacement_overflow, 0, 2),
      std::overflow_error);

  Graph baseline_overflow(3, false);
  baseline_overflow.add_edge(0, 1, max);
  baseline_overflow.add_edge(1, 2, 1);
  REQUIRE_THROWS_AS(undirected_edge_replacement_paths(baseline_overflow, 0, 2),
                    std::overflow_error);

  Graph huge_nonoptimal(4, false);
  huge_nonoptimal.add_edge(0, 1, 2);
  huge_nonoptimal.add_edge(1, 3, 2);
  huge_nonoptimal.add_edge(0, 3, 10);
  huge_nonoptimal.add_edge(0, 2, max);
  huge_nonoptimal.add_edge(2, 3, max);
  const auto safe = undirected_edge_replacement_paths(huge_nonoptimal, 0, 3);
  REQUIRE(safe.has_value());
  REQUIRE_EQ(safe->replacements.size(), 2U);
  REQUIRE_EQ(safe->replacements[0].distance, std::optional<Weight>{10});
  REQUIRE_EQ(safe->replacements[1].distance, std::optional<Weight>{10});
}

TEST_CASE(replacement_paths_randomized_against_per_failure_dijkstra) {
  using algorithms::graphs::Vertex;
  using algorithms::graphs::Weight;
  using replacement_paths_test_detail::verify_result_against_oracle;

  std::mt19937_64 random(0xA11CEBEEFULL);
  for (std::size_t trial = 0; trial < 700U; ++trial) {
    const std::size_t vertex_count = 2U + static_cast<std::size_t>(random() % 8U);
    Graph graph(vertex_count, false);
    for (Vertex from = 0; from < vertex_count; ++from) {
      if ((random() % 7U) == 0U) {
        graph.add_edge(from, from, 1 + static_cast<Weight>(random() % 20U));
      }
      for (Vertex to = from + 1; to < vertex_count; ++to) {
        if ((random() % 100U) < 28U) {
          graph.add_edge(from, to, 1 + static_cast<Weight>(random() % 30U));
          if ((random() % 9U) == 0U) {
            graph.add_edge(from, to,
                           1 + static_cast<Weight>(random() % 30U));
          }
        }
      }
    }

    const Vertex source = static_cast<Vertex>(random() % vertex_count);
    Vertex target = static_cast<Vertex>(random() % vertex_count);
    if (target == source) {
      target = (target + 1U) % vertex_count;
    }
    verify_result_against_oracle(graph, source, target);
  }
}
