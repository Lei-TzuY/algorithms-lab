#pragma once

#include "algorithms/coding/alphabetic_code.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <string>
#include <vector>

namespace alphabetic_code_test_detail {

using algorithms::coding::AlphabeticCodeResult;
using algorithms::coding::WideUnsignedCost;

inline std::uint64_t oracle_cost(const std::vector<std::uint64_t>& weights) {
  const std::size_t n = weights.size();
  if (n <= 1U) return 0U;
  std::vector<std::uint64_t> prefix(n + 1U, 0U);
  for (std::size_t i = 0; i < n; ++i) prefix[i + 1U] = prefix[i] + weights[i];
  std::vector<std::vector<std::uint64_t>> dp(n + 1U, std::vector<std::uint64_t>(n + 1U, 0U));
  for (std::size_t length = 2U; length <= n; ++length) {
    for (std::size_t begin = 0U; begin + length <= n; ++begin) {
      const std::size_t end = begin + length;
      const std::uint64_t sum = prefix[end] - prefix[begin];
      std::uint64_t best = std::numeric_limits<std::uint64_t>::max();
      for (std::size_t split = begin + 1U; split < end; ++split) {
        best = std::min(best, dp[begin][split] + dp[split][end]);
      }
      dp[begin][end] = best + sum;
    }
  }
  return dp[0][n];
}

inline bool prefix_of(const std::string& first, const std::string& second) {
  return first.size() <= second.size() &&
         std::equal(first.begin(), first.end(), second.begin());
}

inline void require_valid_result(const std::vector<std::uint64_t>& weights,
                                 const AlphabeticCodeResult& result) {
  REQUIRE_EQ(result.depths.size(), weights.size());
  REQUIRE_EQ(result.codewords.size(), weights.size());
  if (weights.empty()) {
    REQUIRE_EQ(result.merge_count, std::size_t{0});
    return;
  }
  REQUIRE_EQ(result.merge_count, weights.size() - 1U);
  for (std::size_t i = 0; i < weights.size(); ++i) {
    REQUIRE_EQ(result.depths[i], result.codewords[i].size());
    for (const char bit : result.codewords[i]) REQUIRE(bit == '0' || bit == '1');
  }
  if (weights.size() == 1U) {
    REQUIRE(result.codewords[0].empty());
    return;
  }
  for (std::size_t i = 0; i < weights.size(); ++i) {
    for (std::size_t j = 0; j < weights.size(); ++j) {
      if (i == j) continue;
      REQUIRE(!prefix_of(result.codewords[i], result.codewords[j]));
    }
    if (i + 1U < weights.size()) REQUIRE(result.codewords[i] < result.codewords[i + 1U]);
  }
}

}  // namespace alphabetic_code_test_detail

TEST_CASE(alphabetic_code_boundaries_and_exact_wide_cost) {
  using algorithms::coding::optimal_alphabetic_prefix_code;
  using algorithms::coding::WideUnsignedCost;

  const std::vector<std::uint64_t> empty;
  const auto empty_result = optimal_alphabetic_prefix_code(empty);
  REQUIRE(empty_result.depths.empty());
  REQUIRE(empty_result.codewords.empty());
  REQUIRE_EQ(empty_result.weighted_path_length, (WideUnsignedCost{0U, 0U}));

  const std::vector<std::uint64_t> singleton{std::numeric_limits<std::uint64_t>::max()};
  const auto singleton_result = optimal_alphabetic_prefix_code(singleton);
  alphabetic_code_test_detail::require_valid_result(singleton, singleton_result);
  REQUIRE_EQ(singleton_result.weighted_path_length, (WideUnsignedCost{0U, 0U}));

  const std::vector<std::uint64_t> pair{std::numeric_limits<std::uint64_t>::max(),
                                       std::numeric_limits<std::uint64_t>::max()};
  const auto pair_result = optimal_alphabetic_prefix_code(pair);
  alphabetic_code_test_detail::require_valid_result(pair, pair_result);
  REQUIRE_EQ(pair_result.weighted_path_length,
             (WideUnsignedCost{1U, std::numeric_limits<std::uint64_t>::max() - 1U}));

  std::vector<std::uint64_t> too_many(4097U, 1U);
  REQUIRE_THROWS_AS(optimal_alphabetic_prefix_code(too_many), std::length_error);
}

TEST_CASE(alphabetic_code_deterministic_zero_and_skewed_weights) {
  using algorithms::coding::optimal_alphabetic_prefix_code;
  const std::vector<std::vector<std::uint64_t>> cases{
      {0U, 0U, 0U, 0U, 0U}, {100U, 1U, 1U, 1U}, {1U, 1U, 1U, 100U},
      {45U, 13U, 12U, 16U, 9U, 5U}};
  for (const auto& weights : cases) {
    const auto first = optimal_alphabetic_prefix_code(weights);
    const auto second = optimal_alphabetic_prefix_code(weights);
    alphabetic_code_test_detail::require_valid_result(weights, first);
    REQUIRE(first.depths == second.depths);
    REQUIRE(first.codewords == second.codewords);
    REQUIRE_EQ(first.weighted_path_length, second.weighted_path_length);
    REQUIRE_EQ(first.weighted_path_length.high, std::uint64_t{0});
    REQUIRE_EQ(first.weighted_path_length.low,
               alphabetic_code_test_detail::oracle_cost(weights));
  }
}

TEST_CASE(alphabetic_code_randomized_differential_against_interval_dp) {
  using algorithms::coding::optimal_alphabetic_prefix_code;
  std::mt19937_64 rng(0xA17FAB37ULL);
  for (std::size_t trial = 0; trial < 900U; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 10U);
    std::vector<std::uint64_t> weights(n);
    for (auto& weight : weights) weight = rng() % 31U;

    const auto result = optimal_alphabetic_prefix_code(weights);
    alphabetic_code_test_detail::require_valid_result(weights, result);
    REQUIRE_EQ(result.weighted_path_length.high, std::uint64_t{0});
    REQUIRE_EQ(result.weighted_path_length.low,
               alphabetic_code_test_detail::oracle_cost(weights));
  }
}

TEST_CASE(alphabetic_code_output_depths_replay_weighted_cost) {
  using algorithms::coding::optimal_alphabetic_prefix_code;
  std::mt19937_64 rng(0xC0DEAB1EULL);
  for (std::size_t trial = 0; trial < 300U; ++trial) {
    const std::size_t n = 2U + static_cast<std::size_t>(rng() % 127U);
    std::vector<std::uint64_t> weights(n);
    for (auto& weight : weights) weight = rng() % 100000U;
    const auto result = optimal_alphabetic_prefix_code(weights);
    alphabetic_code_test_detail::require_valid_result(weights, result);
    std::uint64_t replay = 0U;
    for (std::size_t i = 0; i < n; ++i) {
      replay += weights[i] * static_cast<std::uint64_t>(result.depths[i]);
    }
    REQUIRE_EQ(result.weighted_path_length.high, std::uint64_t{0});
    REQUIRE_EQ(result.weighted_path_length.low, replay);
  }
}
