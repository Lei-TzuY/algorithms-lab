#pragma once

#include "algorithms/graphs/minimum_fill_in.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <utility>
#include <vector>

namespace {
using algorithms::graphs::ExactMinimumFillInResult;
using algorithms::graphs::FillEdge;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

struct OracleResult {
  std::size_t fill{};
  std::vector<Vertex> order;
};

std::vector<std::vector<unsigned char>> simple_adjacency(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::vector<unsigned char>> adjacency(
      n, std::vector<unsigned char>(n, 0U));
  for (Vertex from = 0; from < n; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (edge.to != from) {
        adjacency[from][edge.to] = 1U;
      }
    }
  }
  return adjacency;
}

std::size_t replay_fill_count(
    std::vector<std::vector<unsigned char>> adjacency,
    const std::vector<Vertex>& order,
    std::vector<FillEdge>* fill_edges = nullptr) {
  const std::size_t n = adjacency.size();
  std::vector<unsigned char> active(n, 1U);
  std::size_t fill = 0U;
  for (const Vertex vertex : order) {
    REQUIRE(vertex < n);
    REQUIRE(active[vertex] != 0U);
    std::vector<Vertex> neighbors;
    for (Vertex other = 0; other < n; ++other) {
      if (active[other] != 0U && other != vertex &&
          adjacency[vertex][other] != 0U) {
        neighbors.push_back(other);
      }
    }
    for (std::size_t i = 0; i < neighbors.size(); ++i) {
      for (std::size_t j = i + 1U; j < neighbors.size(); ++j) {
        Vertex first = neighbors[i];
        Vertex second = neighbors[j];
        if (adjacency[first][second] == 0U) {
          adjacency[first][second] = 1U;
          adjacency[second][first] = 1U;
          ++fill;
          if (fill_edges != nullptr) {
            if (second < first) {
              std::swap(first, second);
            }
            fill_edges->push_back(FillEdge{first, second});
          }
        }
      }
    }
    active[vertex] = 0U;
  }
  return fill;
}

OracleResult exhaustive_order_oracle(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  std::vector<Vertex> order(n);
  std::iota(order.begin(), order.end(), Vertex{0});
  OracleResult best;
  best.fill = static_cast<std::size_t>(-1);
  do {
    const std::size_t fill = replay_fill_count(simple_adjacency(graph), order);
    if (fill < best.fill) {
      best.fill = fill;
      best.order = order;
    }
  } while (std::next_permutation(order.begin(), order.end()));
  if (n == 0U) {
    best.fill = 0U;
  }
  return best;
}

void verify_result(const Graph& graph, const ExactMinimumFillInResult& result) {
  const std::size_t n = graph.vertex_count();
  REQUIRE_EQ(result.elimination_order.size(), n);
  REQUIRE_EQ(result.steps.size(), n);
  REQUIRE_EQ(result.fill_edges.size(), result.fill_edge_count);

  std::vector<Vertex> sorted_order = result.elimination_order;
  std::sort(sorted_order.begin(), sorted_order.end());
  std::vector<Vertex> expected_vertices(n);
  std::iota(expected_vertices.begin(), expected_vertices.end(), Vertex{0});
  REQUIRE_EQ(sorted_order, expected_vertices);

  auto adjacency = simple_adjacency(graph);
  std::vector<unsigned char> active(n, 1U);
  std::vector<FillEdge> replayed;
  for (std::size_t step_index = 0; step_index < n; ++step_index) {
    const auto& step = result.steps[step_index];
    REQUIRE_EQ(step.vertex, result.elimination_order[step_index]);
    REQUIRE(active[step.vertex] != 0U);

    std::vector<Vertex> current_neighbors;
    for (Vertex other = 0; other < n; ++other) {
      if (active[other] != 0U && other != step.vertex &&
          adjacency[step.vertex][other] != 0U) {
        current_neighbors.push_back(other);
      }
    }
    REQUIRE_EQ(step.current_neighbors, current_neighbors);

    std::vector<FillEdge> step_added;
    for (std::size_t i = 0; i < current_neighbors.size(); ++i) {
      for (std::size_t j = i + 1U; j < current_neighbors.size(); ++j) {
        const Vertex first = current_neighbors[i];
        const Vertex second = current_neighbors[j];
        if (adjacency[first][second] == 0U) {
          adjacency[first][second] = 1U;
          adjacency[second][first] = 1U;
          step_added.push_back(FillEdge{first, second});
          replayed.push_back(FillEdge{first, second});
        }
      }
    }
    REQUIRE_EQ(step.added_edges, step_added);
    active[step.vertex] = 0U;
  }
  REQUIRE_EQ(replayed.size(), result.fill_edge_count);
  REQUIRE_EQ(replayed, result.fill_edges);
}

Graph cycle_graph(const std::size_t n) {
  Graph graph(n, false);
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    graph.add_edge(vertex, (vertex + 1U) % n);
  }
  return graph;
}

