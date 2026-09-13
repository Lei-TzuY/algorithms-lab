#pragma once

#include "test_framework.hpp"

#include "algorithms/graphs/a_star.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <vector>

namespace {

using algorithms::graphs::AStarPathResult;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;
using algorithms::graphs::Weight;
using algorithms::graphs::a_star_shortest_path;

[[nodiscard]] std::uint64_t replay_a_star_path(
    const Graph& graph, const AStarPathResult& result) {
  REQUIRE(!result.path.empty());
  REQUIRE_EQ(result.path_edges.size() + 1U, result.path.size());
  std::uint64_t total = 0U;
  for (std::size_t index = 0U; index < result.path_edges.size(); ++index) {
    const auto& witness = result.path_edges[index];
    REQUIRE_EQ(witness.from, result.path[index]);
    REQUIRE_EQ(witness.to, result.path[index + 1U]);
    const auto& edge = graph.neighbors(witness.from).at(witness.adjacency_index);
    REQUIRE_EQ(edge.to, witness.to);
    REQUIRE_EQ(edge.weight, witness.weight);
    total += static_cast<std::uint64_t>(witness.weight);
  }
  return total;
}

[[nodiscard]] std::vector<std::vector<std::optional<std::uint64_t>>>
a_star_floyd_oracle(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<std::optional<std::uint64_t>>> distance(
      n, std::vector<std::optional<std::uint64_t>>(n));
  for (std::size_t vertex = 0U; vertex < n; ++vertex) {
    distance[vertex][vertex] = 0U;
  }
  for (Vertex from = 0U; from < n; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      const auto weight = static_cast<std::uint64_t>(edge.weight);
      if (!distance[from][edge.to].has_value() ||
          weight < *distance[from][edge.to]) {
        distance[from][edge.to] = weight;
      }
    }
  }
  for (Vertex middle = 0U; middle < n; ++middle) {
    for (Vertex from = 0U; from < n; ++from) {
      if (!distance[from][middle].has_value()) continue;
      for (Vertex to = 0U; to < n; ++to) {
        if (!distance[middle][to].has_value()) continue;
        const std::uint64_t candidate =
            *distance[from][middle] + *distance[middle][to];
        if (!distance[from][to].has_value() ||
            candidate < *distance[from][to]) {
          distance[from][to] = candidate;
        }
      }
    }
  }
  return distance;
}

TEST_CASE(a_star_deterministic_contract_and_witnesses) {
  Graph graph(4U, true);
  graph.add_edge(0U, 1U, 1);
  graph.add_edge(1U, 3U, 9);
  graph.add_edge(0U, 2U, 2);
  graph.add_edge(2U, 3U, 100);

  const std::vector<Weight> exact_heuristic{10, 9, 100, 0};
  const auto exact = a_star_shortest_path(graph, 0U, 3U, exact_heuristic);
  REQUIRE_EQ(exact.distance, std::optional<Weight>{10});
  REQUIRE_EQ(exact.path, (std::vector<Vertex>{0U, 1U, 3U}));
  REQUIRE_EQ(exact.expansion_order, (std::vector<Vertex>{0U, 1U, 3U}));
  REQUIRE_EQ(replay_a_star_path(graph, exact), std::uint64_t{10});

  const std::vector<Weight> zero_heuristic(4U, 0);
  const auto zero = a_star_shortest_path(graph, 0U, 3U, zero_heuristic);
  REQUIRE_EQ(zero.distance, std::optional<Weight>{10});
  REQUIRE_EQ(zero.expansion_order.size(), std::size_t{4});

  Graph parallel(3U, true);
  parallel.add_edge(0U, 1U, 5);
  parallel.add_edge(0U, 1U, 2);
  parallel.add_edge(1U, 1U, 0);
  parallel.add_edge(1U, 2U, 3);
  const std::vector<Weight> parallel_h{5, 3, 0};
  const auto parallel_result =
      a_star_shortest_path(parallel, 0U, 2U, parallel_h);
  REQUIRE_EQ(parallel_result.distance, std::optional<Weight>{5});
  REQUIRE_EQ(replay_a_star_path(parallel, parallel_result), std::uint64_t{5});
  REQUIRE_EQ(parallel_result.path_edges.front().adjacency_index, std::size_t{1});

  Graph unreachable(3U, true);
  unreachable.add_edge(0U, 1U, 1);
  const std::vector<Weight> unreachable_h(3U, 0);
  const auto none = a_star_shortest_path(unreachable, 0U, 2U, unreachable_h);
  REQUIRE(!none.distance.has_value());
  REQUIRE(none.path.empty());
  REQUIRE(none.path_edges.empty());

  const auto same = a_star_shortest_path(unreachable, 1U, 1U, unreachable_h);
  REQUIRE_EQ(same.distance, std::optional<Weight>{0});
  REQUIRE_EQ(same.path, (std::vector<Vertex>{1U}));
  REQUIRE(same.path_edges.empty());

  Graph undirected(3U, false);
  undirected.add_edge(0U, 1U, 2);
  undirected.add_edge(1U, 2U, 3);
  const std::vector<Weight> undirected_h{5, 3, 0};
  const auto undirected_first =
      a_star_shortest_path(undirected, 0U, 2U, undirected_h);
  const auto undirected_second =
      a_star_shortest_path(undirected, 0U, 2U, undirected_h);
  REQUIRE_EQ(undirected_first.distance, std::optional<Weight>{5});
  REQUIRE_EQ(replay_a_star_path(undirected, undirected_first), std::uint64_t{5});
  REQUIRE_EQ(undirected_first.path, undirected_second.path);
  REQUIRE_EQ(undirected_first.path_edges, undirected_second.path_edges);
  REQUIRE_EQ(undirected_first.expansion_order, undirected_second.expansion_order);
}

