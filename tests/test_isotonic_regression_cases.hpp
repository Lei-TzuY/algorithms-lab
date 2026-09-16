#pragma once

#include "algorithms/optimization/isotonic_regression.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <vector>

namespace isotonic_regression_test_detail {

using algorithms::optimization::IsotonicRegressionBlock;
using algorithms::optimization::IsotonicRegressionResult;
using algorithms::optimization::least_squares_isotonic_regression;

inline std::vector<long double> fitted_values(
    const IsotonicRegressionResult& result) {
  std::vector<long double> fitted(result.observation_count, 0.0L);
  std::size_t cursor = 0U;
  for (const auto& block : result.blocks) {
    REQUIRE_EQ(block.begin, cursor);
    REQUIRE(block.end > block.begin);
    REQUIRE_EQ(block.count, block.end - block.begin);
    const long double mean = static_cast<long double>(block.sum) /
                             static_cast<long double>(block.count);
    for (std::size_t index = block.begin; index < block.end; ++index) {
      fitted[index] = mean;
    }
    cursor = block.end;
  }
  REQUIRE_EQ(cursor, result.observation_count);
  return fitted;
}

inline long double squared_error(std::span<const std::int64_t> values,
                                 const std::vector<long double>& fitted) {
  long double total = 0.0L;
  for (std::size_t index = 0; index < values.size(); ++index) {
    const long double delta =
        static_cast<long double>(values[index]) - fitted[index];
    total += delta * delta;
  }
  return total;
}

inline std::vector<long double> exhaustive_partition_oracle(
    std::span<const std::int64_t> values) {
  const std::size_t n = values.size();
  if (n == 0U) {
    return {};
  }

  const std::size_t cut_count = n - 1U;
  const std::uint64_t mask_count = std::uint64_t{1} << cut_count;
  long double best = std::numeric_limits<long double>::infinity();
  std::vector<long double> best_fit;

  for (std::uint64_t mask = 0; mask < mask_count; ++mask) {
    std::vector<long double> fit(n, 0.0L);
    std::size_t begin = 0U;
    long double previous_mean = -std::numeric_limits<long double>::infinity();
    bool valid = true;
    long double objective = 0.0L;

    while (begin < n) {
      std::size_t end = begin + 1U;
      while (end < n &&
             ((mask >> (end - 1U)) & std::uint64_t{1}) == 0U) {
        ++end;
      }

      std::int64_t sum = 0;
      for (std::size_t index = begin; index < end; ++index) {
        sum += values[index];
      }
      const long double mean =
          static_cast<long double>(sum) / static_cast<long double>(end - begin);
      if (mean < previous_mean) {
        valid = false;
        break;
      }
      previous_mean = mean;
      for (std::size_t index = begin; index < end; ++index) {
        fit[index] = mean;
        const long double delta = static_cast<long double>(values[index]) - mean;
        objective += delta * delta;
      }
      begin = end;
    }

    if (valid && objective < best) {
      best = objective;
      best_fit = std::move(fit);
    }
  }

  REQUIRE(!best_fit.empty());
  return best_fit;
}

}  // namespace isotonic_regression_test_detail

TEST_CASE(isotonic_regression_deterministic_exact_blocks) {
  using namespace isotonic_regression_test_detail;

  {
    const std::vector<std::int64_t> input;
    const auto result = least_squares_isotonic_regression(input);
    REQUIRE_EQ(result.observation_count, 0U);
    REQUIRE(result.blocks.empty());
  }
  {
    const std::vector<std::int64_t> input{1, 1, 2, 2};
    const auto result = least_squares_isotonic_regression(input);
    REQUIRE_EQ(result.blocks,
               (std::vector<IsotonicRegressionBlock>{
                   {0U, 2U, 2, 2U}, {2U, 4U, 4, 2U}}));
  }
  {
    const std::vector<std::int64_t> input{4, 3, 2, 1};
    const auto result = least_squares_isotonic_regression(input);
    REQUIRE_EQ(result.blocks,
               (std::vector<IsotonicRegressionBlock>{{0U, 4U, 10, 4U}}));
  }
  {
    const std::vector<std::int64_t> input{3, 1, 2, 8, 4, 6};
    const auto result = least_squares_isotonic_regression(input);
    const auto fitted = fitted_values(result);
    REQUIRE(std::is_sorted(fitted.begin(), fitted.end()));
  }
}

TEST_CASE(isotonic_regression_rejects_unrepresentable_domain) {
  using namespace isotonic_regression_test_detail;
  const std::vector<std::int64_t> too_large{1'000'000'001LL};
  REQUIRE_THROWS_AS(least_squares_isotonic_regression(too_large),
                    std::invalid_argument);

  const std::vector<std::int64_t> too_long(50'001U, 0);
  REQUIRE_THROWS_AS(least_squares_isotonic_regression(too_long),
                    std::length_error);
}

TEST_CASE(isotonic_regression_randomized_against_exhaustive_partitions) {
  using namespace isotonic_regression_test_detail;

  std::mt19937_64 rng(0x15070A1CULL);
  std::uniform_int_distribution<int> length_dist(1, 9);
  std::uniform_int_distribution<int> value_dist(-5, 5);

  for (int trial = 0; trial < 700; ++trial) {
    const std::size_t n = static_cast<std::size_t>(length_dist(rng));
    std::vector<std::int64_t> input(n);
    for (auto& value : input) {
      value = static_cast<std::int64_t>(value_dist(rng));
    }

    const auto result = least_squares_isotonic_regression(input);
    const auto actual = fitted_values(result);
    const auto expected = exhaustive_partition_oracle(input);
    REQUIRE(std::is_sorted(actual.begin(), actual.end()));
    REQUIRE(std::abs(squared_error(input, actual) -
                     squared_error(input, expected)) < 1.0e-12L);

    for (std::size_t index = 1U; index < result.blocks.size(); ++index) {
      const auto& left = result.blocks[index - 1U];
      const auto& right = result.blocks[index];
      const std::int64_t lhs =
          left.sum * static_cast<std::int64_t>(right.count);
      const std::int64_t rhs =
          right.sum * static_cast<std::int64_t>(left.count);
      REQUIRE(lhs < rhs);
    }
  }
}
