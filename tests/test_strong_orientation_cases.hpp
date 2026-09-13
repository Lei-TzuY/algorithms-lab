#pragma once

#include "test_framework.hpp"

#include "algorithms/graphs/strong_orientation.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <queue>
#include <random>
#include <stdexcept>
#include <tuple>
#include <utility>
#include <vector>

namespace {

using algorithms::graphs::Graph;
using algorithms::graphs::StrongOrientationEdge;
using algorithms::graphs::StrongOrientationResult;
using algorithms::graphs::StrongOrientationStatus;
using algorithms::graphs::Vertex;
using algorithms::graphs::Weight;

struct OrientationInputEdge {
  Vertex first;
  Vertex second;
  Weight weight;
};

[[nodiscard]] Graph make_orientation_graph(
    const std::size_t vertex_count,
    const std::vector<OrientationInputEdge>& edges) {
  Graph graph(vertex_count, false);
  for (const OrientationInputEdge& edge : edges) {
    graph.add_edge(edge.first, edge.second, edge.weight);
  }
  return graph;
}

[[nodiscard]] bool reachable_all(
    const std::vector<std::vector<Vertex>>& adjacency) {
  if (adjacency.size() <= 1U) {
    return true;
  }
  std::vector<bool> seen(adjacency.size(), false);
  std::queue<Vertex> queue;
  seen[0] = true;
  queue.push(0U);
  while (!queue.empty()) {
    const Vertex vertex = queue.front();
    queue.pop();
    for (const Vertex next : adjacency[vertex]) {
      if (!seen[next]) {
        seen[next] = true;
        queue.push(next);
      }
    }
  }
  return std::all_of(seen.begin(), seen.end(), [](const bool value) {
    return value;
  });
}

[[nodiscard]] bool undirected_connected(
    const std::size_t vertex_count,
    const std::vector<OrientationInputEdge>& edges,
    const std::optional<std::size_t> removed_edge = std::nullopt) {
  if (vertex_count <= 1U) {
    return true;
  }
  std::vector<std::vector<Vertex>> adjacency(vertex_count);
  for (std::size_t index = 0; index < edges.size(); ++index) {
    if (removed_edge.has_value() && *removed_edge == index) {
      continue;
    }
    const OrientationInputEdge& edge = edges[index];
    if (edge.first == edge.second) {
      continue;
    }
    adjacency[edge.first].push_back(edge.second);
    adjacency[edge.second].push_back(edge.first);
  }
  return reachable_all(adjacency);
}

[[nodiscard]] bool directed_strongly_connected(
    const std::size_t vertex_count,
    const std::vector<std::pair<Vertex, Vertex>>& edges) {
  if (vertex_count <= 1U) {
    return true;
  }
  std::vector<std::vector<Vertex>> forward(vertex_count);
  std::vector<std::vector<Vertex>> reverse(vertex_count);
  for (const auto& [from, to] : edges) {
    forward[from].push_back(to);
    reverse[to].push_back(from);
  }
  return reachable_all(forward) && reachable_all(reverse);
}

[[nodiscard]] bool exhaustive_strong_orientation_exists(
    const std::size_t vertex_count,
    const std::vector<OrientationInputEdge>& edges) {
  std::vector<std::size_t> non_loop_edges;
  for (std::size_t index = 0; index < edges.size(); ++index) {
    if (edges[index].first != edges[index].second) {
      non_loop_edges.push_back(index);
    }
  }
  if (non_loop_edges.size() >= 63U) {
    throw std::logic_error("test oracle received too many edges");
  }

  const std::uint64_t orientation_count =
      std::uint64_t{1} << non_loop_edges.size();
  for (std::uint64_t mask = 0; mask < orientation_count; ++mask) {
    std::vector<std::pair<Vertex, Vertex>> directed;
    directed.reserve(edges.size());
    std::size_t bit = 0U;
    for (const OrientationInputEdge& edge : edges) {
      if (edge.first == edge.second) {
        directed.emplace_back(edge.first, edge.second);
        continue;
      }
      const bool reverse = ((mask >> bit) & std::uint64_t{1}) != 0U;
      ++bit;
      directed.emplace_back(reverse ? edge.second : edge.first,
                            reverse ? edge.first : edge.second);
    }
    if (directed_strongly_connected(vertex_count, directed)) {
      return true;
    }
  }
  return false;
}

[[nodiscard]] bool witness_is_actual_bridge(
    const std::size_t vertex_count,
    const std::vector<OrientationInputEdge>& edges,
    const algorithms::graphs::StrongOrientationBridgeWitness& witness) {
  for (std::size_t index = 0; index < edges.size(); ++index) {
    const OrientationInputEdge& edge = edges[index];
    const Vertex first = std::min(edge.first, edge.second);
    const Vertex second = std::max(edge.first, edge.second);
    if (first != witness.first || second != witness.second ||
        edge.weight != witness.weight) {
      continue;
    }
    if (!undirected_connected(vertex_count, edges, index)) {
      return true;
    }
  }
  return false;
}

void verify_successful_orientation(
    const std::size_t vertex_count,
    const std::vector<OrientationInputEdge>& input,
    const StrongOrientationResult& result) {
  REQUIRE(result.orientable());
  REQUIRE_EQ(result.status, StrongOrientationStatus::success);
  REQUIRE(result.directed_graph.has_value());
  REQUIRE(!result.blocking_bridge.has_value());
  REQUIRE_EQ(result.edges.size(), input.size());
  REQUIRE_EQ(result.directed_graph->vertex_count(), vertex_count);
  REQUIRE(result.directed_graph->directed());

  std::vector<std::tuple<Vertex, Vertex, Weight>> expected;
  expected.reserve(input.size());
  for (const OrientationInputEdge& edge : input) {
    expected.emplace_back(std::min(edge.first, edge.second),
                          std::max(edge.first, edge.second), edge.weight);
  }
  std::sort(expected.begin(), expected.end());

  std::vector<std::tuple<Vertex, Vertex, Weight>> actual;
  actual.reserve(result.edges.size());
  std::vector<std::pair<Vertex, Vertex>> directed;
  directed.reserve(result.edges.size());
  std::vector<bool> seen_indices(result.edges.size(), false);
  for (const StrongOrientationEdge& edge : result.edges) {
    REQUIRE(edge.logical_edge_index < result.edges.size());
    REQUIRE(!seen_indices[edge.logical_edge_index]);
    seen_indices[edge.logical_edge_index] = true;
    REQUIRE(edge.undirected_first <= edge.undirected_second);
    REQUIRE((edge.from == edge.undirected_first &&
             edge.to == edge.undirected_second) ||
            (edge.from == edge.undirected_second &&
             edge.to == edge.undirected_first));
    actual.emplace_back(edge.undirected_first, edge.undirected_second,
                        edge.weight);
    directed.emplace_back(edge.from, edge.to);
  }
  std::sort(actual.begin(), actual.end());
  REQUIRE_EQ(actual, expected);
  REQUIRE(std::all_of(seen_indices.begin(), seen_indices.end(),
                      [](const bool value) { return value; }));
  REQUIRE(directed_strongly_connected(vertex_count, directed));

  std::size_t directed_edge_count = 0U;
  for (const auto& neighbors : result.directed_graph->adjacency()) {
    directed_edge_count += neighbors.size();
  }
  REQUIRE_EQ(directed_edge_count, input.size());
}

}  // namespace

