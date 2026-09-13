#pragma once

#include "algorithms/graphs/minimum_dominating_set.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

namespace minimum_dominating_set_tests {

using algorithms::graphs::Graph;
using algorithms::graphs::Vertex;
using algorithms::graphs::minimum_dominating_set;

[[nodiscard]] inline bool dominates(const Graph& graph, std::uint64_t subset) {
  std::uint64_t covered = 0;
  for (Vertex vertex = 0; vertex < graph.vertex_count(); ++vertex) {
    if ((subset & (std::uint64_t{1} << vertex)) == 0) {
      continue;
    }
    covered |= std::uint64_t{1} << vertex;
    for (const auto& edge : graph.neighbors(vertex)) {
      covered |= std::uint64_t{1} << edge.to;
    }
  }
  const std::uint64_t all = graph.vertex_count() == 0
                                ? 0
                                : ((std::uint64_t{1} << graph.vertex_count()) - 1);
  return covered == all;
}

[[nodiscard]] inline std::size_t exhaustive_optimum(const Graph& graph) {
  const std::size_t n = graph.vertex_count();
  if (n == 0) {
    return 0;
  }
  std::size_t best = n + 1;
  const std::uint64_t end = std::uint64_t{1} << n;
  for (std::uint64_t subset = 0; subset < end; ++subset) {
    const std::size_t size = static_cast<std::size_t>(std::popcount(subset));
    if (size >= best) {
      continue;
    }
    if (dominates(graph, subset)) {
      best = size;
    }
  }
  return best;
}

inline void verify_result(const Graph& graph) {
  const auto result = minimum_dominating_set(graph);
  std::uint64_t mask = 0;
  for (const Vertex vertex : result.vertices) {
    REQUIRE(vertex < graph.vertex_count());
    REQUIRE((mask & (std::uint64_t{1} << vertex)) == 0);
    mask |= std::uint64_t{1} << vertex;
  }
  REQUIRE(std::is_sorted(result.vertices.begin(), result.vertices.end()));
  REQUIRE(dominates(graph, mask));
  REQUIRE_EQ(result.vertices.size(), exhaustive_optimum(graph));
  REQUIRE(result.explored_states <= (std::uint64_t{1} << graph.vertex_count()));

  const auto replay = minimum_dominating_set(graph);
  REQUIRE(result.vertices == replay.vertices);
  REQUIRE_EQ(result.explored_states, replay.explored_states);
  REQUIRE_EQ(result.bound_prunes, replay.bound_prunes);
}

}  // namespace minimum_dominating_set_tests

TEST_CASE(minimum_dominating_set_deterministic_shapes) {
  using namespace minimum_dominating_set_tests;

  Graph empty(0, false);
  REQUIRE(minimum_dominating_set(empty).vertices.empty());

  Graph singleton(1, false);
  verify_result(singleton);
  REQUIRE(minimum_dominating_set(singleton).vertices == std::vector<Vertex>{0});

  Graph star(5, false);
  for (Vertex vertex = 1; vertex < 5; ++vertex) {
    star.add_edge(0, vertex, static_cast<std::int64_t>(vertex));
  }
  verify_result(star);
  REQUIRE(minimum_dominating_set(star).vertices == std::vector<Vertex>{0});

  Graph cycle(6, false);
  for (Vertex vertex = 0; vertex < 6; ++vertex) {
    cycle.add_edge(vertex, (vertex + 1) % 6);
  }
  verify_result(cycle);
  REQUIRE_EQ(minimum_dominating_set(cycle).vertices.size(), std::size_t{2});
}

TEST_CASE(minimum_dominating_set_multigraph_and_validation) {
  using namespace minimum_dominating_set_tests;

  Graph graph(3, false);
  graph.add_edge(0, 1, -9);
  graph.add_edge(0, 1, 100);
  graph.add_edge(1, 1, 77);
  graph.add_edge(1, 2, -100);
  verify_result(graph);
  REQUIRE_EQ(minimum_dominating_set(graph).vertices.size(), std::size_t{1});

  Graph directed(2, true);
  directed.add_edge(0, 1);
  REQUIRE_THROWS_AS(minimum_dominating_set(directed), std::invalid_argument);

  Graph boundary(24, false);
  const auto boundary_result = minimum_dominating_set(boundary);
  REQUIRE_EQ(boundary_result.vertices.size(), std::size_t{24});
  REQUIRE(boundary_result.explored_states <= (std::uint64_t{1} << 24U));

  Graph too_large(25, false);
  REQUIRE_THROWS_AS(minimum_dominating_set(too_large), std::length_error);
}

TEST_CASE(minimum_dominating_set_randomized_exhaustive_differential) {
  using namespace minimum_dominating_set_tests;

  std::mt19937_64 rng(0xD0A11A7EULL);
  for (int trial = 0; trial < 600; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 11ULL);
    Graph graph(n, false);
    const std::size_t copies = static_cast<std::size_t>(rng() % 30ULL);
    for (std::size_t copy = 0; copy < copies && n > 0; ++copy) {
      const Vertex from = static_cast<Vertex>(rng() % n);
      const Vertex to = static_cast<Vertex>(rng() % n);
      const std::int64_t weight = static_cast<std::int64_t>(rng() % 201ULL) - 100;
      graph.add_edge(from, to, weight);
    }
    verify_result(graph);
  }
}