TEST_CASE(a_star_rejects_invalid_graph_or_heuristic_contracts) {
  Graph graph(3U, true);
  graph.add_edge(0U, 1U, 1);
  graph.add_edge(1U, 2U, 1);
  REQUIRE_THROWS_AS(
      a_star_shortest_path(graph, 0U, 2U, std::vector<Weight>{0, 0}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      a_star_shortest_path(graph, 0U, 2U, std::vector<Weight>{0, -1, 0}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      a_star_shortest_path(graph, 0U, 2U, std::vector<Weight>{0, 0, 1}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      a_star_shortest_path(graph, 0U, 2U, std::vector<Weight>{3, 0, 0}),
      std::invalid_argument);

  Graph negative(4U, true);
  negative.add_edge(0U, 1U, 1);
  negative.add_edge(3U, 3U, -1);
  const std::vector<Weight> zeros(4U, 0);
  REQUIRE_THROWS_AS(a_star_shortest_path(negative, 0U, 1U, zeros),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(a_star_shortest_path(graph, 3U, 2U,
                                         std::vector<Weight>(3U, 0)),
                    std::out_of_range);
}

TEST_CASE(a_star_wide_arithmetic_rejects_only_unrepresentable_optimum) {
  const Weight maximum = std::numeric_limits<Weight>::max();
  const std::vector<Weight> zeros4(4U, 0);

  Graph safe(4U, true);
  safe.add_edge(0U, 1U, maximum);
  safe.add_edge(1U, 3U, maximum);
  safe.add_edge(0U, 3U, 7);
  const auto safe_result = a_star_shortest_path(safe, 0U, 3U, zeros4);
  REQUIRE_EQ(safe_result.distance, std::optional<Weight>{7});
  REQUIRE_EQ(replay_a_star_path(safe, safe_result), std::uint64_t{7});

  Graph overflow(3U, true);
  overflow.add_edge(0U, 1U, maximum);
  overflow.add_edge(1U, 2U, maximum);
  REQUIRE_THROWS_AS(a_star_shortest_path(overflow, 0U, 2U,
                                         std::vector<Weight>(3U, 0)),
                    std::overflow_error);
}

TEST_CASE(a_star_zero_heuristic_matches_independent_floyd_warshall) {
  std::mt19937_64 rng(0xA57A57ULL);
  std::uniform_int_distribution<int> vertex_count(1, 9);
  std::uniform_int_distribution<int> chance(0, 99);
  std::uniform_int_distribution<int> edge_weight(0, 30);

  for (int trial = 0; trial < 300; ++trial) {
    const std::size_t n = static_cast<std::size_t>(vertex_count(rng));
    Graph graph(n, true);
    for (Vertex from = 0U; from < n; ++from) {
      for (Vertex to = 0U; to < n; ++to) {
        if (chance(rng) < 24) graph.add_edge(from, to, edge_weight(rng));
      }
    }
    const auto oracle = a_star_floyd_oracle(graph);
    const std::vector<Weight> heuristic(n, 0);
    for (Vertex source = 0U; source < n; ++source) {
      const Vertex target = static_cast<Vertex>(rng() % n);
      const auto result = a_star_shortest_path(graph, source, target, heuristic);
      if (!oracle[source][target].has_value()) {
        REQUIRE(!result.distance.has_value());
        REQUIRE(result.path.empty());
        continue;
      }
      REQUIRE(result.distance.has_value());
      REQUIRE_EQ(static_cast<std::uint64_t>(*result.distance),
                 *oracle[source][target]);
      REQUIRE_EQ(replay_a_star_path(graph, result), *oracle[source][target]);
    }
  }
}

TEST_CASE(a_star_exact_consistent_heuristic_matches_independent_oracle) {
  std::mt19937_64 rng(0xC0A57ULL);
  std::uniform_int_distribution<int> vertex_count(2, 10);
  std::uniform_int_distribution<int> chance(0, 99);
  std::uniform_int_distribution<int> edge_weight(1, 25);

  for (int trial = 0; trial < 300; ++trial) {
    const std::size_t n = static_cast<std::size_t>(vertex_count(rng));
    Graph graph(n, true);
    for (Vertex vertex = 0U; vertex + 1U < n; ++vertex) {
      graph.add_edge(vertex, vertex + 1U, edge_weight(rng));
    }
    for (Vertex from = 0U; from < n; ++from) {
      for (Vertex to = from + 2U; to < n; ++to) {
        if (chance(rng) < 28) graph.add_edge(from, to, edge_weight(rng));
      }
    }

    const auto oracle = a_star_floyd_oracle(graph);
    const Vertex target = n - 1U;
    std::vector<Weight> heuristic(n, 0);
    for (Vertex vertex = 0U; vertex < n; ++vertex) {
      REQUIRE(oracle[vertex][target].has_value());
      heuristic[vertex] = static_cast<Weight>(*oracle[vertex][target]);
    }

    for (Vertex source = 0U; source < n; ++source) {
      const auto result = a_star_shortest_path(graph, source, target, heuristic);
      REQUIRE(result.distance.has_value());
      REQUIRE_EQ(static_cast<std::uint64_t>(*result.distance),
                 *oracle[source][target]);
      REQUIRE_EQ(replay_a_star_path(graph, result), *oracle[source][target]);
      std::vector<unsigned char> seen(n, 0U);
      for (const Vertex vertex : result.expansion_order) {
        REQUIRE_EQ(seen[vertex], static_cast<unsigned char>(0U));
        seen[vertex] = 1U;
      }
      REQUIRE(!result.expansion_order.empty());
      REQUIRE_EQ(result.expansion_order.back(), target);
    }
  }
}

}  // namespace
