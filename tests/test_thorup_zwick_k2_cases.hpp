#pragma once

#include "algorithms/graphs/thorup_zwick_k2.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::graphs::Graph;
using algorithms::graphs::ThorupZwickK2Oracle;
using algorithms::graphs::Vertex;
using algorithms::graphs::Weight;

namespace thorup_zwick_k2_test_detail {

using DistanceMatrix =
    std::vector<std::vector<std::optional<Weight>>>;

inline DistanceMatrix brute_apsp(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  DistanceMatrix distance(
      n, std::vector<std::optional<Weight>>(n));

  for (Vertex vertex = 0U; vertex < n; ++vertex) {
    distance[vertex][vertex] = Weight{0};
    for (const auto& edge : graph.neighbors(vertex)) {
      if (!distance[vertex][edge.to].has_value() ||
          edge.weight < *distance[vertex][edge.to]) {
        distance[vertex][edge.to] = edge.weight;
      }
    }
  }

  for (Vertex middle = 0U; middle < n; ++middle) {
    for (Vertex source = 0U; source < n; ++source) {
      if (!distance[source][middle].has_value()) {
        continue;
      }
      for (Vertex target = 0U; target < n; ++target) {
        if (!distance[middle][target].has_value()) {
          continue;
        }

        const Weight first = *distance[source][middle];
        const Weight second = *distance[middle][target];
        if (first >
            std::numeric_limits<Weight>::max() - second) {
          continue;
        }

        const Weight candidate = first + second;
        if (!distance[source][target].has_value() ||
            candidate < *distance[source][target]) {
          distance[source][target] = candidate;
        }
      }
    }
  }

  return distance;
}

inline void require_oracle_bounds(
    const Graph& graph, const ThorupZwickK2Oracle& oracle) {
  const DistanceMatrix exact = brute_apsp(graph);
  const std::size_t n = graph.vertex_count();

  for (Vertex first = 0U; first < n; ++first) {
    for (Vertex second = 0U; second < n; ++second) {
      const auto estimate = oracle.query(first, second);
      const auto reverse = oracle.query(second, first);
      REQUIRE(estimate == reverse);

      if (!exact[first][second].has_value()) {
        REQUIRE(!estimate.has_value());
        continue;
      }

      REQUIRE(estimate.has_value());
      REQUIRE(*estimate >= *exact[first][second]);

      const Weight distance = *exact[first][second];
      REQUIRE(distance <=
              std::numeric_limits<Weight>::max() / 3);
      REQUIRE(*estimate <= 3 * distance);
    }
  }
}

inline Graph random_graph(
    std::mt19937_64& random, const std::size_t n) {
  Graph graph(n, false);

  for (Vertex first = 0U; first < n; ++first) {
    if ((random() % 7U) == 0U) {
      graph.add_edge(
          first, first,
          static_cast<Weight>(random() % 5U));
    }

    for (Vertex second = first + 1U;
         second < n; ++second) {
      if ((random() % 100U) < 38U) {
        graph.add_edge(
            first, second,
            static_cast<Weight>(random() % 13U));
        if ((random() % 9U) == 0U) {
          graph.add_edge(
              first, second,
              static_cast<Weight>(random() % 13U));
        }
      }
    }
  }

  return graph;
}

}  // namespace thorup_zwick_k2_test_detail

TEST_CASE(thorup_zwick_k2_empty_and_singleton_contract) {
  const Graph empty(0U, false);
  const ThorupZwickK2Oracle empty_oracle(empty, 7U);

  REQUIRE_EQ(empty_oracle.vertex_count(), 0U);
  REQUIRE_EQ(empty_oracle.landmark_count(), 0U);
  REQUIRE_THROWS_AS(empty_oracle.query(0U, 0U),
                    std::out_of_range);

  const Graph singleton(1U, false);
  const ThorupZwickK2Oracle singleton_oracle(singleton, 7U);

  REQUIRE_EQ(singleton_oracle.vertex_count(), 1U);
  REQUIRE_EQ(singleton_oracle.landmark_count(), 1U);
  REQUIRE(singleton_oracle.is_landmark(0U));
  REQUIRE_EQ(singleton_oracle.pivot(0U), 0U);
  REQUIRE_EQ(singleton_oracle.pivot_distance(0U), 0);
  REQUIRE(singleton_oracle.query(0U, 0U) ==
          std::optional<Weight>(0));
}

