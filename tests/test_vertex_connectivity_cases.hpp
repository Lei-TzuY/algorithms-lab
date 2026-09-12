#pragma once

#include "algorithms/graphs/vertex_connectivity.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <queue>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace vertex_connectivity_test_detail {

using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;
using StructuralAdjacency = std::vector<std::vector<unsigned char>>;

[[nodiscard]] inline StructuralAdjacency adjacency_of(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  StructuralAdjacency adjacency(n, std::vector<unsigned char>(n, 0U));
  for (Vertex from = 0; from < n; ++from) {
    for (const auto& edge : graph.neighbors(from)) {
      if (edge.to != from) {
        adjacency[from][edge.to] = 1U;
      }
    }
  }
  return adjacency;
}

[[nodiscard]] inline bool disconnected_after_mask(
    const StructuralAdjacency& adjacency, std::uint64_t removed_mask) {
  const std::size_t n = adjacency.size();
  std::size_t remaining = 0U;
  Vertex start = n;
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    if (((removed_mask >> vertex) & 1U) == 0U) {
      ++remaining;
      if (start == n) {
        start = vertex;
      }
    }
  }
  if (remaining <= 1U) {
    return true;
  }

  std::vector<bool> seen(n, false);
  std::queue<Vertex> queue;
  seen[start] = true;
  queue.push(start);
  while (!queue.empty()) {
    const Vertex vertex = queue.front();
    queue.pop();
    for (Vertex next = 0; next < n; ++next) {
      if (adjacency[vertex][next] != 0U &&
          ((removed_mask >> next) & 1U) == 0U && !seen[next]) {
        seen[next] = true;
        queue.push(next);
      }
    }
  }

  std::size_t reached = 0U;
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    if (((removed_mask >> vertex) & 1U) == 0U && seen[vertex]) {
      ++reached;
    }
  }
  return reached != remaining;
}

[[nodiscard]] inline std::size_t exhaustive_connectivity(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  if (n <= 1U) {
    return 0U;
  }
  if (n >= 64U) {
    throw std::logic_error("vertex-connectivity exhaustive oracle domain exceeded");
  }
  const auto adjacency = adjacency_of(graph);
  const std::uint64_t limit = std::uint64_t{1} << n;
  for (std::size_t size = 0; size < n; ++size) {
    for (std::uint64_t mask = 0; mask < limit; ++mask) {
      if (static_cast<std::size_t>(std::popcount(mask)) != size) {
        continue;
      }
      if (disconnected_after_mask(adjacency, mask)) {
        return size;
      }
    }
  }
  return n - 1U;
}

[[nodiscard]] inline std::uint64_t mask_of(
    std::size_t n, const std::vector<Vertex>& vertices) {
  if (n >= 64U) {
    throw std::logic_error("vertex-connectivity witness replay domain exceeded");
  }
  std::uint64_t mask = 0U;
  for (const Vertex vertex : vertices) {
    if (vertex >= n) {
      throw std::runtime_error("vertex-connectivity witness contains invalid vertex");
    }
    mask |= std::uint64_t{1} << vertex;
  }
  return mask;
}

inline void verify_result(const Graph& graph,
                          const algorithms::graphs::VertexConnectivityResult& result) {
  const std::size_t n = graph.vertex_count();
  REQUIRE_EQ(result.separator.size(), result.connectivity);
  const auto adjacency = adjacency_of(graph);
  const std::uint64_t mask = mask_of(n, result.separator);

  if (n == 0U) {
    REQUIRE_EQ(result.connectivity, 0U);
    REQUIRE(!result.input_connected);
    REQUIRE(!result.complete_graph);
    REQUIRE(!result.separated_pair.has_value());
    return;
  }
  if (n == 1U) {
    REQUIRE_EQ(result.connectivity, 0U);
    REQUIRE(result.input_connected);
    REQUIRE(result.complete_graph);
    REQUIRE(!result.separated_pair.has_value());
    return;
  }

  if (!result.input_connected) {
    REQUIRE_EQ(result.connectivity, 0U);
    REQUIRE(result.separator.empty());
    REQUIRE(result.separated_pair.has_value());
    REQUIRE(disconnected_after_mask(adjacency, 0U));
    return;
  }

  if (result.complete_graph) {
    REQUIRE_EQ(result.connectivity, n - 1U);
    REQUIRE(!result.separated_pair.has_value());
    REQUIRE(disconnected_after_mask(adjacency, mask));
    return;
  }

  REQUIRE(result.separated_pair.has_value());
  const auto [left, right] = *result.separated_pair;
  REQUIRE(left < n);
  REQUIRE(right < n);
  REQUIRE(left != right);
  REQUIRE(adjacency[left][right] == 0U);
  REQUIRE(((mask >> left) & 1U) == 0U);
  REQUIRE(((mask >> right) & 1U) == 0U);
  REQUIRE(disconnected_after_mask(adjacency, mask));
}

}  // namespace vertex_connectivity_test_detail