TEST_CASE(minimum_fill_in_edge_cases_and_validation) {
  {
    Graph graph(0U, false);
    const auto result = algorithms::graphs::exact_minimum_fill_in(graph);
    REQUIRE_EQ(result.fill_edge_count, 0U);
    REQUIRE(result.elimination_order.empty());
    REQUIRE(result.fill_edges.empty());
  }
  {
    Graph graph(1U, false);
    graph.add_edge(0U, 0U, -99);
    const auto result = algorithms::graphs::exact_minimum_fill_in(graph);
    REQUIRE_EQ(result.fill_edge_count, 0U);
    REQUIRE_EQ(result.elimination_order, std::vector<Vertex>{0U});
    verify_result(graph, result);
  }
  {
    Graph graph(2U, true);
    graph.add_edge(0U, 1U);
    REQUIRE_THROWS_AS(algorithms::graphs::exact_minimum_fill_in(graph),
                      std::invalid_argument);
  }
  {
    Graph graph(algorithms::graphs::kExactMinimumFillInMaxVertices + 1U,
                false);
    REQUIRE_THROWS_AS(algorithms::graphs::exact_minimum_fill_in(graph),
                      std::length_error);
  }
}

TEST_CASE(minimum_fill_in_known_structures) {
  {
    Graph path(5U, false);
    for (Vertex v = 0; v + 1U < 5U; ++v) {
      path.add_edge(v, v + 1U);
    }
    const auto result = algorithms::graphs::exact_minimum_fill_in(path);
    REQUIRE_EQ(result.fill_edge_count, 0U);
    REQUIRE_EQ(result.elimination_order,
               (std::vector<Vertex>{0U, 1U, 2U, 3U, 4U}));
    verify_result(path, result);
  }
  {
    Graph star(4U, false);
    star.add_edge(0U, 1U);
    star.add_edge(0U, 2U);
    star.add_edge(0U, 3U);
    const auto result = algorithms::graphs::exact_minimum_fill_in(star);
    REQUIRE_EQ(result.fill_edge_count, 0U);
    REQUIRE_EQ(result.elimination_order,
               (std::vector<Vertex>{1U, 2U, 0U, 3U}));
    verify_result(star, result);
  }
  {
    Graph cycle4 = cycle_graph(4U);
    const auto result = algorithms::graphs::exact_minimum_fill_in(cycle4);
    REQUIRE_EQ(result.fill_edge_count, 1U);
    REQUIRE_EQ(result.elimination_order,
               (std::vector<Vertex>{0U, 1U, 2U, 3U}));
    REQUIRE_EQ(result.fill_edges, (std::vector<FillEdge>{{1U, 3U}}));
    verify_result(cycle4, result);
  }
  {
    Graph cycle5 = cycle_graph(5U);
    const auto result = algorithms::graphs::exact_minimum_fill_in(cycle5);
    REQUIRE_EQ(result.fill_edge_count, 2U);
    REQUIRE_EQ(result.elimination_order,
               (std::vector<Vertex>{0U, 1U, 2U, 3U, 4U}));
    REQUIRE_EQ(result.fill_edges,
               (std::vector<FillEdge>{{1U, 4U}, {2U, 4U}}));
    verify_result(cycle5, result);
  }
}

TEST_CASE(minimum_fill_in_multigraph_projection_and_determinism) {
  Graph graph(4U, false);
  graph.add_edge(0U, 1U, 7);
  graph.add_edge(0U, 1U, -50);
  graph.add_edge(1U, 2U, 99);
  graph.add_edge(2U, 3U, 3);
  graph.add_edge(3U, 0U, -8);
  graph.add_edge(2U, 2U, -1000);

  const auto first = algorithms::graphs::exact_minimum_fill_in(graph);
  const auto second = algorithms::graphs::exact_minimum_fill_in(graph);
  REQUIRE_EQ(first.fill_edge_count, 1U);
  REQUIRE_EQ(first.elimination_order, second.elimination_order);
  REQUIRE_EQ(first.fill_edges, second.fill_edges);
  REQUIRE_EQ(first.steps, second.steps);
  verify_result(graph, first);
}

TEST_CASE(minimum_fill_in_randomized_against_all_elimination_orders) {
  std::mt19937_64 rng(0xF1111AULL);
  for (std::size_t trial = 0; trial < 180U; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 8U);
    Graph graph(n, false);
    for (Vertex u = 0; u < n; ++u) {
      if ((rng() % 7U) == 0U) {
        graph.add_edge(u, u, static_cast<std::int64_t>(rng()));
      }
      for (Vertex v = u + 1U; v < n; ++v) {
        if ((rng() % 100U) < 37U) {
          graph.add_edge(u, v, static_cast<std::int64_t>(rng()));
          if ((rng() % 5U) == 0U) {
            graph.add_edge(u, v, -static_cast<std::int64_t>(rng() % 1000U));
          }
        }
      }
    }

    const auto expected = exhaustive_order_oracle(graph);
    const auto actual = algorithms::graphs::exact_minimum_fill_in(graph);
    REQUIRE_EQ(actual.fill_edge_count, expected.fill);
    REQUIRE_EQ(actual.elimination_order, expected.order);
    verify_result(graph, actual);
  }
}

}  // namespace

