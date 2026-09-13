#pragma once

#include "algorithms/graphs/greedy_spanner.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace greedy_spanner_test_detail {

using algorithms::graphs::Edge;
using algorithms::graphs::Graph;
using algorithms::graphs::GreedySpannerEdgeWitness;
using algorithms::graphs::Vertex;
using algorithms::graphs::Weight;

struct LogicalEdge {
  Vertex from;
  std::size_t adjacency_index;
  Vertex to;
  Weight weight;
};

[[nodiscard]] inline std::vector<LogicalEdge> logical_edges(const Graph& graph) {
  std::vector<LogicalEdge> edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    const auto& adjacency = graph.neighbors(from);
    for (std::size_t index = 0; index < adjacency.size(); ++index) {
      if (from <= adjacency[index].to) {
        edges.push_back(LogicalEdge{from, index, adjacency[index].to,
                                    adjacency[index].weight});
      }
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

using Distance = std::optional<std::uint64_t>;
using DistanceMatrix = std::vector<std::vector<Distance>>;

[[nodiscard]] inline DistanceMatrix all_pairs(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  DistanceMatrix distance(n, std::vector<Distance>(n));
  for (std::size_t vertex = 0; vertex < n; ++vertex) {
    distance[vertex][vertex] = 0;
  }
  for (Vertex from = 0; from < n; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      REQUIRE(edge.weight >= 0);
      const auto weight = static_cast<std::uint64_t>(edge.weight);
      if (!distance[from][edge.to].has_value() ||
          weight < *distance[from][edge.to]) {
        distance[from][edge.to] = weight;
      }
    }
  }
  for (std::size_t via = 0; via < n; ++via) {
    for (std::size_t from = 0; from < n; ++from) {
      if (!distance[from][via].has_value()) {
        continue;
      }
      for (std::size_t to = 0; to < n; ++to) {
        if (!distance[via][to].has_value()) {
          continue;
        }
        const std::uint64_t left = *distance[from][via];
        const std::uint64_t right = *distance[via][to];
        REQUIRE(left <= std::numeric_limits<std::uint64_t>::max() - right);
        const std::uint64_t candidate = left + right;
        if (!distance[from][to].has_value() || candidate < *distance[from][to]) {
          distance[from][to] = candidate;
        }
      }
    }
  }
  return distance;
}

[[nodiscard]] inline std::vector<GreedySpannerEdgeWitness> oracle_selected(
    const Graph& graph, std::uint64_t stretch) {
  Graph partial(graph.vertex_count(), false);
  std::vector<GreedySpannerEdgeWitness> selected;
  for (const LogicalEdge& edge : logical_edges(graph)) {
    if (edge.from == edge.to) {
      continue;
    }
    const auto threshold =
        stretch * static_cast<std::uint64_t>(edge.weight);
    const auto distance = all_pairs(partial);
    if (distance[edge.from][edge.to].has_value() &&
        *distance[edge.from][edge.to] <= threshold) {
      continue;
    }
    partial.add_edge(edge.from, edge.to, edge.weight);
    selected.push_back(GreedySpannerEdgeWitness{
        edge.from, edge.adjacency_index, edge.to, edge.weight});
  }
  return selected;
}

inline void verify_replay(const Graph& original,
                          const algorithms::graphs::GreedySpannerResult& result) {
  Graph replay(original.vertex_count(), false);
  for (const GreedySpannerEdgeWitness& witness : result.selected_edges) {
    REQUIRE(witness.from < original.vertex_count());
    const auto& adjacency = original.neighbors(witness.from);
    REQUIRE(witness.adjacency_index < adjacency.size());
    const Edge& source = adjacency[witness.adjacency_index];
    REQUIRE_EQ(source.to, witness.to);
    REQUIRE_EQ(source.weight, witness.weight);
    REQUIRE(witness.from != witness.to);
    replay.add_edge(witness.from, witness.to, witness.weight);
  }
  REQUIRE_EQ(replay.adjacency(), result.spanner.adjacency());
}

inline void verify_stretch(const Graph& original,
                           const algorithms::graphs::GreedySpannerResult& result) {
  const DistanceMatrix original_distance = all_pairs(original);
  const DistanceMatrix spanner_distance = all_pairs(result.spanner);
  for (std::size_t from = 0; from < original.vertex_count(); ++from) {
    for (std::size_t to = 0; to < original.vertex_count(); ++to) {
      REQUIRE_EQ(spanner_distance[from][to].has_value(),
                 original_distance[from][to].has_value());
      if (!original_distance[from][to].has_value()) {
        continue;
      }
      REQUIRE(*original_distance[from][to] <= *spanner_distance[from][to]);
      REQUIRE(*original_distance[from][to] <=
              std::numeric_limits<std::uint64_t>::max() / result.stretch);
      REQUIRE(*spanner_distance[from][to] <=
              result.stretch * *original_distance[from][to]);
    }
  }
}

}  // namespace greedy_spanner_test_detail

