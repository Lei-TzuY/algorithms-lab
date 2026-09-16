#pragma once

#include "algorithms/combinatorial/schroeppel_shamir_subset_sum.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <vector>

namespace schroeppel_shamir_test_detail {

using algorithms::combinatorial::SubsetSumWitness;
using algorithms::combinatorial::schroeppel_shamir_subset_sum;
using algorithms::combinatorial::valid_subset_sum_witness;

inline bool exhaustive_exists(std::span<const std::int64_t> values,
                              std::int64_t target) {
  const std::size_t count = std::size_t{1} << values.size();
  for (std::size_t mask = 0; mask < count; ++mask) {
    std::int64_t sum = 0;
    for (std::size_t bit = 0; bit < values.size(); ++bit) {
      if ((mask & (std::size_t{1} << bit)) != 0U) {
        sum += values[bit];
      }
    }
    if (sum == target) {
      return true;
    }
  }
  return false;
}

inline void enumerate_half_sums(std::span<const std::int64_t> values,
                                std::size_t begin, std::size_t count,
                                std::vector<std::int64_t>& output) {
  const std::size_t combinations = std::size_t{1} << count;
  output.clear();
  output.reserve(combinations);
  for (std::size_t mask = 0; mask < combinations; ++mask) {
    std::int64_t sum = 0;
    for (std::size_t bit = 0; bit < count; ++bit) {
      if ((mask & (std::size_t{1} << bit)) != 0U) {
        sum += values[begin + bit];
      }
    }
    output.push_back(sum);
  }
}

inline bool two_way_mitm_exists(std::span<const std::int64_t> values,
                                std::int64_t target) {
  const std::size_t left_count = values.size() / 2U;
  const std::size_t right_count = values.size() - left_count;
  std::vector<std::int64_t> left;
  std::vector<std::int64_t> right;
  enumerate_half_sums(values, 0U, left_count, left);
  enumerate_half_sums(values, left_count, right_count, right);
  std::sort(right.begin(), right.end());
  for (const std::int64_t lhs : left) {
    const std::int64_t needed = target - lhs;
    if (std::binary_search(right.begin(), right.end(), needed)) {
      return true;
    }
  }
  return false;
}

inline void require_valid_result(std::span<const std::int64_t> values,
                                 std::int64_t target,
                                 const std::optional<SubsetSumWitness>& result) {
  REQUIRE(result.has_value());
  REQUIRE(valid_subset_sum_witness(values, target, *result));
}

}  // namespace schroeppel_shamir_test_detail

TEST_CASE(schroeppel_shamir_deterministic_edge_cases) {
  using namespace schroeppel_shamir_test_detail;

  require_valid_result({}, 0, schroeppel_shamir_subset_sum({}, 0));
  REQUIRE(!schroeppel_shamir_subset_sum({}, 1).has_value());

  const std::vector<std::int64_t> values{7, -3, 5, -8, 7, 0};
  require_valid_result(values, 9, schroeppel_shamir_subset_sum(values, 9));
  require_valid_result(values, 0, schroeppel_shamir_subset_sum(values, 0));
  REQUIRE(!schroeppel_shamir_subset_sum(values, 100).has_value());

  const auto first = schroeppel_shamir_subset_sum(values, 4);
  const auto second = schroeppel_shamir_subset_sum(values, 4);
  REQUIRE_EQ(first, second);
  require_valid_result(values, 4, first);

  const std::vector<std::int64_t> duplicate_values{4, 4, 4, 4, -4, -4};
  require_valid_result(duplicate_values, 8,
                       schroeppel_shamir_subset_sum(duplicate_values, 8));
}

TEST_CASE(schroeppel_shamir_representability_and_length_contract) {
  using namespace schroeppel_shamir_test_detail;

  const std::vector<std::int64_t> too_many(41U, 0);
  REQUIRE_THROWS_AS(schroeppel_shamir_subset_sum(too_many, 0),
                    std::length_error);

  const std::vector<std::int64_t> positive_overflow{
      std::numeric_limits<std::int64_t>::max(), 1};
  REQUIRE_THROWS_AS(schroeppel_shamir_subset_sum(positive_overflow, 0),
                    std::overflow_error);

  const std::vector<std::int64_t> negative_overflow{
      std::numeric_limits<std::int64_t>::min(), -1};
  REQUIRE_THROWS_AS(schroeppel_shamir_subset_sum(negative_overflow, 0),
                    std::overflow_error);

  const std::vector<std::int64_t> exact_boundary{
      std::numeric_limits<std::int64_t>::min(),
      std::numeric_limits<std::int64_t>::max()};
  require_valid_result(
      exact_boundary, -1, schroeppel_shamir_subset_sum(exact_boundary, -1));
  require_valid_result(exact_boundary, std::numeric_limits<std::int64_t>::min(),
                       schroeppel_shamir_subset_sum(
                           exact_boundary,
                           std::numeric_limits<std::int64_t>::min()));
}

TEST_CASE(schroeppel_shamir_randomized_exhaustive_differential) {
  using namespace schroeppel_shamir_test_detail;

  std::mt19937_64 random(0x5C4A03E11ULL);
  std::uniform_int_distribution<int> length_dist(0, 16);
  std::uniform_int_distribution<int> value_dist(-20, 20);
  std::uniform_int_distribution<int> target_dist(-90, 90);

  for (std::size_t trial = 0; trial < 260U; ++trial) {
    const std::size_t n = static_cast<std::size_t>(length_dist(random));
    std::vector<std::int64_t> values(n);
    for (auto& value : values) {
      value = static_cast<std::int64_t>(value_dist(random));
    }
    const std::int64_t target = static_cast<std::int64_t>(target_dist(random));
    const bool expected = exhaustive_exists(values, target);
    const auto actual = schroeppel_shamir_subset_sum(values, target);
    REQUIRE_EQ(actual.has_value(), expected);
    if (actual.has_value()) {
      REQUIRE(valid_subset_sum_witness(values, target, *actual));
    }
  }
}

TEST_CASE(schroeppel_shamir_medium_two_way_mitm_cross_check) {
  using namespace schroeppel_shamir_test_detail;

  std::mt19937_64 random(0x2A7D51D3ULL);
  std::uniform_int_distribution<int> length_dist(20, 28);
  std::uniform_int_distribution<int> value_dist(-50, 50);
  std::uniform_int_distribution<int> target_dist(-300, 300);

  for (std::size_t trial = 0; trial < 160U; ++trial) {
    const std::size_t n = static_cast<std::size_t>(length_dist(random));
    std::vector<std::int64_t> values(n);
    for (auto& value : values) {
      value = static_cast<std::int64_t>(value_dist(random));
    }
    const std::int64_t target = static_cast<std::int64_t>(target_dist(random));
    const bool expected = two_way_mitm_exists(values, target);
    const auto actual = schroeppel_shamir_subset_sum(values, target);
    REQUIRE_EQ(actual.has_value(), expected);
    if (actual.has_value()) {
      REQUIRE(valid_subset_sum_witness(values, target, *actual));
    }
  }

  std::vector<std::int64_t> forty(40U, 1);
  require_valid_result(forty, 20, schroeppel_shamir_subset_sum(forty, 20));
}
