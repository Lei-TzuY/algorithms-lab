#pragma once

#include "algorithms/graphs/general_matching.hpp"
#include "algorithms/graphs/weighted_matching.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace weighted_matching_recovery_tests {

using algorithms::graphs::BoundedWeightedMatchingResult;
using algorithms::graphs::Graph;
using algorithms::graphs::WeightedMatchingEdge;
using algorithms::graphs::bounded_exact_maximum_weight_matching;
using algorithms::graphs::edmonds_blossom_maximum_matching;

[[nodiscard]] inline std::int64_t exhaustive_edge_subset_optimum(
    const std::size_t vertex_count,
    const std::vector<WeightedMatchingEdge>& edges) {
  REQUIRE(edges.size() <= 20U);
  std::int64_t best = 0;
  const std::uint64_t limit = std::uint64_t{1} << edges.size();
  for (std::uint64_t subset = 0U; subset < limit; ++subset) {
    std::uint64_t used_vertices = 0U;
    std::int64_t total = 0;
    bool valid = true;
    for (std::size_t edge_index = 0U; edge_index < edges.size(); ++edge_index) {
      if ((subset & (std::uint64_t{1} << edge_index)) == 0U) {
        continue;
      }
      const WeightedMatchingEdge& edge = edges[edge_index];
      if (edge.first >= vertex_count || edge.second >= vertex_count ||
          edge.first == edge.second) {
        valid = false;
        break;
      }
      const std::uint64_t first_bit = std::uint64_t{1} << edge.first;
      const std::uint64_t second_bit = std::uint64_t{1} << edge.second;
      if ((used_vertices & (first_bit | second_bit)) != 0U) {
        valid = false;
        break;
      }
      used_vertices |= first_bit | second_bit;
      total += edge.weight;
    }
    if (valid && total > best) {
      best = total;
    }
  }
  return best;
}

inline void require_witness_replays(
    const std::size_t vertex_count,
    const std::vector<WeightedMatchingEdge>& edges,
    const BoundedWeightedMatchingResult& result) {
  REQUIRE_EQ(result.mate.size(), vertex_count);
  std::vector<bool> used(vertex_count, false);
  std::int64_t total = 0;
  for (const std::size_t edge_index : result.edge_indices) {
    REQUIRE(edge_index < edges.size());
    const WeightedMatchingEdge& edge = edges[edge_index];
    REQUIRE(edge.first < vertex_count);
    REQUIRE(edge.second < vertex_count);
    REQUIRE(edge.first != edge.second);
    REQUIRE(!used[edge.first]);
    REQUIRE(!used[edge.second]);
    used[edge.first] = true;
    used[edge.second] = true;
    REQUIRE(result.mate[edge.first].has_value());
    REQUIRE(result.mate[edge.second].has_value());
    REQUIRE_EQ(*result.mate[edge.first], edge.second);
    REQUIRE_EQ(*result.mate[edge.second], edge.first);
    total += edge.weight;
  }
  REQUIRE_EQ(total, result.total_weight);
  for (std::size_t vertex = 0U; vertex < vertex_count; ++vertex) {
    REQUIRE_EQ(result.mate[vertex].has_value(), used[vertex]);
  }
}

}  // namespace weighted_matching_recovery_tests

TEST_CASE(weighted_matching_handles_empty_negative_parallel_and_ties) {
  using namespace weighted_matching_recovery_tests;
  const auto empty = bounded_exact_maximum_weight_matching(0U, {});
  REQUIRE_EQ(empty.total_weight, 0);
  REQUIRE(empty.mate.empty());
  REQUIRE(empty.edge_indices.empty());

  const std::vector<WeightedMatchingEdge> edges{
      {0U, 1U, -5}, {0U, 1U, 7}, {1U, 0U, 7}, {1U, 1U, 100},
      {2U, 3U, 0}};
  const auto result = bounded_exact_maximum_weight_matching(4U, edges);
  REQUIRE_EQ(result.total_weight, 7);
  REQUIRE_EQ(result.edge_indices.size(), 1U);
  REQUIRE_EQ(result.edge_indices.front(), 1U);
  require_witness_replays(4U, edges, result);
}

