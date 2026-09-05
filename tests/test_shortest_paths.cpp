#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>

#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/shortest_paths.hpp"

using namespace algorithms::graphs;

TEST_CASE(dijkstra_zero_weight_parallel_and_unreachable) {
  Graph graph(6, true);
  graph.add_edge(0, 1, 5);
  graph.add_edge(0, 1, 2);  // parallel edge
  graph.add_edge(1, 2, 0);  // zero-weight edge
  graph.add_edge(0, 3, 10);
  graph.add_edge(2, 3, 1);
  graph.add_edge(3, 3, 0);  // self-loop

  const auto result = dijkstra(graph, 0);
  REQUIRE_EQ(*result.distance[0], Weight{0});
  REQUIRE_EQ(*result.distance[1], Weight{2});
  REQUIRE_EQ(*result.distance[2], Weight{2});
  REQUIRE_EQ(*result.distance[3], Weight{3});
  REQUIRE(!result.distance[4].has_value());
  REQUIRE(!result.distance[5].has_value());
}

TEST_CASE(dijkstra_rejects_any_negative_edge) {
  Graph graph(4, true);
  graph.add_edge(0, 1, 2);
  graph.add_edge(2, 3, -1);  // unreachable from source, still forbidden
  REQUIRE_THROWS_AS(dijkstra(graph, 0), std::invalid_argument);
}

TEST_CASE(bellman_ford_negative_edges_and_negative_cycle_detection) {
  Graph graph(5, true);
  graph.add_edge(0, 1, 4);
  graph.add_edge(0, 2, 5);
  graph.add_edge(1, 2, -3);
  graph.add_edge(2, 3, 2);

  const auto result = bellman_ford(graph, 0);
  REQUIRE(!result.has_reachable_negative_cycle);
  REQUIRE_EQ(*result.distance[2], Weight{1});
  REQUIRE_EQ(*result.distance[3], Weight{3});
  REQUIRE(!result.distance[4].has_value());

  Graph negative_cycle(4, true);
  negative_cycle.add_edge(0, 1, 1);
  negative_cycle.add_edge(1, 2, -2);
  negative_cycle.add_edge(2, 1, -2);
  negative_cycle.add_edge(2, 3, 1);
  REQUIRE(bellman_ford(negative_cycle, 0).has_reachable_negative_cycle);

  Graph unreachable_negative_cycle(4, true);
  unreachable_negative_cycle.add_edge(0, 1, 3);
  unreachable_negative_cycle.add_edge(2, 3, -2);
  unreachable_negative_cycle.add_edge(3, 2, -2);
  REQUIRE(!bellman_ford(unreachable_negative_cycle, 0)
               .has_reachable_negative_cycle);
}

TEST_CASE(randomized_dijkstra_bellman_ford_differential) {
  std::mt19937 rng(0xD1FF3A3u);
  std::uniform_int_distribution<int> vertex_count_dist(1, 25);
  std::uniform_int_distribution<int> weight_dist(0, 30);
  std::bernoulli_distribution directed_dist(0.6);
  std::bernoulli_distribution edge_dist(0.16);
  std::bernoulli_distribution parallel_dist(0.08);
  std::bernoulli_distribution self_loop_dist(0.15);

  for (int trial = 0; trial < 400; ++trial) {
    const std::size_t n = static_cast<std::size_t>(vertex_count_dist(rng));
    Graph graph(n, directed_dist(rng));

    for (Vertex from = 0; from < n; ++from) {
      for (Vertex to = 0; to < n; ++to) {
        if (from == to && !self_loop_dist(rng)) {
          continue;
        }
        if (!edge_dist(rng)) {
          continue;
        }
        graph.add_edge(from, to, static_cast<Weight>(weight_dist(rng)));
        if (parallel_dist(rng)) {
          graph.add_edge(from, to, static_cast<Weight>(weight_dist(rng)));
        }
      }
    }

    std::uniform_int_distribution<std::size_t> source_dist(0, n - 1);
    const Vertex source = source_dist(rng);
    const auto dijkstra_result = dijkstra(graph, source);
    const auto bellman_result = bellman_ford(graph, source);
    REQUIRE(!bellman_result.has_reachable_negative_cycle);
    REQUIRE_EQ(dijkstra_result.distance, bellman_result.distance);
  }
}