TEST_CASE(strong_orientation_rejects_directed_and_handles_trivial_graphs) {
  Graph directed(2U, true);
  directed.add_edge(0U, 1U, 4);
  REQUIRE_THROWS_AS(algorithms::graphs::strong_orientation(directed),
                    std::invalid_argument);

  const auto empty = algorithms::graphs::strong_orientation(Graph(0U, false));
  verify_successful_orientation(0U, {}, empty);

  const std::vector<OrientationInputEdge> singleton_edges{{0U, 0U, -7}};
  const auto singleton = algorithms::graphs::strong_orientation(
      make_orientation_graph(1U, singleton_edges));
  verify_successful_orientation(1U, singleton_edges, singleton);
  REQUIRE_EQ(singleton.edges[0].from, 0U);
  REQUIRE_EQ(singleton.edges[0].to, 0U);
}

TEST_CASE(strong_orientation_reports_disconnected_and_bridge_obstructions) {
  const std::vector<OrientationInputEdge> disconnected_edges{
      {0U, 1U, 1}, {0U, 1U, 2}, {2U, 2U, 3}};
  const auto disconnected = algorithms::graphs::strong_orientation(
      make_orientation_graph(3U, disconnected_edges));
  REQUIRE(!disconnected.orientable());
  REQUIRE_EQ(disconnected.status, StrongOrientationStatus::disconnected);
  REQUIRE(disconnected.edges.empty());
  REQUIRE(!disconnected.directed_graph.has_value());
  REQUIRE(!disconnected.blocking_bridge.has_value());

  const std::vector<OrientationInputEdge> bridge_edges{
      {0U, 1U, 5}, {1U, 2U, 6}, {2U, 0U, 7}, {2U, 3U, 8}};
  const auto bridge = algorithms::graphs::strong_orientation(
      make_orientation_graph(4U, bridge_edges));
  REQUIRE(!bridge.orientable());
  REQUIRE_EQ(bridge.status, StrongOrientationStatus::bridge);
  REQUIRE(bridge.edges.empty());
  REQUIRE(!bridge.directed_graph.has_value());
  REQUIRE(bridge.blocking_bridge.has_value());
  REQUIRE(witness_is_actual_bridge(4U, bridge_edges,
                                   *bridge.blocking_bridge));
}