TEST_CASE(greedy_spanner_validation_and_boundaries) {
  using algorithms::graphs::Graph;
  using algorithms::graphs::greedy_weighted_spanner;

  Graph empty(0, false);
  const auto empty_result = greedy_weighted_spanner(empty, 3);
  REQUIRE_EQ(empty_result.logical_input_edge_count, std::size_t{0});
  REQUIRE(empty_result.selected_edges.empty());
  REQUIRE_EQ(empty_result.spanner.vertex_count(), std::size_t{0});

  Graph singleton(1, false);
  singleton.add_edge(0, 0, 7);
  const auto singleton_result = greedy_weighted_spanner(singleton, 4);
  REQUIRE_EQ(singleton_result.logical_input_edge_count, std::size_t{1});
  REQUIRE_EQ(singleton_result.considered_non_loop_edge_count, std::size_t{0});
  REQUIRE(singleton_result.selected_edges.empty());

  Graph directed(2, true);
  directed.add_edge(0, 1, 1);
  REQUIRE_THROWS_AS(greedy_weighted_spanner(directed, 2), std::invalid_argument);
  REQUIRE_THROWS_AS(greedy_weighted_spanner(empty, 0), std::invalid_argument);

  Graph negative(2, false);
  negative.add_edge(0, 1, -1);
  REQUIRE_THROWS_AS(greedy_weighted_spanner(negative, 1), std::invalid_argument);

  Graph negative_loop(1, false);
  negative_loop.add_edge(0, 0, -1);
  REQUIRE_THROWS_AS(greedy_weighted_spanner(negative_loop, 1),
                    std::invalid_argument);
}

TEST_CASE(greedy_spanner_deterministic_selection_and_multigraph_semantics) {
  using algorithms::graphs::Graph;
  using algorithms::graphs::greedy_weighted_spanner;

  Graph triangle(3, false);
  triangle.add_edge(0, 1, 2);
  triangle.add_edge(1, 2, 2);
  triangle.add_edge(0, 2, 3);

  const auto exact = greedy_weighted_spanner(triangle, 1);
  REQUIRE_EQ(exact.selected_edges.size(), std::size_t{3});
  const auto stretched = greedy_weighted_spanner(triangle, 2);
  REQUIRE_EQ(stretched.selected_edges.size(), std::size_t{2});
  REQUIRE_EQ(stretched.selected_edges,
             greedy_spanner_test_detail::oracle_selected(triangle, 2));
  greedy_spanner_test_detail::verify_replay(triangle, stretched);
  greedy_spanner_test_detail::verify_stretch(triangle, stretched);

  Graph multigraph(3, false);
  multigraph.add_edge(0, 0, 0);
  multigraph.add_edge(0, 1, 5);
  multigraph.add_edge(1, 0, 2);
  multigraph.add_edge(1, 2, 3);
  multigraph.add_edge(0, 2, 20);
  const auto result = greedy_weighted_spanner(multigraph, 2);
  REQUIRE_EQ(result.logical_input_edge_count, std::size_t{5});
  REQUIRE_EQ(result.considered_non_loop_edge_count, std::size_t{4});
  REQUIRE_EQ(result.selected_edges,
             greedy_spanner_test_detail::oracle_selected(multigraph, 2));
  REQUIRE_EQ(result.selected_edges.size(), std::size_t{2});
  REQUIRE_EQ(result.selected_edges[0].weight, 2);
  REQUIRE_EQ(result.selected_edges[1].weight, 3);
  greedy_spanner_test_detail::verify_replay(multigraph, result);
  greedy_spanner_test_detail::verify_stretch(multigraph, result);
}

