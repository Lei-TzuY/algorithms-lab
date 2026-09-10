#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/shortest_paths.hpp"

using namespace algorithms::graphs;

namespace {

using DistanceMatrix =
    std::vector<std::vector<std::optional<Weight>>>;

DistanceMatrix floyd_warshall_oracle(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  DistanceMatrix distance(
      n, std::vector<std::optional<Weight>>(n));
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    distance[vertex][vertex] = Weight{0};
  }
  for (Vertex from = 0; from < n; ++from) {
    for (const Edge& edge : graph.neighbors(from)) {
      if (!distance[from][edge.to].has_value() ||
          edge.weight < *distance[from][edge.to]) {
        distance[from][edge.to] = edge.weight;
      }
    }
  }

  for (Vertex via = 0; via < n; ++via) {
    for (Vertex from = 0; from < n; ++from) {
      if (!distance[from][via].has_value()) {
        continue;
      }
      for (Vertex to = 0; to < n; ++to) {
        if (!distance[via][to].has_value()) {
          continue;
        }
        const Weight left = *distance[from][via];
        const Weight right = *distance[via][to];
        if ((right > 0 &&
             left > std::numeric_limits<Weight>::max() - right) ||
            (right < 0 &&
             left < std::numeric_limits<Weight>::min() - right)) {
          throw std::logic_error("test Floyd-Warshall oracle overflow");
        }
        const Weight candidate = left + right;
        if (!distance[from][to].has_value() ||
            candidate < *distance[from][to]) {
          distance[from][to] = candidate;
        }
      }
    }
  }
  return distance;
}

void verify_parent_witness(const Graph& graph,
                           const AllPairsShortestPathResult& result) {
  for (Vertex source = 0; source < graph.vertex_count(); ++source) {
    for (Vertex target = 0; target < graph.vertex_count(); ++target) {
      if (!result.distance[source][target].has_value()) {
        REQUIRE(!result.parent[source][target].has_value());
        continue;
      }
      if (source == target) {
        REQUIRE(!result.parent[source][target].has_value());
        continue;
      }

      Vertex current = target;
      std::size_t steps = 0;
      while (current != source) {
        REQUIRE(steps < graph.vertex_count());
        REQUIRE(result.parent[source][current].has_value());
        const Vertex parent = *result.parent[source][current];
        REQUIRE(result.distance[source][parent].has_value());

        bool edge_witness = false;
        for (const Edge& edge : graph.neighbors(parent)) {
          if (edge.to == current &&
              *result.distance[source][parent] + edge.weight ==
                  *result.distance[source][current]) {
            edge_witness = true;
            break;
          }
        }
        REQUIRE(edge_witness);
        current = parent;
        ++steps;
      }
    }
  }
}

}  // namespace

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

TEST_CASE(johnson_classic_negative_edges_and_unreachable_pairs) {
  Graph graph(6, true);
  graph.add_edge(0, 1, 3);
  graph.add_edge(0, 2, 8);
  graph.add_edge(0, 4, -4);
  graph.add_edge(1, 3, 1);
  graph.add_edge(1, 4, 7);
  graph.add_edge(2, 1, 4);
  graph.add_edge(3, 0, 2);
  graph.add_edge(3, 2, -5);
  graph.add_edge(4, 3, 6);

  const auto result = johnson_all_pairs_shortest_paths(graph);
  REQUIRE(result.has_value());
  const std::vector<std::vector<Weight>> expected{
      {0, 1, -3, 2, -4},
      {3, 0, -4, 1, -1},
      {7, 4, 0, 5, 3},
      {2, -1, -5, 0, -2},
      {8, 5, 1, 6, 0},
  };
  for (Vertex from = 0; from < 5U; ++from) {
    for (Vertex to = 0; to < 5U; ++to) {
      REQUIRE(result->distance[from][to].has_value());
      REQUIRE_EQ(*result->distance[from][to], expected[from][to]);
    }
    REQUIRE(!result->distance[from][5].has_value());
  }
  REQUIRE_EQ(*result->distance[5][5], Weight{0});
  for (Vertex to = 0; to < 5U; ++to) {
    REQUIRE(!result->distance[5][to].has_value());
  }
  verify_parent_witness(graph, *result);
}

TEST_CASE(johnson_detects_negative_cycle_anywhere) {
  Graph graph(5, true);
  graph.add_edge(0, 1, 4);
  graph.add_edge(2, 3, -2);
  graph.add_edge(3, 2, 1);
  REQUIRE(!johnson_all_pairs_shortest_paths(graph).has_value());

  Graph negative_self_loop(2, true);
  negative_self_loop.add_edge(1, 1, -1);
  REQUIRE(!johnson_all_pairs_shortest_paths(negative_self_loop).has_value());

  Graph undirected_negative_edge(2, false);
  undirected_negative_edge.add_edge(0, 1, -1);
  REQUIRE(!johnson_all_pairs_shortest_paths(undirected_negative_edge).has_value());
}