TEST_CASE(thorup_zwick_k2_rejects_directed_and_negative_graphs) {
  Graph directed(2U, true);
  directed.add_edge(0U, 1U, 1);
  REQUIRE_THROWS_AS(
      ThorupZwickK2Oracle(directed),
      std::invalid_argument);

  Graph negative(2U, false);
  negative.add_edge(0U, 1U, -1);
  REQUIRE_THROWS_AS(
      ThorupZwickK2Oracle(negative),
      std::invalid_argument);
}

TEST_CASE(thorup_zwick_k2_disconnected_components_force_landmarks) {
  using namespace thorup_zwick_k2_test_detail;

  Graph graph(6U, false);
  graph.add_edge(0U, 1U, 4);
  graph.add_edge(1U, 2U, 3);
  graph.add_edge(3U, 4U, 0);
  // Vertex 5 is an isolated third component.

  const ThorupZwickK2Oracle oracle(graph, UINT64_C(0xD15C0));
  REQUIRE(oracle.landmark_count() >= 3U);

  REQUIRE(!oracle.query(0U, 3U).has_value());
  REQUIRE(!oracle.query(2U, 5U).has_value());
  REQUIRE(oracle.query(3U, 4U) ==
          std::optional<Weight>(0));

  require_oracle_bounds(graph, oracle);
}

TEST_CASE(thorup_zwick_k2_parallel_zero_and_self_loop_edges) {
  using namespace thorup_zwick_k2_test_detail;

  Graph graph(5U, false);
  graph.add_edge(0U, 1U, 9);
  graph.add_edge(0U, 1U, 2);
  graph.add_edge(1U, 2U, 0);
  graph.add_edge(2U, 3U, 5);
  graph.add_edge(3U, 4U, 1);
  graph.add_edge(2U, 2U, 0);

  const ThorupZwickK2Oracle oracle(graph, UINT64_C(0xB00C));
  require_oracle_bounds(graph, oracle);
}

TEST_CASE(thorup_zwick_k2_same_seed_is_structurally_deterministic) {
  Graph graph(10U, false);
  for (Vertex vertex = 1U; vertex < 10U; ++vertex) {
    graph.add_edge(
        vertex - 1U, vertex,
        static_cast<Weight>(1U + (vertex % 4U)));
  }
  graph.add_edge(0U, 9U, 7);
  graph.add_edge(2U, 8U, 3);

  const ThorupZwickK2Oracle first(
      graph, UINT64_C(0x123456789ABCDEF0));
  const ThorupZwickK2Oracle second(
      graph, UINT64_C(0x123456789ABCDEF0));

  REQUIRE(first.landmarks() == second.landmarks());
  REQUIRE_EQ(first.landmark_count(), second.landmark_count());

  for (Vertex vertex = 0U; vertex < 10U; ++vertex) {
    REQUIRE_EQ(first.pivot(vertex), second.pivot(vertex));
    REQUIRE_EQ(first.pivot_distance(vertex),
               second.pivot_distance(vertex));
    REQUIRE_EQ(first.bunch_size(vertex),
               second.bunch_size(vertex));
  }

  for (Vertex u = 0U; u < 10U; ++u) {
    for (Vertex v = 0U; v < 10U; ++v) {
      REQUIRE(first.query(u, v) == second.query(u, v));
    }
  }
}

TEST_CASE(thorup_zwick_k2_random_small_graphs_respect_three_stretch) {
  using namespace thorup_zwick_k2_test_detail;

  std::mt19937_64 random(UINT64_C(0x7A4F2B19D3C5E801));

  for (std::size_t trial = 0U; trial < 180U; ++trial) {
    const std::size_t n =
        1U + static_cast<std::size_t>(random() % 9U);
    const Graph graph = random_graph(random, n);

    for (std::size_t seed_trial = 0U;
         seed_trial < 4U; ++seed_trial) {
      const std::uint64_t seed = random();
      const ThorupZwickK2Oracle oracle(graph, seed);

      REQUIRE_EQ(oracle.vertex_count(), n);
      REQUIRE(oracle.landmark_count() >= 1U);
      require_oracle_bounds(graph, oracle);
    }
  }
}
