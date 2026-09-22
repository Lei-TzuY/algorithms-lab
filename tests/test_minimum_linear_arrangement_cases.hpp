#pragma once

#include "algorithms/graphs/minimum_linear_arrangement.hpp"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

using algorithms::graphs::MinimumLinearArrangementResult;
using algorithms::graphs::minimum_linear_arrangement;

namespace minimum_linear_arrangement_test_detail {

inline std::size_t direct_arrangement_cost(
    const std::size_t vertex_count,
    const std::vector<std::pair<std::size_t, std::size_t>>& edges,
    const std::vector<std::size_t>& ordering) {
  REQUIRE_EQ(ordering.size(), vertex_count);

  std::vector<std::size_t> position(vertex_count, vertex_count);
  for (std::size_t index = 0U; index < ordering.size(); ++index) {
    REQUIRE(ordering[index] < vertex_count);
    REQUIRE_EQ(position[ordering[index]], vertex_count);
    position[ordering[index]] = index;
  }

  std::size_t total = 0U;
  for (const auto& [first, second] : edges) {
    const std::size_t first_position = position[first];
    const std::size_t second_position = position[second];
    total += first_position > second_position
                 ? first_position - second_position
                 : second_position - first_position;
  }
  return total;
}

inline MinimumLinearArrangementResult brute_force_minla(
    const std::size_t vertex_count,
    const std::vector<std::pair<std::size_t, std::size_t>>& edges) {
  std::vector<std::size_t> ordering(vertex_count);
  std::iota(ordering.begin(), ordering.end(), 0U);

  MinimumLinearArrangementResult best{
      std::numeric_limits<std::size_t>::max(), {}};

  do {
    const std::size_t cost =
        direct_arrangement_cost(vertex_count, edges, ordering);
    // next_permutation enumerates lexicographically, so the first optimum is
    // exactly the lexicographically smallest optimum.
    if (cost < best.cost) {
      best = MinimumLinearArrangementResult{cost, ordering};
    }
  } while (std::next_permutation(ordering.begin(), ordering.end()));

  if (vertex_count == 0U) {
    return MinimumLinearArrangementResult{0U, {}};
  }
  return best;
}

inline std::vector<std::pair<std::size_t, std::size_t>>
edge_list_from_mask(
    const std::size_t vertex_count,
    const std::uint64_t edge_mask) {
  std::vector<std::pair<std::size_t, std::size_t>> edges;
  std::size_t bit = 0U;
  for (std::size_t first = 0U; first < vertex_count; ++first) {
    for (std::size_t second = first + 1U;
         second < vertex_count; ++second) {
      if ((edge_mask & (UINT64_C(1) << bit)) != 0U) {
        edges.emplace_back(first, second);
      }
      ++bit;
    }
  }
  return edges;
}

}  // namespace minimum_linear_arrangement_test_detail

TEST_CASE(minimum_linear_arrangement_empty_single_and_edgeless) {
  REQUIRE(
      minimum_linear_arrangement(0U, {}) ==
      MinimumLinearArrangementResult{0U, {}});

  REQUIRE(
      minimum_linear_arrangement(1U, {}) ==
      MinimumLinearArrangementResult{0U, {0U}});

  REQUIRE(
      minimum_linear_arrangement(5U, {}) ==
      MinimumLinearArrangementResult{
          0U, {0U, 1U, 2U, 3U, 4U}});
}

TEST_CASE(minimum_linear_arrangement_known_path_and_complete_graph) {
  const std::vector<std::pair<std::size_t, std::size_t>> path{
      {0U, 1U}, {1U, 2U}, {2U, 3U}};
  REQUIRE(
      minimum_linear_arrangement(4U, path) ==
      MinimumLinearArrangementResult{
          3U, {0U, 1U, 2U, 3U}});

  std::vector<std::pair<std::size_t, std::size_t>> complete;
  for (std::size_t first = 0U; first < 4U; ++first) {
    for (std::size_t second = first + 1U; second < 4U; ++second) {
      complete.emplace_back(first, second);
    }
  }
  REQUIRE(
      minimum_linear_arrangement(4U, complete) ==
      MinimumLinearArrangementResult{
          10U, {0U, 1U, 2U, 3U}});
}

TEST_CASE(minimum_linear_arrangement_rejects_invalid_simple_graphs) {
  REQUIRE_THROWS_AS(
      minimum_linear_arrangement(23U, {}),
      std::length_error);
  REQUIRE_THROWS_AS(
      minimum_linear_arrangement(3U, {{0U, 3U}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      minimum_linear_arrangement(3U, {{1U, 1U}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      minimum_linear_arrangement(
          3U, {{0U, 2U}, {2U, 0U}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      minimum_linear_arrangement(
          3U, {{0U, 2U}, {0U, 2U}}),
      std::invalid_argument);
}

TEST_CASE(minimum_linear_arrangement_all_graphs_through_five_vertices) {
  using namespace minimum_linear_arrangement_test_detail;

  for (std::size_t vertex_count = 0U;
       vertex_count <= 5U; ++vertex_count) {
    const std::size_t possible_edges =
        vertex_count * (vertex_count - (vertex_count == 0U ? 0U : 1U)) / 2U;
    const std::uint64_t graph_count =
        UINT64_C(1) << possible_edges;

    for (std::uint64_t graph = 0U;
         graph < graph_count; ++graph) {
      const auto edges =
          edge_list_from_mask(vertex_count, graph);
      const auto expected =
          brute_force_minla(vertex_count, edges);
      const auto actual =
          minimum_linear_arrangement(vertex_count, edges);

      REQUIRE(actual == expected);
      REQUIRE_EQ(
          direct_arrangement_cost(
              vertex_count, edges, actual.ordering),
          actual.cost);
    }
  }
}

TEST_CASE(minimum_linear_arrangement_random_small_graphs_match_permutations) {
  using namespace minimum_linear_arrangement_test_detail;

  std::mt19937_64 random(0x4D494E4C41554C4CULL);
  for (std::size_t trial = 0U; trial < 140U; ++trial) {
    const std::size_t vertex_count =
        static_cast<std::size_t>(random() % 8U);

    std::vector<std::pair<std::size_t, std::size_t>> edges;
    for (std::size_t first = 0U;
         first < vertex_count; ++first) {
      for (std::size_t second = first + 1U;
           second < vertex_count; ++second) {
        if ((random() & 3ULL) != 0ULL) {
          edges.emplace_back(first, second);
        }
      }
    }

    const auto expected =
        brute_force_minla(vertex_count, edges);
    const auto actual =
        minimum_linear_arrangement(vertex_count, edges);

    REQUIRE(actual == expected);
  }
}

TEST_CASE(minimum_linear_arrangement_medium_graph_replays_exact_cost) {
  using namespace minimum_linear_arrangement_test_detail;

  constexpr std::size_t vertex_count = 14U;
  std::mt19937_64 random(0xC07C07C07ULL);
  std::vector<std::pair<std::size_t, std::size_t>> edges;
  for (std::size_t first = 0U;
       first < vertex_count; ++first) {
    for (std::size_t second = first + 1U;
         second < vertex_count; ++second) {
      if ((random() % 5U) <= 1U) {
        edges.emplace_back(first, second);
      }
    }
  }

  const auto result =
      minimum_linear_arrangement(vertex_count, edges);
  REQUIRE_EQ(result.ordering.size(), vertex_count);
  REQUIRE_EQ(
      direct_arrangement_cost(
          vertex_count, edges, result.ordering),
      result.cost);
}