TEST_CASE(johnson_empty_extremes_and_overflow_boundaries) {
  const auto empty = johnson_all_pairs_shortest_paths(Graph(0, true));
  REQUIRE(empty.has_value());
  REQUIRE(empty->distance.empty());
  REQUIRE(empty->parent.empty());

  Graph min_edge(2, true);
  min_edge.add_edge(0, 1, std::numeric_limits<Weight>::min());
  const auto min_result = johnson_all_pairs_shortest_paths(min_edge);
  REQUIRE(min_result.has_value());
  REQUIRE_EQ(*min_result->distance[0][1],
             std::numeric_limits<Weight>::min());

  Graph max_reweighted_edge(3, true);
  max_reweighted_edge.add_edge(0, 2, std::numeric_limits<Weight>::min());
  max_reweighted_edge.add_edge(1, 2, std::numeric_limits<Weight>::max());
  const auto max_result = johnson_all_pairs_shortest_paths(max_reweighted_edge);
  REQUIRE(max_result.has_value());
  REQUIRE_EQ(*max_result->distance[1][2],
             std::numeric_limits<Weight>::max());

  Graph positive_overflow(3, true);
  positive_overflow.add_edge(0, 1, std::numeric_limits<Weight>::max());
  positive_overflow.add_edge(1, 2, 1);
  REQUIRE_THROWS_AS(johnson_all_pairs_shortest_paths(positive_overflow),
                    std::overflow_error);

  Graph negative_underflow(3, true);
  negative_underflow.add_edge(0, 1, std::numeric_limits<Weight>::min());
  negative_underflow.add_edge(1, 2, -1);
  REQUIRE_THROWS_AS(johnson_all_pairs_shortest_paths(negative_underflow),
                    std::overflow_error);
}

TEST_CASE(johnson_randomized_dag_differential_against_floyd_warshall) {
  std::mt19937 rng(0xA11FA1A5u);
  std::uniform_int_distribution<int> vertex_count_dist(1, 8);
  std::uniform_int_distribution<int> weight_dist(-9, 15);
  std::bernoulli_distribution edge_dist(0.38);
  std::bernoulli_distribution parallel_dist(0.14);

  for (int trial = 0; trial < 700; ++trial) {
    const std::size_t n = static_cast<std::size_t>(vertex_count_dist(rng));
    Graph graph(n, true);
    for (Vertex from = 0; from < n; ++from) {
      for (Vertex to = from + 1U; to < n; ++to) {
        if (!edge_dist(rng)) {
          continue;
        }
        graph.add_edge(from, to, static_cast<Weight>(weight_dist(rng)));
        if (parallel_dist(rng)) {
          graph.add_edge(from, to, static_cast<Weight>(weight_dist(rng)));
        }
      }
    }

    const auto actual = johnson_all_pairs_shortest_paths(graph);
    REQUIRE(actual.has_value());
    REQUIRE_EQ(actual->distance, floyd_warshall_oracle(graph));
    verify_parent_witness(graph, *actual);
  }
}

TEST_CASE(johnson_randomized_cyclic_potential_constructed_differential) {
  std::mt19937 rng(0xC1C1E5A5u);
  std::uniform_int_distribution<int> vertex_count_dist(1, 9);
  std::uniform_int_distribution<int> potential_dist(-8, 0);
  std::uniform_int_distribution<int> reduced_weight_dist(0, 12);
  std::bernoulli_distribution edge_dist(0.28);
  std::bernoulli_distribution parallel_dist(0.10);
  std::bernoulli_distribution self_loop_dist(0.12);

  for (int trial = 0; trial < 450; ++trial) {
    const std::size_t n = static_cast<std::size_t>(vertex_count_dist(rng));
    std::vector<Weight> generating_potential(n);
    for (Weight& value : generating_potential) {
      value = static_cast<Weight>(potential_dist(rng));
    }

    Graph graph(n, true);
    for (Vertex from = 0; from < n; ++from) {
      for (Vertex to = 0; to < n; ++to) {
        if (from == to && !self_loop_dist(rng)) {
          continue;
        }
        if (!edge_dist(rng)) {
          continue;
        }

        const auto add_generated_edge = [&]() {
          const Weight reduced =
              static_cast<Weight>(reduced_weight_dist(rng));
          // reduced = original + h[from] - h[to], so constructing
          // original = reduced - h[from] + h[to] guarantees every
          // generated cycle has non-negative total reduced weight and thus
          // no negative original cycle. Values stay in [-8, 20].
          const Weight original =
              reduced - generating_potential[from] + generating_potential[to];
          graph.add_edge(from, to, original);
        };

        add_generated_edge();
        if (parallel_dist(rng)) {
          add_generated_edge();
        }
      }
    }

    const auto actual = johnson_all_pairs_shortest_paths(graph);
    REQUIRE(actual.has_value());
    REQUIRE_EQ(actual->distance, floyd_warshall_oracle(graph));
    verify_parent_witness(graph, *actual);
  }
}

TEST_CASE(johnson_nonnegative_cross_checks_every_source_with_dijkstra) {
  std::mt19937 rng(0xD1057A11u);
  std::uniform_int_distribution<int> vertex_count_dist(1, 10);
  std::uniform_int_distribution<int> weight_dist(0, 25);
  std::bernoulli_distribution directed_dist(0.7);
  std::bernoulli_distribution edge_dist(0.22);
  std::bernoulli_distribution parallel_dist(0.08);
  std::bernoulli_distribution self_loop_dist(0.12);

  for (int trial = 0; trial < 250; ++trial) {
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

    const auto actual = johnson_all_pairs_shortest_paths(graph);
    REQUIRE(actual.has_value());
    for (Vertex source = 0; source < n; ++source) {
      REQUIRE_EQ(actual->distance[source], dijkstra(graph, source).distance);
    }
  }
}