TEST_CASE(strong_orientation_handles_cycles_parallel_edges_and_weights) {
  const std::vector<OrientationInputEdge> triangle{
      {0U, 1U, 11}, {1U, 2U, -12}, {2U, 0U, 13}, {1U, 1U, 99}};
  const auto triangle_result = algorithms::graphs::strong_orientation(
      make_orientation_graph(3U, triangle));
  verify_successful_orientation(3U, triangle, triangle_result);

  const std::vector<OrientationInputEdge> parallel{
      {0U, 1U, -3}, {0U, 1U, 4}};
  const auto parallel_result = algorithms::graphs::strong_orientation(
      make_orientation_graph(2U, parallel));
  verify_successful_orientation(2U, parallel, parallel_result);
  REQUIRE(parallel_result.edges[0].from != parallel_result.edges[1].from);

  const auto repeated = algorithms::graphs::strong_orientation(
      make_orientation_graph(3U, triangle));
  REQUIRE_EQ(repeated.status, triangle_result.status);
  REQUIRE_EQ(repeated.edges, triangle_result.edges);
}

TEST_CASE(strong_orientation_matches_exhaustive_small_multigraph_oracle) {
  std::mt19937_64 rng(0x524F4242494E53ULL);
  std::uniform_int_distribution<int> vertex_count_dist(0, 6);
  std::uniform_int_distribution<int> edge_count_dist(0, 8);
  std::uniform_int_distribution<int> weight_dist(-9, 9);

  for (std::size_t trial = 0; trial < 600U; ++trial) {
    const std::size_t vertex_count =
        static_cast<std::size_t>(vertex_count_dist(rng));
    const std::size_t edge_count =
        vertex_count == 0U
            ? 0U
            : static_cast<std::size_t>(edge_count_dist(rng));
    std::vector<OrientationInputEdge> edges;
    edges.reserve(edge_count);
    if (vertex_count != 0U) {
      std::uniform_int_distribution<std::size_t> vertex_dist(
          0U, vertex_count - 1U);
      for (std::size_t edge = 0; edge < edge_count; ++edge) {
        edges.push_back(OrientationInputEdge{
            vertex_dist(rng), vertex_dist(rng),
            static_cast<Weight>(weight_dist(rng))});
      }
    }

    const bool connected = undirected_connected(vertex_count, edges);
    const bool exact_exists =
        exhaustive_strong_orientation_exists(vertex_count, edges);
    const auto result = algorithms::graphs::strong_orientation(
        make_orientation_graph(vertex_count, edges));

    REQUIRE_EQ(result.orientable(), exact_exists);
    if (exact_exists) {
      verify_successful_orientation(vertex_count, edges, result);
      continue;
    }

    if (!connected) {
      REQUIRE_EQ(result.status, StrongOrientationStatus::disconnected);
      REQUIRE(!result.blocking_bridge.has_value());
    } else {
      REQUIRE_EQ(result.status, StrongOrientationStatus::bridge);
      REQUIRE(result.blocking_bridge.has_value());
      REQUIRE(witness_is_actual_bridge(vertex_count, edges,
                                       *result.blocking_bridge));
    }
  }
}
