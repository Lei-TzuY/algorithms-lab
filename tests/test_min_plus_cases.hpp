#pragma once

#include "algorithms/graphs/graph.hpp"
#include "algorithms/graphs/min_plus_matrix.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using algorithms::graphs::Graph;
using algorithms::graphs::MinPlusMatrix;
using algorithms::graphs::Weight;
using algorithms::graphs::exact_walk_distances_nonnegative;
using algorithms::graphs::min_plus_identity;
using algorithms::graphs::min_plus_power_nonnegative;
using algorithms::graphs::min_plus_product;

[[nodiscard]] MinPlusMatrix min_plus_exact_walk_oracle(const Graph& graph,
                                              std::size_t edge_count) {
  const std::size_t n = graph.vertex_count();
  MinPlusMatrix result(n, n);
  for (std::size_t source = 0; source < n; ++source) {
    std::vector<std::optional<Weight>> previous(n);
    previous[source] = Weight{0};
    for (std::size_t step = 0; step < edge_count; ++step) {
      std::vector<std::optional<Weight>> next(n);
      for (std::size_t from = 0; from < n; ++from) {
        if (!previous[from].has_value()) {
          continue;
        }
        for (const auto& edge : graph.neighbors(from)) {
          if (edge.weight < 0) {
            throw std::invalid_argument("oracle requires non-negative edges");
          }
          const Weight candidate = static_cast<Weight>(*previous[from] + edge.weight);
          if (!next[edge.to].has_value() || candidate < *next[edge.to]) {
            next[edge.to] = candidate;
          }
        }
      }
      previous = std::move(next);
    }
    for (std::size_t target = 0; target < n; ++target) {
      result.at(source, target) = previous[target];
    }
  }
  return result;
}

TEST_CASE(min_plus_matrix_shape_identity_and_product) {
  MinPlusMatrix left(2U, 3U,
                     {Weight{0}, Weight{4}, std::nullopt,
                      Weight{-2}, std::nullopt, Weight{5}});
  MinPlusMatrix right(3U, 2U,
                      {Weight{3}, Weight{1},
                       Weight{2}, std::nullopt,
                       Weight{-1}, Weight{7}});
  const MinPlusMatrix product = min_plus_product(left, right);
  REQUIRE_EQ(product.rows(), 2U);
  REQUIRE_EQ(product.columns(), 2U);
  REQUIRE_EQ(product.at(0U, 0U), MinPlusMatrix::Value{Weight{3}});
  REQUIRE_EQ(product.at(0U, 1U), MinPlusMatrix::Value{Weight{1}});
  REQUIRE_EQ(product.at(1U, 0U), MinPlusMatrix::Value{Weight{1}});
  REQUIRE_EQ(product.at(1U, 1U), MinPlusMatrix::Value{Weight{-1}});

  MinPlusMatrix empty_rows(0U, 5U);
  MinPlusMatrix empty_product = min_plus_product(empty_rows, MinPlusMatrix(5U, 3U));
  REQUIRE_EQ(empty_product.rows(), 0U);
  REQUIRE_EQ(empty_product.columns(), 3U);

  const MinPlusMatrix identity = min_plus_identity(3U);
  REQUIRE_EQ(identity.at(0U, 0U), MinPlusMatrix::Value{Weight{0}});
  REQUIRE(!identity.at(0U, 1U).has_value());

  REQUIRE_THROWS_AS(MinPlusMatrix(2U, 2U, {Weight{1}}), std::invalid_argument);
  REQUIRE_THROWS_AS(
      MinPlusMatrix(std::numeric_limits<std::size_t>::max(), 2U),
      std::length_error);
  REQUIRE_THROWS_AS(left.at(2U, 0U), std::out_of_range);
  REQUIRE_THROWS_AS(min_plus_product(MinPlusMatrix(2U, 3U), MinPlusMatrix(2U, 1U)),
                    std::invalid_argument);
}

TEST_CASE(min_plus_overflow_contract_preserves_representable_optimum) {
  constexpr Weight maximum = std::numeric_limits<Weight>::max();
  constexpr Weight minimum = std::numeric_limits<Weight>::min();

  const MinPlusMatrix left(1U, 2U, {maximum, Weight{2}});
  const MinPlusMatrix right(2U, 1U, {Weight{1}, Weight{3}});
  const MinPlusMatrix product = min_plus_product(left, right);
  REQUIRE_EQ(product.at(0U, 0U), MinPlusMatrix::Value{Weight{5}});

  REQUIRE_THROWS_AS(min_plus_product(MinPlusMatrix(1U, 1U, {maximum}),
                                     MinPlusMatrix(1U, 1U, {Weight{1}})),
                    std::overflow_error);
  REQUIRE_THROWS_AS(min_plus_product(MinPlusMatrix(1U, 1U, {minimum}),
                                     MinPlusMatrix(1U, 1U, {Weight{-1}})),
                    std::overflow_error);
}