TEST_CASE(greedy_spanner_checked_threshold_and_replay) {
  using algorithms::graphs::Graph;
  using algorithms::graphs::greedy_weighted_spanner;

  Graph boundary(2, false);
  boundary.add_edge(0, 1, std::numeric_limits<std::int64_t>::max());
  const auto exact = greedy_weighted_spanner(boundary, 1);
  REQUIRE_EQ(exact.selected_edges.size(), std::size_t{1});
  REQUIRE_EQ(exact.selected_edges.front().weight,
             std::numeric_limits<std::int64_t>::max());
  REQUIRE_THROWS_AS(greedy_weighted_spanner(boundary, 2), std::overflow_error);

  Graph zero(2, false);
  zero.add_edge(0, 1, 0);
  const auto huge = greedy_weighted_spanner(
      zero, std::numeric_limits<std::uint64_t>::max());
  REQUIRE_EQ(huge.selected_edges.size(), std::size_t{1});
  REQUIRE_EQ(huge.selected_edges.front().weight, 0);
  REQUIRE_EQ(huge.selected_edges,
             greedy_weighted_spanner(
                 zero, std::numeric_limits<std::uint64_t>::max())
                 .selected_edges);
}

TEST_CASE(greedy_spanner_randomized_differential_and_stretch_guarantee) {
  using algorithms::graphs::Graph;
  using algorithms::graphs::greedy_weighted_spanner;

  std::mt19937_64 random(0x5A9A77E2ULL);
  for (std::size_t trial = 0; trial < 600; ++trial) {
    const std::size_t vertex_count = static_cast<std::size_t>(random() % 9U);
    Graph graph(vertex_count, false);
    const std::size_t edge_count = vertex_count == 0
                                       ? 0
                                       : static_cast<std::size_t>(random() % 22U);
    for (std::size_t edge = 0; edge < edge_count; ++edge) {
      const auto from = static_cast<std::size_t>(random() % vertex_count);
      const auto to = static_cast<std::size_t>(random() % vertex_count);
      const auto weight = static_cast<std::int64_t>(random() % 21U);
      graph.add_edge(from, to, weight);
    }
    const std::uint64_t stretch = 1U + (random() % 5U);
    const auto result = greedy_weighted_spanner(graph, stretch);

    REQUIRE_EQ(result.logical_input_edge_count,
               greedy_spanner_test_detail::logical_edges(graph).size());
    REQUIRE_EQ(result.selected_edges,
               greedy_spanner_test_detail::oracle_selected(graph, stretch));
    greedy_spanner_test_detail::verify_replay(graph, result);
    greedy_spanner_test_detail::verify_stretch(graph, result);

    const auto final_distance =
        greedy_spanner_test_detail::all_pairs(result.spanner);
    for (const auto& edge : greedy_spanner_test_detail::logical_edges(graph)) {
      if (edge.from == edge.to) {
        continue;
      }
      REQUIRE(final_distance[edge.from][edge.to].has_value());
      const auto threshold =
          stretch * static_cast<std::uint64_t>(edge.weight);
      REQUIRE(*final_distance[edge.from][edge.to] <= threshold);
    }
  }
}
