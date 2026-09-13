#pragma once

#include "algorithms/games/sprague_grundy.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace sprague_grundy_test_detail {

enum class Mark : unsigned char { unknown, visiting, losing, winning };

[[nodiscard]] inline bool product_game_winning(
    const algorithms::graphs::Graph& graph,
    algorithms::graphs::Vertex vertex, std::size_t heap,
    std::vector<std::vector<Mark>>& memo) {
  auto& mark = memo[vertex][heap];
  if (mark == Mark::losing) return false;
  if (mark == Mark::winning) return true;
  if (mark == Mark::visiting) throw std::logic_error("oracle cycle");
  mark = Mark::visiting;
  for (const auto& edge : graph.neighbors(vertex)) {
    if (!product_game_winning(graph, edge.to, heap, memo)) {
      mark = Mark::winning;
      return true;
    }
  }
  for (std::size_t next_heap = 0U; next_heap < heap; ++next_heap) {
    if (!product_game_winning(graph, vertex, next_heap, memo)) {
      mark = Mark::winning;
      return true;
    }
  }
  mark = Mark::losing;
  return false;
}

[[nodiscard]] inline std::size_t independent_grundy_by_nim_equivalence(
    const algorithms::graphs::Graph& graph,
    algorithms::graphs::Vertex vertex) {
  const std::size_t limit = graph.vertex_count();
  std::optional<std::size_t> losing_heap;
  for (std::size_t heap = 0U; heap <= limit; ++heap) {
    std::vector<std::vector<Mark>> memo(
        graph.vertex_count(), std::vector<Mark>(limit + 1U, Mark::unknown));
    if (!product_game_winning(graph, vertex, heap, memo)) {
      REQUIRE(!losing_heap.has_value());
      losing_heap = heap;
    }
  }
  REQUIRE(losing_heap.has_value());
  return *losing_heap;
}

[[nodiscard]] inline bool two_component_winning(
    const algorithms::graphs::Graph& first, algorithms::graphs::Vertex first_vertex,
    const algorithms::graphs::Graph& second,
    algorithms::graphs::Vertex second_vertex,
    std::vector<std::vector<Mark>>& memo) {
  auto& mark = memo[first_vertex][second_vertex];
  if (mark == Mark::losing) return false;
  if (mark == Mark::winning) return true;
  if (mark == Mark::visiting) throw std::logic_error("oracle cycle");
  mark = Mark::visiting;
  for (const auto& edge : first.neighbors(first_vertex)) {
    if (!two_component_winning(first, edge.to, second, second_vertex, memo)) {
      mark = Mark::winning;
      return true;
    }
  }
  for (const auto& edge : second.neighbors(second_vertex)) {
    if (!two_component_winning(first, first_vertex, second, edge.to, memo)) {
      mark = Mark::winning;
      return true;
    }
  }
  mark = Mark::losing;
  return false;
}

[[nodiscard]] inline algorithms::graphs::Graph random_dag(std::mt19937_64& rng,
                                                           std::size_t n) {
  algorithms::graphs::Graph graph(n, true);
  std::uniform_int_distribution<int> chance(0, 99);
  std::uniform_int_distribution<int> weight(-50, 50);
  for (std::size_t from = 0U; from < n; ++from) {
    for (std::size_t to = 0U; to < from; ++to) {
      if (chance(rng) < 33) graph.add_edge(from, to, weight(rng));
      if (chance(rng) < 7) graph.add_edge(from, to, weight(rng));
    }
  }
  return graph;
}

}  // namespace sprague_grundy_test_detail

