#pragma once

#include "algorithms/combinatorial/consecutive_ones.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <optional>
#include <random>
#include <vector>

namespace {

using algorithms::combinatorial::ConsecutiveOnesResult;
using algorithms::combinatorial::consecutive_ones_ordering;

[[nodiscard]] bool c1p_valid_order(
    std::size_t column_count,
    const std::vector<std::vector<std::size_t>>& rows,
    const std::vector<std::size_t>& order) {
  if (order.size() != column_count) {
    return false;
  }
  std::vector<std::size_t> position(column_count, 0U);
  std::vector<bool> seen(column_count, false);
  for (std::size_t index = 0; index < order.size(); ++index) {
    if (order[index] >= column_count || seen[order[index]]) {
      return false;
    }
    seen[order[index]] = true;
    position[order[index]] = index;
  }
  for (const auto& row : rows) {
    if (row.empty()) {
      continue;
    }
    std::size_t first = column_count;
    std::size_t last = 0U;
    for (const std::size_t column : row) {
      first = std::min(first, position[column]);
      last = std::max(last, position[column]);
    }
    if (last - first + 1U != row.size()) {
      return false;
    }
  }
  return true;
}

[[nodiscard]] std::optional<std::vector<std::size_t>> c1p_exhaustive_order(
    std::size_t column_count,
    const std::vector<std::vector<std::size_t>>& rows) {
  std::vector<std::size_t> order(column_count);
  std::iota(order.begin(), order.end(), 0U);
  do {
    if (c1p_valid_order(column_count, rows, order)) {
      return order;
    }
  } while (std::next_permutation(order.begin(), order.end()));
  return std::nullopt;
}

}  // namespace

TEST_CASE(consecutive_ones_validates_domain_and_trivial_rows) {
  const auto empty = consecutive_ones_ordering(0U, {{}});
  REQUIRE(empty.has_value());
  REQUIRE(empty->column_order.empty());

  const std::vector<std::vector<std::size_t>> rows{
      {}, {0U}, {0U, 1U, 2U, 3U}, {1U, 2U}, {1U, 2U}};
  const auto result = consecutive_ones_ordering(4U, rows);
  REQUIRE(result.has_value());
  REQUIRE_EQ(result->column_order,
             (std::vector<std::size_t>{0U, 1U, 2U, 3U}));
  REQUIRE(c1p_valid_order(4U, rows, result->column_order));

  REQUIRE_THROWS_AS(consecutive_ones_ordering(21U, {}), std::length_error);
  REQUIRE_THROWS_AS(consecutive_ones_ordering(3U, {{3U}}), std::out_of_range);
  REQUIRE_THROWS_AS(consecutive_ones_ordering(3U, {{1U, 1U}}),
                    std::invalid_argument);
}

TEST_CASE(consecutive_ones_rejects_incompatible_pair_constraints) {
  const std::vector<std::vector<std::size_t>> triangle{
      {0U, 1U}, {1U, 2U}, {0U, 2U}};
  REQUIRE(!consecutive_ones_ordering(3U, triangle).has_value());

  const std::vector<std::vector<std::size_t>> cycle{
      {0U, 1U}, {1U, 2U}, {2U, 3U}, {3U, 0U}};
  REQUIRE(!consecutive_ones_ordering(4U, cycle).has_value());
}

TEST_CASE(consecutive_ones_handles_public_twenty_column_boundary) {
  std::vector<std::vector<std::size_t>> rows;
  for (std::size_t column = 0; column + 1U < 20U; ++column) {
    rows.push_back({column, column + 1U});
  }
  const auto result = consecutive_ones_ordering(20U, rows);
  REQUIRE(result.has_value());
  std::vector<std::size_t> expected(20U);
  std::iota(expected.begin(), expected.end(), 0U);
  REQUIRE_EQ(result->column_order, expected);
  REQUIRE(c1p_valid_order(20U, rows, result->column_order));
  REQUIRE(result->explored_states <= (std::size_t{1} << 20U));
}

TEST_CASE(consecutive_ones_matches_exhaustive_permutation_oracle) {
  std::mt19937_64 rng(0xC01EC0715EEDULL);
  for (std::size_t trial = 0; trial < 320U; ++trial) {
    const std::size_t column_count = static_cast<std::size_t>(rng() % 9U);
    const std::size_t row_count = static_cast<std::size_t>(rng() % 13U);
    std::vector<std::vector<std::size_t>> rows;
    rows.reserve(row_count);
    for (std::size_t row_index = 0; row_index < row_count; ++row_index) {
      std::vector<std::size_t> row;
      for (std::size_t column = 0; column < column_count; ++column) {
        if ((rng() & 3U) == 0U) {
          row.push_back(column);
        }
      }
      rows.push_back(std::move(row));
    }

    const auto result = consecutive_ones_ordering(column_count, rows);
    const auto oracle = c1p_exhaustive_order(column_count, rows);
    REQUIRE_EQ(result.has_value(), oracle.has_value());
    if (result.has_value()) {
      REQUIRE(c1p_valid_order(column_count, rows, result->column_order));
      REQUIRE_EQ(result->column_order, *oracle);
      REQUIRE(result->explored_states <= (std::size_t{1} << column_count));
    }
  }
}
