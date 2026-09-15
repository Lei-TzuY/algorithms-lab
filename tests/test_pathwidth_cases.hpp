#pragma once

#include "algorithms/graphs/pathwidth.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace pathwidth_test {

using algorithms::graphs::ExactPathwidthResult;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

using Matrix = std::vector<std::vector<bool>>;

inline Matrix simple_matrix(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  Matrix matrix(n, std::vector<bool>(n, false));
  for (Vertex u = 0; u < n; ++u) {
    for (const auto& edge : graph.neighbors(u)) {
      if (edge.to != u) {
        matrix[u][edge.to] = true;
      }
    }
  }
  return matrix;
}

inline std::size_t boundary_size(const Matrix& matrix,
                                 const std::vector<bool>& in_prefix) {
  const std::size_t n = matrix.size();
  std::size_t boundary = 0;
  for (Vertex u = 0; u < n; ++u) {
    if (!in_prefix[u]) {
      continue;
    }
    bool reaches_suffix = false;
    for (Vertex v = 0; v < n; ++v) {
      if (!in_prefix[v] && matrix[u][v]) {
        reaches_suffix = true;
        break;
      }
    }
    if (reaches_suffix) {
      ++boundary;
    }
  }
  return boundary;
}

inline std::size_t separation_of_order(const Matrix& matrix,
                                       const std::vector<Vertex>& order) {
  std::vector<bool> in_prefix(matrix.size(), false);
  std::size_t width = 0;
  width = std::max(width, boundary_size(matrix, in_prefix));
  for (Vertex vertex : order) {
    in_prefix[vertex] = true;
    width = std::max(width, boundary_size(matrix, in_prefix));
  }
  return width;
}

inline std::pair<std::size_t, std::vector<Vertex>> exhaustive_oracle(
    const Matrix& matrix) {
  const std::size_t n = matrix.size();
  if (n == 0) {
    return {0, {}};
  }
  std::vector<Vertex> order(n);
  std::iota(order.begin(), order.end(), Vertex{0});
  std::size_t best = n;
  std::vector<Vertex> best_order;
  do {
    const std::size_t width = separation_of_order(matrix, order);
    if (width < best) {
      best = width;
      best_order = order;
    }
  } while (std::next_permutation(order.begin(), order.end()));
  return {best, best_order};
}

inline void verify_result(const Graph& graph, const ExactPathwidthResult& result) {
  const Matrix matrix = simple_matrix(graph);
  const std::size_t n = matrix.size();
  REQUIRE_EQ(result.ordering.size(), n);
  REQUIRE_EQ(result.bags.size(), n);
  REQUIRE_EQ(result.prefix_boundary_sizes.size(), n + 1);

  std::vector<bool> seen(n, false);
  std::vector<bool> in_prefix(n, false);
  std::size_t witnessed_width = 0;
  for (std::size_t step = 0; step < n; ++step) {
    const Vertex vertex = result.ordering[step];
    REQUIRE(vertex < n);
    REQUIRE(!seen[vertex]);
    seen[vertex] = true;

    const std::size_t expected_boundary = boundary_size(matrix, in_prefix);
    REQUIRE_EQ(result.prefix_boundary_sizes[step], expected_boundary);

    std::vector<Vertex> expected_bag;
    for (Vertex prior = 0; prior < n; ++prior) {
      if (!in_prefix[prior]) {
        continue;
      }
      bool reaches_suffix = false;
      for (Vertex future = 0; future < n; ++future) {
        if (!in_prefix[future] && matrix[prior][future]) {
          reaches_suffix = true;
          break;
        }
      }
      if (reaches_suffix) {
        expected_bag.push_back(prior);
      }
    }
    expected_bag.push_back(vertex);
    std::sort(expected_bag.begin(), expected_bag.end());
    REQUIRE(result.bags[step] == expected_bag);
    REQUIRE(!result.bags[step].empty());
    witnessed_width = std::max(witnessed_width, result.bags[step].size() - 1);
    in_prefix[vertex] = true;
  }
  REQUIRE_EQ(result.prefix_boundary_sizes[n], std::size_t{0});
  REQUIRE_EQ(witnessed_width, result.pathwidth);
  REQUIRE_EQ(separation_of_order(matrix, result.ordering), result.pathwidth);

  for (Vertex vertex = 0; vertex < n; ++vertex) {
    bool appeared = false;
    bool ended = false;
    for (const auto& bag : result.bags) {
      const bool present = std::binary_search(bag.begin(), bag.end(), vertex);
      if (present) {
        REQUIRE(!ended);
        appeared = true;
      } else if (appeared) {
        ended = true;
      }
    }
    REQUIRE(appeared);
  }
  for (Vertex u = 0; u < n; ++u) {
    for (Vertex v = u + 1; v < n; ++v) {
      if (!matrix[u][v]) {
        continue;
      }
      bool covered = false;
      for (const auto& bag : result.bags) {
        if (std::binary_search(bag.begin(), bag.end(), u) &&
            std::binary_search(bag.begin(), bag.end(), v)) {
          covered = true;
          break;
        }
      }
      REQUIRE(covered);
    }
  }
}

}  // namespace pathwidth_test

using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

