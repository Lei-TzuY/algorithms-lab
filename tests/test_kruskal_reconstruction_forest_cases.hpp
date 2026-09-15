#pragma once

#include "algorithms/graphs/kruskal_reconstruction_forest.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <queue>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using algorithms::graphs::Graph;
using algorithms::graphs::KruskalReconstructionForest;
using algorithms::graphs::MinimaxConnectivityResult;
using algorithms::graphs::Vertex;
using algorithms::graphs::Weight;

struct KruskalOracleEdge {
  Vertex first;
  Vertex second;
  Weight weight;
};

std::vector<KruskalOracleEdge> kruskal_oracle_edges(const Graph& graph) {
  std::vector<KruskalOracleEdge> edges;
  for (Vertex from = 0; from < graph.vertex_count(); ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (from < edge.to) {
        edges.push_back(KruskalOracleEdge{from, edge.to, edge.weight});
      }
    }
  }
  return edges;
}

std::vector<bool> kruskal_oracle_reachable_at(const Graph& graph, Vertex start,
                                               Weight threshold) {
  std::vector<bool> seen(graph.vertex_count(), false);
  std::queue<Vertex> pending;
  seen[start] = true;
  pending.push(start);
  while (!pending.empty()) {
    const Vertex from = pending.front();
    pending.pop();
    for (const auto& edge : graph.neighbors(from)) {
      if (edge.weight <= threshold && !seen[edge.to]) {
        seen[edge.to] = true;
        pending.push(edge.to);
      }
    }
  }
  return seen;
}

std::optional<Weight> kruskal_oracle_bottleneck(const Graph& graph,
                                                 Vertex first,
                                                 Vertex second) {
  if (first == second) {
    return std::nullopt;
  }
  std::vector<Weight> thresholds;
  for (const auto& edge : kruskal_oracle_edges(graph)) {
    thresholds.push_back(edge.weight);
  }
  std::sort(thresholds.begin(), thresholds.end());
  thresholds.erase(std::unique(thresholds.begin(), thresholds.end()),
                   thresholds.end());
  for (const Weight threshold : thresholds) {
    if (kruskal_oracle_reachable_at(graph, first, threshold)[second]) {
      return threshold;
    }
  }
  return std::nullopt;
}

}  // namespace

TEST_CASE(kruskal_reconstruction_empty_and_isolated_boundaries) {
  Graph empty(0U, false);
  KruskalReconstructionForest empty_forest(empty);
  REQUIRE_EQ(empty_forest.vertex_count(), std::size_t{0});
  REQUIRE_EQ(empty_forest.reconstruction_node_count(), std::size_t{0});
  REQUIRE_THROWS_AS(empty_forest.component_size_at_most(0U, 0),
                    std::out_of_range);

  Graph isolated(3U, false);
  isolated.add_edge(1U, 1U, std::numeric_limits<Weight>::min());
  KruskalReconstructionForest isolated_forest(isolated);
  REQUIRE_EQ(isolated_forest.reconstruction_node_count(), std::size_t{3});
  for (Vertex vertex = 0; vertex < 3U; ++vertex) {
    REQUIRE_EQ(isolated_forest.component_size_at_most(
                   vertex, std::numeric_limits<Weight>::max()),
               std::size_t{1});
    REQUIRE(isolated_forest.connected_at_most(
        vertex, vertex, std::numeric_limits<Weight>::min()));
  }
  REQUIRE(!isolated_forest.minimax_connectivity(0U, 1U).connected);
}

TEST_CASE(kruskal_reconstruction_validation_signed_thresholds_and_multigraphs) {
  Graph graph(6U, false);
  graph.add_edge(0U, 1U, -5);
  graph.add_edge(1U, 2U, 4);
  graph.add_edge(0U, 2U, 9);
  graph.add_edge(2U, 3U, 4);
  graph.add_edge(4U, 4U, -100);
  graph.add_edge(0U, 1U, -3);

  KruskalReconstructionForest forest(graph);
  REQUIRE_EQ(forest.vertex_count(), std::size_t{6});
  REQUIRE_EQ(forest.reconstruction_node_count(), std::size_t{9});
  REQUIRE_EQ(forest.minimax_connectivity(0U, 3U),
             (MinimaxConnectivityResult{true, Weight{4}}));
  REQUIRE_EQ(forest.minimax_connectivity(2U, 2U),
             (MinimaxConnectivityResult{true, std::nullopt}));
  REQUIRE_EQ(forest.minimax_connectivity(0U, 4U),
             (MinimaxConnectivityResult{false, std::nullopt}));
  REQUIRE(forest.connected_at_most(0U, 1U, -5));
  REQUIRE(!forest.connected_at_most(0U, 2U, 3));
  REQUIRE(forest.connected_at_most(0U, 3U, 4));
  REQUIRE_EQ(forest.component_size_at_most(0U, -6), std::size_t{1});
  REQUIRE_EQ(forest.component_size_at_most(0U, -5), std::size_t{2});
  REQUIRE_EQ(forest.component_size_at_most(0U, 4), std::size_t{4});

  REQUIRE_THROWS_AS(forest.minimax_connectivity(6U, 0U), std::out_of_range);
  Graph directed(2U, true);
  REQUIRE_THROWS_AS(KruskalReconstructionForest(directed),
                    std::invalid_argument);
}