TEST_CASE(min_plus_power_and_exact_walk_boundaries) {
  MinPlusMatrix adjacency(3U, 3U);
  adjacency.at(0U, 1U) = Weight{2};
  adjacency.at(1U, 2U) = Weight{3};
  adjacency.at(0U, 2U) = Weight{10};

  REQUIRE_EQ(min_plus_power_nonnegative(adjacency, 0U), min_plus_identity(3U));
  const MinPlusMatrix squared = min_plus_power_nonnegative(adjacency, 2U);
  REQUIRE_EQ(squared.at(0U, 2U), MinPlusMatrix::Value{Weight{5}});
  REQUIRE(!squared.at(0U, 1U).has_value());

  MinPlusMatrix deferred_overflow(4U, 4U);
  deferred_overflow.at(0U, 1U) = std::numeric_limits<Weight>::max();
  deferred_overflow.at(1U, 3U) = Weight{1};
  deferred_overflow.at(0U, 2U) = Weight{1};
  deferred_overflow.at(2U, 1U) = Weight{1};
  const MinPlusMatrix cubed = min_plus_power_nonnegative(deferred_overflow, 3U);
  REQUIRE_EQ(cubed.at(0U, 3U), MinPlusMatrix::Value{Weight{3}});

  MinPlusMatrix truly_overflowing(2U, 2U);
  truly_overflowing.at(0U, 1U) = std::numeric_limits<Weight>::max();
  truly_overflowing.at(1U, 1U) = Weight{1};
  REQUIRE_THROWS_AS(min_plus_power_nonnegative(truly_overflowing, 2U),
                    std::overflow_error);

  MinPlusMatrix negative(1U, 1U, {Weight{-1}});
  REQUIRE_THROWS_AS(min_plus_power_nonnegative(negative, 2U), std::invalid_argument);
  REQUIRE_THROWS_AS(min_plus_power_nonnegative(MinPlusMatrix(1U, 2U), 2U),
                    std::invalid_argument);

  Graph graph(3U, true);
  graph.add_edge(0U, 1U, 7);
  graph.add_edge(0U, 1U, 2);
  graph.add_edge(1U, 2U, 3);
  graph.add_edge(0U, 2U, 20);
  graph.add_edge(2U, 2U, 1);
  const MinPlusMatrix exactly_two = exact_walk_distances_nonnegative(graph, 2U);
  REQUIRE_EQ(exactly_two.at(0U, 2U), MinPlusMatrix::Value{Weight{5}});
  REQUIRE_EQ(exactly_two.at(2U, 2U), MinPlusMatrix::Value{Weight{2}});

  Graph negative_graph(2U, true);
  negative_graph.add_edge(0U, 1U, -1);
  REQUIRE_THROWS_AS(exact_walk_distances_nonnegative(negative_graph, 0U),
                    std::invalid_argument);
}

TEST_CASE(min_plus_randomized_fixed_edge_walk_differential) {
  std::mt19937_64 generator(0x6D696E706C7573ULL);
  std::uniform_int_distribution<int> vertex_count_distribution(0, 7);
  std::uniform_int_distribution<int> weight_distribution(0, 25);
  std::bernoulli_distribution directed_distribution(0.5);
  std::bernoulli_distribution edge_distribution(0.3);
  std::bernoulli_distribution duplicate_distribution(0.12);
  std::uniform_int_distribution<int> edge_count_distribution(0, 9);

  for (std::size_t trial = 0; trial < 500U; ++trial) {
    const std::size_t n = static_cast<std::size_t>(vertex_count_distribution(generator));
    Graph graph(n, directed_distribution(generator));
    for (std::size_t from = 0; from < n; ++from) {
      for (std::size_t to = 0; to < n; ++to) {
        if (!edge_distribution(generator)) {
          continue;
        }
        graph.add_edge(from, to, static_cast<Weight>(weight_distribution(generator)));
        if (duplicate_distribution(generator)) {
          graph.add_edge(from, to,
                         static_cast<Weight>(weight_distribution(generator)));
        }
      }
    }
    const std::size_t edge_count =
        static_cast<std::size_t>(edge_count_distribution(generator));
    const MinPlusMatrix actual = exact_walk_distances_nonnegative(
        graph, static_cast<std::uint64_t>(edge_count));
    const MinPlusMatrix expected = min_plus_exact_walk_oracle(graph, edge_count);
    REQUIRE_EQ(actual, expected);
  }
}

}  // namespace
