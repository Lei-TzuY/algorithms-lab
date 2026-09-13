#pragma once

#include "algorithms/graphs/feedback_vertex_set.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

namespace feedback_vertex_set_tests {

using algorithms::graphs::DirectedFeedbackVertexSetResult;
using algorithms::graphs::Edge;
using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;

[[nodiscard]] inline bool mask_removed(std::uint64_t mask, Vertex vertex) {
  return (mask & (std::uint64_t{1} << vertex)) != 0U;
}

[[nodiscard]] inline bool acyclic_after_removal(const Graph& graph,
                                                std::uint64_t mask) {
  const std::size_t n = graph.vertex_count();
  std::vector<std::size_t> indegree(n, 0U);
  std::size_t kept = 0U;
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    if (mask_removed(mask, vertex)) {
      continue;
    }
    ++kept;
    for (const Edge& edge : graph.neighbors(vertex)) {
      if (!mask_removed(mask, edge.to)) {
        ++indegree[edge.to];
      }
    }
  }
  std::vector<Vertex> queue;
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    if (!mask_removed(mask, vertex) && indegree[vertex] == 0U) {
      queue.push_back(vertex);
    }
  }
  std::size_t head = 0U;
  std::size_t seen = 0U;
  while (head < queue.size()) {
    const Vertex vertex = queue[head++];
    ++seen;
    for (const Edge& edge : graph.neighbors(vertex)) {
      if (mask_removed(mask, edge.to)) {
        continue;
      }
      --indegree[edge.to];
      if (indegree[edge.to] == 0U) {
        queue.push_back(edge.to);
      }
    }
  }
  return seen == kept;
}

[[nodiscard]] inline std::vector<Vertex> vertices_from_mask(std::uint64_t mask,
                                                             std::size_t n) {
  std::vector<Vertex> result;
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    if (mask_removed(mask, vertex)) {
      result.push_back(vertex);
    }
  }
  return result;
}

[[nodiscard]] inline std::vector<Vertex> exhaustive_optimum(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  const std::uint64_t limit = std::uint64_t{1} << n;
  std::vector<Vertex> best;
  bool have_best = false;
  for (std::uint64_t mask = 0U; mask < limit; ++mask) {
    if (have_best && static_cast<std::size_t>(std::popcount(mask)) > best.size()) {
      continue;
    }
    if (!acyclic_after_removal(graph, mask)) {
      continue;
    }
    auto candidate = vertices_from_mask(mask, n);
    if (!have_best || candidate.size() < best.size() ||
        (candidate.size() == best.size() && candidate < best)) {
      best = std::move(candidate);
      have_best = true;
    }
  }
  return best;
}

inline void verify_topological_certificate(
    const Graph& graph, const DirectedFeedbackVertexSetResult& result) {
  const std::size_t n = graph.vertex_count();
  std::vector<unsigned char> removed(n, 0U);
  for (const Vertex vertex : result.removed_vertices) {
    REQUIRE(vertex < n);
    REQUIRE(removed[vertex] == 0U);
    removed[vertex] = 1U;
  }
  std::vector<std::size_t> position(n, n);
  for (std::size_t index = 0; index < result.topological_order.size(); ++index) {
    const Vertex vertex = result.topological_order[index];
    REQUIRE(vertex < n);
    REQUIRE(removed[vertex] == 0U);
    REQUIRE(position[vertex] == n);
    position[vertex] = index;
  }
  REQUIRE_EQ(result.topological_order.size() + result.removed_vertices.size(), n);
  for (Vertex vertex = 0; vertex < n; ++vertex) {
    if (removed[vertex] != 0U) {
      continue;
    }
    REQUIRE(position[vertex] < n);
    for (const Edge& edge : graph.neighbors(vertex)) {
      if (removed[edge.to] == 0U) {
        REQUIRE(position[vertex] < position[edge.to]);
      }
    }
  }
}

TEST_CASE(feedback_vertex_set_deterministic_basics) {
  {
    Graph graph(0, true);
    const auto result = algorithms::graphs::minimum_directed_feedback_vertex_set(graph);
    REQUIRE(result.removed_vertices.empty());
    REQUIRE(result.topological_order.empty());
  }
  {
    Graph graph(1, true);
    const auto result = algorithms::graphs::minimum_directed_feedback_vertex_set(graph);
    REQUIRE(result.removed_vertices.empty());
    REQUIRE_EQ(result.topological_order, std::vector<Vertex>({0}));
  }
  {
    Graph graph(1, true);
    graph.add_edge(0, 0, -99);
    const auto result = algorithms::graphs::minimum_directed_feedback_vertex_set(graph);
    REQUIRE_EQ(result.removed_vertices, std::vector<Vertex>({0}));
    REQUIRE_EQ(result.forced_self_loop_vertices, std::vector<Vertex>({0}));
    REQUIRE(result.topological_order.empty());
  }
  {
    Graph graph(5, true);
    graph.add_edge(0, 1);
    graph.add_edge(0, 2);
    graph.add_edge(1, 3);
    graph.add_edge(2, 3);
    graph.add_edge(3, 4);
    const auto result = algorithms::graphs::minimum_directed_feedback_vertex_set(graph);
    REQUIRE(result.removed_vertices.empty());
    verify_topological_certificate(graph, result);
  }
}