TEST_CASE(kruskal_reconstruction_preserves_full_signed_weight_domain) {
  Graph graph(3U, false);
  graph.add_edge(0U, 1U, std::numeric_limits<Weight>::min());
  graph.add_edge(1U, 2U, std::numeric_limits<Weight>::max());
  KruskalReconstructionForest forest(graph);

  REQUIRE(forest.connected_at_most(0U, 1U,
                                   std::numeric_limits<Weight>::min()));
  REQUIRE(!forest.connected_at_most(
      0U, 2U, std::numeric_limits<Weight>::max() - 1));
  REQUIRE(forest.connected_at_most(0U, 2U,
                                   std::numeric_limits<Weight>::max()));
  REQUIRE_EQ(forest.minimax_connectivity(0U, 2U).bottleneck,
             std::optional<Weight>{std::numeric_limits<Weight>::max()});
}

TEST_CASE(kruskal_reconstruction_equal_weight_order_does_not_change_queries) {
  Graph first(5U, false);
  Graph second(5U, false);
  const std::vector<std::pair<Vertex, Vertex>> edges{{0U, 1U}, {1U, 2U},
                                                      {2U, 3U}, {3U, 4U},
                                                      {0U, 4U}, {1U, 3U}};
  for (const auto& [u, v] : edges) {
    first.add_edge(u, v, 7);
  }
  for (auto it = edges.rbegin(); it != edges.rend(); ++it) {
    second.add_edge(it->first, it->second, 7);
  }

  KruskalReconstructionForest left(first);
  KruskalReconstructionForest right(second);
  for (Vertex u = 0; u < 5U; ++u) {
    for (Vertex v = 0; v < 5U; ++v) {
      REQUIRE_EQ(left.minimax_connectivity(u, v),
                 right.minimax_connectivity(u, v));
      REQUIRE_EQ(left.connected_at_most(u, v, 6),
                 right.connected_at_most(u, v, 6));
      REQUIRE_EQ(left.connected_at_most(u, v, 7),
                 right.connected_at_most(u, v, 7));
    }
    REQUIRE_EQ(left.component_size_at_most(u, 7),
               right.component_size_at_most(u, 7));
  }
}

TEST_CASE(kruskal_reconstruction_randomized_threshold_bfs_differential) {
  std::mt19937_64 generator(UINT64_C(0x4b5255534b414c));
  std::uniform_int_distribution<int> vertex_count_distribution(0, 8);
  std::uniform_int_distribution<int> weight_distribution(-20, 20);
  std::uniform_int_distribution<int> copy_distribution(0, 2);

  for (std::size_t trial = 0; trial < 500U; ++trial) {
    const std::size_t vertex_count =
        static_cast<std::size_t>(vertex_count_distribution(generator));
    Graph graph(vertex_count, false);
    for (Vertex from = 0; from < vertex_count; ++from) {
      if ((generator() & UINT64_C(7)) == 0U) {
        graph.add_edge(from, from, weight_distribution(generator));
      }
      for (Vertex to = from + 1U; to < vertex_count; ++to) {
        const int copies = copy_distribution(generator);
        for (int copy = 0; copy < copies; ++copy) {
          graph.add_edge(from, to, weight_distribution(generator));
        }
      }
    }

    KruskalReconstructionForest forest(graph);
    std::vector<Weight> thresholds{
        std::numeric_limits<Weight>::min(), -21, -10, 0, 10, 21,
        std::numeric_limits<Weight>::max()};
    for (const auto& edge : kruskal_oracle_edges(graph)) {
      thresholds.push_back(edge.weight);
    }
    std::sort(thresholds.begin(), thresholds.end());
    thresholds.erase(std::unique(thresholds.begin(), thresholds.end()),
                     thresholds.end());

    for (Vertex first = 0; first < vertex_count; ++first) {
      for (const Weight threshold : thresholds) {
        const auto reachable =
            kruskal_oracle_reachable_at(graph, first, threshold);
        std::size_t expected_size = 0;
        for (const bool member : reachable) {
          expected_size += static_cast<std::size_t>(member);
        }
        REQUIRE_EQ(forest.component_size_at_most(first, threshold),
                   expected_size);
        for (Vertex second = 0; second < vertex_count; ++second) {
          REQUIRE_EQ(forest.connected_at_most(first, second, threshold),
                     reachable[second]);
        }
      }

      for (Vertex second = 0; second < vertex_count; ++second) {
        const auto actual = forest.minimax_connectivity(first, second);
        if (first == second) {
          REQUIRE(actual.connected);
          REQUIRE(!actual.bottleneck.has_value());
          continue;
        }
        const auto expected =
            kruskal_oracle_bottleneck(graph, first, second);
        REQUIRE_EQ(actual.connected, expected.has_value());
        REQUIRE_EQ(actual.bottleneck, expected);
      }
    }
  }
}