TEST_CASE(vertex_connectivity_deterministic_semantics_and_validation) {
  using algorithms::graphs::Graph;
  using algorithms::graphs::Vertex;
  using algorithms::graphs::exact_vertex_connectivity;

  Graph empty(0U, false);
  vertex_connectivity_test_detail::verify_result(
      empty, exact_vertex_connectivity(empty));

  Graph singleton(1U, false);
  vertex_connectivity_test_detail::verify_result(
      singleton, exact_vertex_connectivity(singleton));

  Graph path(5U, false);
  path.add_edge(0U, 1U);
  path.add_edge(1U, 2U);
  path.add_edge(2U, 3U);
  path.add_edge(3U, 4U);
  const auto path_result = exact_vertex_connectivity(path);
  REQUIRE_EQ(path_result.connectivity, 1U);
  vertex_connectivity_test_detail::verify_result(path, path_result);

  Graph complete(5U, false);
  for (Vertex left = 0; left < 5U; ++left) {
    for (Vertex right = left + 1U; right < 5U; ++right) {
      complete.add_edge(left, right);
    }
  }
  const auto complete_result = exact_vertex_connectivity(complete);
  REQUIRE_EQ(complete_result.connectivity, 4U);
  REQUIRE(complete_result.complete_graph);
  vertex_connectivity_test_detail::verify_result(complete, complete_result);

  Graph disconnected(4U, false);
  disconnected.add_edge(0U, 1U);
  disconnected.add_edge(2U, 3U);
  const auto disconnected_result = exact_vertex_connectivity(disconnected);
  REQUIRE_EQ(disconnected_result.connectivity, 0U);
  REQUIRE(!disconnected_result.input_connected);
  vertex_connectivity_test_detail::verify_result(disconnected,
                                                  disconnected_result);

  Graph directed(3U, true);
  directed.add_edge(0U, 1U);
  REQUIRE_THROWS_AS(exact_vertex_connectivity(directed), std::invalid_argument);
}

TEST_CASE(vertex_connectivity_multigraph_semantics_and_determinism) {
  using algorithms::graphs::Graph;
  using algorithms::graphs::exact_vertex_connectivity;

  Graph triangle(3U, false);
  triangle.add_edge(0U, 0U, 17);
  triangle.add_edge(0U, 1U, 4);
  triangle.add_edge(0U, 1U, -9);
  triangle.add_edge(1U, 2U, 3);
  triangle.add_edge(2U, 0U, 8);

  const auto first = exact_vertex_connectivity(triangle);
  const auto second = exact_vertex_connectivity(triangle);
  REQUIRE_EQ(first.connectivity, 2U);
  REQUIRE_EQ(first.connectivity, second.connectivity);
  REQUIRE(first.separator == second.separator);
  REQUIRE(first.separated_pair == second.separated_pair);
  REQUIRE(first.input_connected == second.input_connected);
  REQUIRE(first.complete_graph == second.complete_graph);
  vertex_connectivity_test_detail::verify_result(triangle, first);
}

TEST_CASE(vertex_connectivity_randomized_exhaustive_differential) {
  using algorithms::graphs::Graph;
  using algorithms::graphs::Vertex;
  using algorithms::graphs::exact_vertex_connectivity;

  std::mt19937_64 random(0x51A5EEDULL);
  for (std::size_t trial = 0; trial < 900U; ++trial) {
    const std::size_t n = static_cast<std::size_t>(random() % 9U);
    Graph graph(n, false);
    const std::size_t copies = static_cast<std::size_t>(random() % 30U);
    for (std::size_t copy = 0; copy < copies && n > 0U; ++copy) {
      const Vertex from = static_cast<Vertex>(random() % n);
      const Vertex to = static_cast<Vertex>(random() % n);
      graph.add_edge(from, to, static_cast<std::int64_t>(random()));
      if ((random() & 7U) == 0U) {
        graph.add_edge(from, to, -7);
      }
    }

    const auto actual = exact_vertex_connectivity(graph);
    REQUIRE_EQ(actual.connectivity,
               vertex_connectivity_test_detail::exhaustive_connectivity(graph));
    vertex_connectivity_test_detail::verify_result(graph, actual);
  }
}