TEST_CASE(pathwidth_validation_and_known_graphs) {
  using algorithms::graphs::exact_pathwidth;
  {
    Graph directed(2, true);
    directed.add_edge(0, 1);
    REQUIRE_THROWS_AS(exact_pathwidth(directed), std::invalid_argument);
  }
  REQUIRE_THROWS_AS(exact_pathwidth(Graph(21, false)), std::length_error);

  {
    Graph graph(0, false);
    const auto result = exact_pathwidth(graph);
    REQUIRE_EQ(result.pathwidth, std::size_t{0});
    pathwidth_test::verify_result(graph, result);
  }
  {
    Graph graph(1, false);
    graph.add_edge(0, 0, -99);
    const auto result = exact_pathwidth(graph);
    REQUIRE_EQ(result.pathwidth, std::size_t{0});
    REQUIRE(result.ordering == std::vector<Vertex>{0});
    pathwidth_test::verify_result(graph, result);
  }
  {
    Graph path(5, false);
    for (Vertex v = 1; v < 5; ++v) path.add_edge(v - 1, v);
    const auto result = exact_pathwidth(path);
    REQUIRE_EQ(result.pathwidth, std::size_t{1});
    REQUIRE(result.ordering == (std::vector<Vertex>{0, 1, 2, 3, 4}));
    pathwidth_test::verify_result(path, result);
  }
  {
    Graph cycle(4, false);
    cycle.add_edge(0, 1); cycle.add_edge(1, 2); cycle.add_edge(2, 3); cycle.add_edge(3, 0);
    const auto result = exact_pathwidth(cycle);
    REQUIRE_EQ(result.pathwidth, std::size_t{2});
    pathwidth_test::verify_result(cycle, result);
  }
  {
    Graph clique(5, false);
    for (Vertex u = 0; u < 5; ++u) for (Vertex v = u + 1; v < 5; ++v) clique.add_edge(u, v);
    const auto result = exact_pathwidth(clique);
    REQUIRE_EQ(result.pathwidth, std::size_t{4});
    REQUIRE(result.ordering == (std::vector<Vertex>{0, 1, 2, 3, 4}));
    pathwidth_test::verify_result(clique, result);
  }
}

TEST_CASE(pathwidth_multigraph_semantics_and_determinism) {
  using algorithms::graphs::exact_pathwidth;
  Graph graph(5, false);
  graph.add_edge(0, 1, -7);
  graph.add_edge(0, 1, 999);
  graph.add_edge(1, 2, 3);
  graph.add_edge(2, 3, -4);
  graph.add_edge(3, 4, 5);
  graph.add_edge(2, 2, -123);
  const auto first = exact_pathwidth(graph);
  const auto second = exact_pathwidth(graph);
  REQUIRE_EQ(first.pathwidth, std::size_t{1});
  REQUIRE(first.ordering == second.ordering);
  REQUIRE(first.bags == second.bags);
  REQUIRE(first.prefix_boundary_sizes == second.prefix_boundary_sizes);
  pathwidth_test::verify_result(graph, first);
}

TEST_CASE(pathwidth_lexicographic_optimum_matches_permutation_oracle) {
  using algorithms::graphs::exact_pathwidth;
  Graph graph(6, false);
  graph.add_edge(0, 3);
  graph.add_edge(1, 3);
  graph.add_edge(1, 4);
  graph.add_edge(2, 4);
  graph.add_edge(2, 5);
  const auto result = exact_pathwidth(graph);
  const auto oracle = pathwidth_test::exhaustive_oracle(pathwidth_test::simple_matrix(graph));
  REQUIRE_EQ(result.pathwidth, oracle.first);
  REQUIRE(result.ordering == oracle.second);
  pathwidth_test::verify_result(graph, result);
}

TEST_CASE(pathwidth_lexicographic_reconstruction_respects_global_optimum) {
  using algorithms::graphs::exact_pathwidth;
  Graph graph(8, false);
  graph.add_edge(0, 2);
  graph.add_edge(1, 4);
  graph.add_edge(1, 5);
  graph.add_edge(2, 4);
  graph.add_edge(2, 5);
  graph.add_edge(3, 7);
  graph.add_edge(4, 6);

  const auto result = exact_pathwidth(graph);
  const std::vector<Vertex> expected{0, 1, 2, 5, 4, 3, 6, 7};
  REQUIRE_EQ(result.pathwidth, std::size_t{2});
  REQUIRE(result.ordering == expected);
  pathwidth_test::verify_result(graph, result);
}

TEST_CASE(pathwidth_randomized_differential_and_bag_replay) {
  using algorithms::graphs::exact_pathwidth;
  std::mt19937_64 rng(0x5041544857494454ULL);
  for (std::size_t trial = 0; trial < 300; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 8U);
    Graph graph(n, false);
    for (Vertex u = 0; u < n; ++u) {
      if ((rng() & 7U) == 0U) graph.add_edge(u, u, static_cast<std::int64_t>(rng() % 101U) - 50);
      for (Vertex v = u + 1; v < n; ++v) {
        if ((rng() % 100U) < 34U) {
          graph.add_edge(u, v, static_cast<std::int64_t>(rng() % 101U) - 50);
          if ((rng() & 3U) == 0U) graph.add_edge(u, v, static_cast<std::int64_t>(rng() % 101U) - 50);
        }
      }
    }
    const auto result = exact_pathwidth(graph);
    const auto oracle = pathwidth_test::exhaustive_oracle(pathwidth_test::simple_matrix(graph));
    REQUIRE_EQ(result.pathwidth, oracle.first);
    REQUIRE(result.ordering == oracle.second);
    pathwidth_test::verify_result(graph, result);
  }
}