TEST_CASE(sprague_grundy_deterministic_contracts) {
  using algorithms::games::analyze_impartial_dag;
  algorithms::graphs::Graph graph(3U, true);
  graph.add_edge(1U, 0U, 7);
  graph.add_edge(2U, 0U, -4);
  graph.add_edge(2U, 1U, 99);
  const auto empty = analyze_impartial_dag(algorithms::graphs::Graph(0U, true));
  REQUIRE(empty.grundy.empty());
  REQUIRE(empty.winning_move.empty());
  REQUIRE_EQ(empty.max_grundy, 0U);

  const auto result = analyze_impartial_dag(graph);
  REQUIRE_EQ(result.grundy, (std::vector<std::size_t>{0U, 1U, 2U}));
  REQUIRE(!result.winning_move[0].has_value());
  REQUIRE_EQ(result.winning_move[1], std::optional<std::size_t>{0U});
  REQUIRE_EQ(result.winning_move[2], std::optional<std::size_t>{0U});
  REQUIRE_EQ(result.max_grundy, 2U);

  algorithms::graphs::Graph deterministic_move(4U, true);
  deterministic_move.add_edge(3U, 1U);
  deterministic_move.add_edge(3U, 0U);
  deterministic_move.add_edge(2U, 0U);
  const auto deterministic_result = analyze_impartial_dag(deterministic_move);
  REQUIRE_EQ(deterministic_result.grundy[3U], 1U);
  REQUIRE_EQ(deterministic_result.winning_move[3U],
             std::optional<std::size_t>{1U});

  algorithms::graphs::Graph parallel(2U, true);
  parallel.add_edge(1U, 0U, 1);
  parallel.add_edge(1U, 0U, 999);
  REQUIRE_EQ(analyze_impartial_dag(parallel).grundy[1], 1U);

  algorithms::graphs::Graph cycle(2U, true);
  cycle.add_edge(0U, 1U);
  cycle.add_edge(1U, 0U);
  REQUIRE_THROWS_AS(analyze_impartial_dag(cycle), std::invalid_argument);

  algorithms::graphs::Graph self_loop(1U, true);
  self_loop.add_edge(0U, 0U);
  REQUIRE_THROWS_AS(analyze_impartial_dag(self_loop), std::invalid_argument);

  algorithms::graphs::Graph undirected(2U, false);
  undirected.add_edge(0U, 1U);
  REQUIRE_THROWS_AS(analyze_impartial_dag(undirected), std::invalid_argument);

  const std::vector<std::size_t> values{1U, 2U, 3U};
  REQUIRE_EQ(algorithms::games::sprague_grundy_nim_sum(values), 0U);
}

TEST_CASE(sprague_grundy_randomized_exact_product_game_oracle) {
  using namespace sprague_grundy_test_detail;
  std::mt19937_64 rng(0x5A7A6EULL);
  std::uniform_int_distribution<std::size_t> size_dist(1U, 10U);
  for (int trial = 0; trial < 350; ++trial) {
    const auto graph = random_dag(rng, size_dist(rng));
    const auto result = algorithms::games::analyze_impartial_dag(graph);
    std::size_t observed_max = 0U;
    for (algorithms::graphs::Vertex vertex = 0U;
         vertex < graph.vertex_count(); ++vertex) {
      REQUIRE_EQ(result.grundy[vertex],
                 independent_grundy_by_nim_equivalence(graph, vertex));
      if (result.grundy[vertex] > observed_max) observed_max = result.grundy[vertex];

      std::optional<algorithms::graphs::Vertex> first_zero_successor;
      for (const auto& edge : graph.neighbors(vertex)) {
        if (result.grundy[edge.to] == 0U) {
          first_zero_successor = edge.to;
          break;
        }
      }
      REQUIRE_EQ(result.winning_move[vertex],
                 result.grundy[vertex] == 0U
                     ? std::optional<algorithms::graphs::Vertex>{}
                     : first_zero_successor);
    }
    REQUIRE_EQ(result.max_grundy, observed_max);
  }
}

TEST_CASE(sprague_grundy_disjoint_sum_xor_matches_direct_product_game) {
  using namespace sprague_grundy_test_detail;
  std::mt19937_64 rng(0xD15A01A7ULL);
  std::uniform_int_distribution<std::size_t> size_dist(1U, 6U);
  for (int trial = 0; trial < 180; ++trial) {
    const auto first = random_dag(rng, size_dist(rng));
    const auto second = random_dag(rng, size_dist(rng));
    const auto first_result = algorithms::games::analyze_impartial_dag(first);
    const auto second_result = algorithms::games::analyze_impartial_dag(second);
    for (algorithms::graphs::Vertex first_vertex = 0U;
         first_vertex < first.vertex_count(); ++first_vertex) {
      for (algorithms::graphs::Vertex second_vertex = 0U;
           second_vertex < second.vertex_count(); ++second_vertex) {
        std::vector<std::vector<Mark>> memo(
            first.vertex_count(),
            std::vector<Mark>(second.vertex_count(), Mark::unknown));
        const bool direct = two_component_winning(
            first, first_vertex, second, second_vertex, memo);
        const bool xor_wins =
            (first_result.grundy[first_vertex] ^
             second_result.grundy[second_vertex]) != 0U;
        REQUIRE_EQ(direct, xor_wins);
      }
    }
  }
}