TEST_CASE(weighted_matching_validates_bounds_and_exact_overflow_contract) {
  using namespace weighted_matching_recovery_tests;
  const std::vector<WeightedMatchingEdge> invalid{{0U, 2U, 1}};
  REQUIRE_THROWS_AS(bounded_exact_maximum_weight_matching(2U, invalid),
                    std::out_of_range);
  REQUIRE_THROWS_AS(bounded_exact_maximum_weight_matching(21U, {}),
                    std::length_error);

  const std::vector<WeightedMatchingEdge> maximum{
      {0U, 1U, std::numeric_limits<std::int64_t>::max()}};
  const auto exact = bounded_exact_maximum_weight_matching(2U, maximum);
  REQUIRE_EQ(exact.total_weight, std::numeric_limits<std::int64_t>::max());

  const std::vector<WeightedMatchingEdge> overflowing{
      {0U, 1U, std::numeric_limits<std::int64_t>::max()},
      {2U, 3U, std::numeric_limits<std::int64_t>::max()}};
  REQUIRE_THROWS_AS(bounded_exact_maximum_weight_matching(4U, overflowing),
                    std::overflow_error);

  const std::vector<WeightedMatchingEdge> minimum_only{
      {0U, 1U, std::numeric_limits<std::int64_t>::min()}};
  const auto unmatched = bounded_exact_maximum_weight_matching(2U, minimum_only);
  REQUIRE_EQ(unmatched.total_weight, 0);
  REQUIRE(unmatched.edge_indices.empty());
}

TEST_CASE(weighted_matching_matches_independent_edge_subset_oracle) {
  using namespace weighted_matching_recovery_tests;
  std::mt19937_64 random(0xA11CEB00CULL);
  for (std::size_t trial = 0U; trial < 600U; ++trial) {
    const std::size_t vertex_count = static_cast<std::size_t>(random() % 8U);
    const std::size_t edge_count =
        vertex_count == 0U ? 0U : static_cast<std::size_t>(random() % 13U);
    std::vector<WeightedMatchingEdge> edges;
    edges.reserve(edge_count);
    for (std::size_t index = 0U; index < edge_count; ++index) {
      const std::size_t first =
          static_cast<std::size_t>(random() % vertex_count);
      const std::size_t second =
          static_cast<std::size_t>(random() % vertex_count);
      const std::int64_t weight =
          static_cast<std::int64_t>(random() % 51U) - 20;
      edges.push_back({first, second, weight});
    }

    const auto result =
        bounded_exact_maximum_weight_matching(vertex_count, edges);
    REQUIRE_EQ(result.total_weight,
               exhaustive_edge_subset_optimum(vertex_count, edges));
    require_witness_replays(vertex_count, edges, result);
  }
}

TEST_CASE(weighted_matching_cross_checks_unit_weights_with_edmonds_blossom) {
  using namespace weighted_matching_recovery_tests;
  std::mt19937_64 random(0xB10550AULL);
  for (std::size_t trial = 0U; trial < 400U; ++trial) {
    const std::size_t vertex_count = static_cast<std::size_t>(random() % 11U);
    Graph graph(vertex_count, false);
    std::vector<WeightedMatchingEdge> edges;
    if (vertex_count != 0U) {
      const std::size_t edge_count = static_cast<std::size_t>(random() % 28U);
      edges.reserve(edge_count);
      for (std::size_t index = 0U; index < edge_count; ++index) {
        const std::size_t first =
            static_cast<std::size_t>(random() % vertex_count);
        const std::size_t second =
            static_cast<std::size_t>(random() % vertex_count);
        graph.add_edge(first, second, 1);
        edges.push_back({first, second, 1});
      }
    }

    const auto weighted =
        bounded_exact_maximum_weight_matching(vertex_count, edges);
    const auto cardinality = edmonds_blossom_maximum_matching(graph);
    REQUIRE_EQ(weighted.edge_indices.size(), cardinality.cardinality);
    REQUIRE_EQ(weighted.total_weight,
               static_cast<std::int64_t>(cardinality.cardinality));
  }
}