TEST_CASE(feedback_vertex_set_cycles_and_ties) {
  Graph triangle(3, true);
  triangle.add_edge(0, 1);
  triangle.add_edge(1, 2);
  triangle.add_edge(2, 0);
  const auto first = algorithms::graphs::minimum_directed_feedback_vertex_set(triangle);
  const auto second = algorithms::graphs::minimum_directed_feedback_vertex_set(triangle);
  REQUIRE_EQ(first.removed_vertices, std::vector<Vertex>({0}));
  REQUIRE_EQ(first, second);
  verify_topological_certificate(triangle, first);

  Graph disjoint(6, true);
  disjoint.add_edge(0, 1);
  disjoint.add_edge(1, 0);
  disjoint.add_edge(3, 4);
  disjoint.add_edge(4, 5);
  disjoint.add_edge(5, 3);
  const auto result = algorithms::graphs::minimum_directed_feedback_vertex_set(disjoint);
  REQUIRE_EQ(result.removed_vertices, std::vector<Vertex>({0, 3}));
  verify_topological_certificate(disjoint, result);

  Graph shared(5, true);
  shared.add_edge(0, 1);
  shared.add_edge(1, 2);
  shared.add_edge(2, 0);
  shared.add_edge(0, 3);
  shared.add_edge(3, 4);
  shared.add_edge(4, 0);
  const auto shared_result =
      algorithms::graphs::minimum_directed_feedback_vertex_set(shared);
  REQUIRE_EQ(shared_result.removed_vertices, std::vector<Vertex>({0}));
  verify_topological_certificate(shared, shared_result);
}

TEST_CASE(feedback_vertex_set_multigraph_validation_and_bounds) {
  Graph graph(3, true);
  graph.add_edge(0, 1, -10);
  graph.add_edge(0, 1, 999);
  graph.add_edge(1, 0, 5);
  graph.add_edge(1, 2, -50);
  const auto result = algorithms::graphs::minimum_directed_feedback_vertex_set(graph);
  REQUIRE_EQ(result.removed_vertices, std::vector<Vertex>({0}));
  verify_topological_certificate(graph, result);

  Graph undirected(3, false);
  undirected.add_edge(0, 1);
  REQUIRE_THROWS_AS(
      algorithms::graphs::minimum_directed_feedback_vertex_set(undirected),
      std::invalid_argument);

  Graph too_large(algorithms::graphs::kMaxExactFeedbackVertexSetVertices + 1U,
                  true);
  REQUIRE_THROWS_AS(
      algorithms::graphs::minimum_directed_feedback_vertex_set(too_large),
      std::length_error);
}

TEST_CASE(feedback_vertex_set_randomized_exhaustive_differential) {
  std::mt19937_64 rng(0xFEE1DEADBEEFULL);
  std::uniform_int_distribution<int> n_dist(0, 9);
  std::uniform_int_distribution<int> edge_coin(0, 99);
  std::uniform_int_distribution<int> copies_dist(1, 3);
  std::uniform_int_distribution<int> weight_dist(-1000, 1000);

  for (int trial = 0; trial < 500; ++trial) {
    const std::size_t n = static_cast<std::size_t>(n_dist(rng));
    Graph graph(n, true);
    for (Vertex from = 0; from < n; ++from) {
      for (Vertex to = 0; to < n; ++to) {
        if (edge_coin(rng) >= 22) {
          continue;
        }
        const int copies = copies_dist(rng);
        for (int copy = 0; copy < copies; ++copy) {
          graph.add_edge(from, to, static_cast<std::int64_t>(weight_dist(rng)));
        }
      }
    }
    const auto expected = exhaustive_optimum(graph);
    const auto actual =
        algorithms::graphs::minimum_directed_feedback_vertex_set(graph);
    REQUIRE_EQ(actual.removed_vertices, expected);
    verify_topological_certificate(graph, actual);
    for (const Vertex forced : actual.forced_self_loop_vertices) {
      REQUIRE(std::binary_search(actual.removed_vertices.begin(),
                                 actual.removed_vertices.end(), forced));
    }
    const auto replay =
        algorithms::graphs::minimum_directed_feedback_vertex_set(graph);
    REQUIRE_EQ(actual, replay);
  }
}

}  // namespace feedback_vertex_set_tests
