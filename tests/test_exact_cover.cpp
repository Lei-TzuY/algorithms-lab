#include "algorithms/combinatorial/exact_cover.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using algorithms::combinatorial::ExactCoverSolution;
using algorithms::combinatorial::solve_exact_cover;

[[nodiscard]] bool witness_is_exact(
    std::size_t column_count,
    const std::vector<std::vector<std::size_t>>& rows,
    const ExactCoverSolution& solution) {
  std::vector<std::size_t> counts(column_count, 0);
  std::vector<bool> used(rows.size(), false);
  for (const std::size_t row : solution.selected_rows) {
    if (row >= rows.size() || used[row]) {
      return false;
    }
    used[row] = true;
    for (const std::size_t column : rows[row]) {
      if (column >= column_count) {
        return false;
      }
      ++counts[column];
    }
  }
  return std::all_of(counts.begin(), counts.end(),
                     [](std::size_t count) { return count == 1; });
}

[[nodiscard]] bool brute_force_exists(
    std::size_t column_count,
    const std::vector<std::vector<std::size_t>>& rows) {
  if (rows.size() >= 63) {
    throw std::logic_error("exact-cover oracle row bound exceeded");
  }

  const std::uint64_t subset_count = std::uint64_t{1} << rows.size();
  for (std::uint64_t mask = 0; mask < subset_count; ++mask) {
    std::vector<unsigned char> counts(column_count, 0);
    bool valid = true;
    for (std::size_t row = 0; row < rows.size() && valid; ++row) {
      if ((mask & (std::uint64_t{1} << row)) == 0) {
        continue;
      }
      for (const std::size_t column : rows[row]) {
        ++counts[column];
        if (counts[column] > 1) {
          valid = false;
          break;
        }
      }
    }
    if (valid && std::all_of(counts.begin(), counts.end(),
                             [](unsigned char count) { return count == 1; })) {
      return true;
    }
  }
  return false;
}

TEST_CASE(exact_cover_classic_instance_and_edge_cases) {
  const std::vector<std::vector<std::size_t>> classic{
      {0, 3, 6}, {0, 3}, {3, 4, 6}, {2, 4, 5}, {1, 2, 5, 6}, {1, 6}};
  const auto solution = solve_exact_cover(7, classic);
  REQUIRE(solution.has_value());
  REQUIRE(witness_is_exact(7, classic, *solution));
  REQUIRE_EQ(solution->selected_rows, (std::vector<std::size_t>{1, 3, 5}));

  const auto zero_columns = solve_exact_cover(0, {});
  REQUIRE(zero_columns.has_value());
  REQUIRE(zero_columns->selected_rows.empty());

  REQUIRE(!solve_exact_cover(2, {{0}, {0}}).has_value());
  REQUIRE(solve_exact_cover(1, {{}, {0}}).has_value());
}

TEST_CASE(exact_cover_restores_links_after_failed_branch) {
  const std::vector<std::vector<std::size_t>> rows{{0, 1}, {0}, {1, 2}, {1, 2}};
  const auto solution = solve_exact_cover(3, rows);
  REQUIRE(solution.has_value());
  REQUIRE_EQ(solution->selected_rows, (std::vector<std::size_t>{1, 2}));
  REQUIRE(witness_is_exact(3, rows, *solution));
}

TEST_CASE(exact_cover_validation_rejects_invalid_rows) {
  REQUIRE_THROWS_AS(solve_exact_cover(3, {{0, 1, 1}}), std::invalid_argument);
  REQUIRE_THROWS_AS(solve_exact_cover(2, {{2}}), std::out_of_range);
  REQUIRE_THROWS_AS(
      solve_exact_cover(std::numeric_limits<std::size_t>::max(), {}),
      std::length_error);
}

TEST_CASE(exact_cover_is_deterministic) {
  const std::vector<std::vector<std::size_t>> rows{
      {0, 1}, {2, 3}, {0, 2}, {1, 3}};
  const auto first = solve_exact_cover(4, rows);
  const auto second = solve_exact_cover(4, rows);
  REQUIRE_EQ(first, second);
  REQUIRE(first.has_value());
  REQUIRE(witness_is_exact(4, rows, *first));
}

TEST_CASE(exact_cover_randomized_differential_against_subset_enumeration) {
  std::mt19937_64 rng(0xD1A6C0DEULL);
  std::uniform_int_distribution<int> column_dist(0, 8);
  std::uniform_int_distribution<int> row_dist(0, 12);
  std::bernoulli_distribution include_column(0.32);

  for (int trial = 0; trial < 2000; ++trial) {
    const std::size_t column_count =
        static_cast<std::size_t>(column_dist(rng));
    const std::size_t row_count = static_cast<std::size_t>(row_dist(rng));
    std::vector<std::vector<std::size_t>> rows(row_count);
    for (std::size_t row = 0; row < row_count; ++row) {
      for (std::size_t column = 0; column < column_count; ++column) {
        if (include_column(rng)) {
          rows[row].push_back(column);
        }
      }
    }

    const bool expected = brute_force_exists(column_count, rows);
    const auto actual = solve_exact_cover(column_count, rows);
    REQUIRE_EQ(actual.has_value(), expected);
    if (actual.has_value()) {
      REQUIRE(witness_is_exact(column_count, rows, *actual));
    }
    REQUIRE_EQ(actual, solve_exact_cover(column_count, rows));
  }
}

}  // namespace
