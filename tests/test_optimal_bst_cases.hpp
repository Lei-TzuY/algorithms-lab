#pragma once

#include "algorithms/dynamic_programming/optimal_bst.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

namespace optimal_bst_test {

struct CubicResult {
  std::uint64_t cost = 0;
  std::vector<std::size_t> roots;
  std::size_t stride = 0;
};

inline CubicResult cubic_oracle(std::span<const std::uint64_t> successful,
                                std::span<const std::uint64_t> unsuccessful) {
  const std::size_t n = successful.size();
  const std::size_t stride = n + 1U;
  std::vector<std::uint64_t> cost(stride * stride, 0U);
  std::vector<std::size_t> roots(
      stride * stride,
      algorithms::dynamic_programming::OptimalBstResult::no_root);
  const auto idx = [stride](std::size_t i, std::size_t j) {
    return i * stride + j;
  };
  for (std::size_t i = 0; i <= n; ++i) {
    cost[idx(i, i)] = unsuccessful[i];
  }
  for (std::size_t len = 1; len <= n; ++len) {
    for (std::size_t i = 0; i + len <= n; ++i) {
      const std::size_t j = i + len;
      std::uint64_t weight = 0;
      for (std::size_t k = i; k < j; ++k) {
        weight += successful[k];
      }
      for (std::size_t k = i; k <= j; ++k) {
        weight += unsuccessful[k];
      }
      bool have = false;
      std::uint64_t best = 0;
      std::size_t best_root = 0;
      for (std::size_t root = i; root < j; ++root) {
        const std::uint64_t candidate =
            cost[idx(i, root)] + cost[idx(root + 1U, j)] + weight;
        if (!have || candidate < best) {
          have = true;
          best = candidate;
          best_root = root;
        }
      }
      cost[idx(i, j)] = best;
      roots[idx(i, j)] = best_root;
    }
  }
  return CubicResult{cost[idx(0, n)], std::move(roots), stride};
}

inline std::uint64_t replay_cost(
    std::span<const std::uint64_t> successful,
    std::span<const std::uint64_t> unsuccessful,
    const algorithms::dynamic_programming::OptimalBstResult& result) {
  std::uint64_t total = 0;
  for (std::size_t i = 0; i < successful.size(); ++i) {
    total += successful[i] *
             static_cast<std::uint64_t>(result.key_depths[i] + 1U);
  }
  for (std::size_t i = 0; i < unsuccessful.size(); ++i) {
    total += unsuccessful[i] *
             static_cast<std::uint64_t>(result.gap_depths[i] + 1U);
  }
  return total;
}

inline void require_knuth_monotonicity(
    const algorithms::dynamic_programming::OptimalBstResult& result,
    std::size_t n) {
  const std::size_t stride = result.root_table_stride;
  REQUIRE_EQ(stride, n + 1U);
  REQUIRE_EQ(result.interval_roots.size(), stride * stride);
  const auto root = [&](std::size_t i, std::size_t j) {
    return result.interval_roots[i * stride + j];
  };
  for (std::size_t len = 2; len <= n; ++len) {
    for (std::size_t i = 0; i + len <= n; ++i) {
      const std::size_t j = i + len;
      REQUIRE(root(i, j - 1U) <= root(i, j));
      REQUIRE(root(i, j) <= root(i + 1U, j));
    }
  }
}

inline void require_parent_witness(
    const algorithms::dynamic_programming::OptimalBstResult& result) {
  const std::size_t n = result.parent.size();
  REQUIRE_EQ(result.key_depths.size(), n);
  REQUIRE_EQ(result.gap_depths.size(), n + 1U);
  if (n == 0U) {
    REQUIRE(!result.root.has_value());
    REQUIRE_EQ(result.gap_depths[0], 0U);
    return;
  }
  REQUIRE(result.root.has_value());
  REQUIRE_EQ(result.parent[*result.root], std::nullopt);
  REQUIRE_EQ(result.key_depths[*result.root], 0U);
  std::size_t roots_seen = 0;
  for (std::size_t key = 0; key < n; ++key) {
    if (!result.parent[key].has_value()) {
      ++roots_seen;
      REQUIRE_EQ(key, *result.root);
    } else {
      REQUIRE(*result.parent[key] < n);
      REQUIRE_EQ(result.key_depths[key],
                 result.key_depths[*result.parent[key]] + 1U);
    }
  }
  REQUIRE_EQ(roots_seen, 1U);
}

}  // namespace optimal_bst_test

TEST_CASE(optimal_bst_contracts_and_known_vectors) {
  using algorithms::dynamic_programming::optimal_binary_search_tree_knuth;

  const std::vector<std::uint64_t> empty_success;
  const std::vector<std::uint64_t> bad_failure;
  REQUIRE_THROWS_AS(optimal_binary_search_tree_knuth(empty_success, bad_failure),
                    std::invalid_argument);

  const std::vector<std::uint64_t> empty_failure{7};
  const auto empty =
      optimal_binary_search_tree_knuth(empty_success, empty_failure);
  REQUIRE_EQ(empty.weighted_search_cost.high, 0U);
  REQUIRE_EQ(empty.weighted_search_cost.low, 7U);
  REQUIRE_EQ(empty.candidate_evaluations, 0U);
  optimal_bst_test::require_parent_witness(empty);

  const std::vector<std::uint64_t> singleton_success{3};
  const std::vector<std::uint64_t> singleton_failure{1, 2};
  const auto singleton = optimal_binary_search_tree_knuth(
      singleton_success, singleton_failure);
  REQUIRE_EQ(singleton.weighted_search_cost.to_uint64(), 9U);
  REQUIRE_EQ(*singleton.root, 0U);
  REQUIRE_EQ(singleton.gap_depths[0], 1U);
  REQUIRE_EQ(singleton.gap_depths[1], 1U);

  const std::vector<std::uint64_t> classic_success{15, 10, 5, 10, 20};
  const std::vector<std::uint64_t> classic_failure{5, 10, 5, 5, 5, 10};
  const auto classic =
      optimal_binary_search_tree_knuth(classic_success, classic_failure);
  REQUIRE_EQ(classic.weighted_search_cost.to_uint64(), 275U);
  REQUIRE_EQ(*classic.root, 1U);
  REQUIRE_EQ(optimal_bst_test::replay_cost(classic_success, classic_failure,
                                           classic),
             275U);
  optimal_bst_test::require_knuth_monotonicity(classic,
                                                classic_success.size());
  optimal_bst_test::require_parent_witness(classic);

  const std::vector<std::uint64_t> boundary_success{
      std::numeric_limits<std::uint64_t>::max()};
  const std::vector<std::uint64_t> zero_failures{0, 0};
  const auto boundary =
      optimal_binary_search_tree_knuth(boundary_success, zero_failures);
  REQUIRE_EQ(boundary.weighted_search_cost.high, 0U);
  REQUIRE_EQ(boundary.weighted_search_cost.low,
             std::numeric_limits<std::uint64_t>::max());

  const std::vector<std::uint64_t> huge_failures{
      std::numeric_limits<std::uint64_t>::max(),
      std::numeric_limits<std::uint64_t>::max()};
  const auto wide =
      optimal_binary_search_tree_knuth(boundary_success, huge_failures);
  REQUIRE(wide.weighted_search_cost.high > 0U);
  REQUIRE_THROWS_AS(wide.weighted_search_cost.to_uint64(), std::overflow_error);
}

TEST_CASE(optimal_bst_zero_weight_ties_are_deterministic) {
  using algorithms::dynamic_programming::optimal_binary_search_tree_knuth;
  const std::vector<std::uint64_t> successful(12, 0U);
  const std::vector<std::uint64_t> unsuccessful(13, 0U);
  const auto first = optimal_binary_search_tree_knuth(successful, unsuccessful);
  const auto second = optimal_binary_search_tree_knuth(successful, unsuccessful);
  REQUIRE_EQ(first.interval_roots, second.interval_roots);
  REQUIRE_EQ(first.parent, second.parent);
  REQUIRE_EQ(first.key_depths, second.key_depths);
  REQUIRE_EQ(first.gap_depths, second.gap_depths);
  REQUIRE_EQ(*first.root, 0U);
  optimal_bst_test::require_knuth_monotonicity(first, successful.size());
  optimal_bst_test::require_parent_witness(first);
}

TEST_CASE(optimal_bst_randomized_differential_against_cubic_dp) {
  using algorithms::dynamic_programming::optimal_binary_search_tree_knuth;
  std::mt19937_64 rng(0x0B57C0DEULL);
  std::uniform_int_distribution<std::size_t> size_dist(0, 20);
  std::uniform_int_distribution<std::uint64_t> weight_dist(0, 30);

  for (std::size_t trial = 0; trial < 700; ++trial) {
    const std::size_t n = size_dist(rng);
    std::vector<std::uint64_t> successful(n);
    std::vector<std::uint64_t> unsuccessful(n + 1U);
    for (auto& value : successful) value = weight_dist(rng);
    for (auto& value : unsuccessful) value = weight_dist(rng);

    const auto actual =
        optimal_binary_search_tree_knuth(successful, unsuccessful);
    const auto oracle = optimal_bst_test::cubic_oracle(successful, unsuccessful);
    REQUIRE(actual.weighted_search_cost.fits_uint64());
    REQUIRE_EQ(actual.weighted_search_cost.to_uint64(), oracle.cost);
    REQUIRE_EQ(optimal_bst_test::replay_cost(successful, unsuccessful, actual),
               oracle.cost);
    optimal_bst_test::require_knuth_monotonicity(actual, n);
    optimal_bst_test::require_parent_witness(actual);
    if (n > 0U) {
      REQUIRE(actual.candidate_evaluations <= 2U * n * n);
    } else {
      REQUIRE_EQ(actual.candidate_evaluations, 0U);
    }
  }
}

TEST_CASE(optimal_bst_large_monotonicity_diagnostic_stays_quadratic) {
  using algorithms::dynamic_programming::optimal_binary_search_tree_knuth;
  constexpr std::size_t n = 256;
  std::mt19937_64 rng(0x4B4E555448ULL);
  std::uniform_int_distribution<std::uint64_t> weight_dist(0, 1000);
  std::vector<std::uint64_t> successful(n);
  std::vector<std::uint64_t> unsuccessful(n + 1U);
  for (auto& value : successful) value = weight_dist(rng);
  for (auto& value : unsuccessful) value = weight_dist(rng);
  const auto result =
      optimal_binary_search_tree_knuth(successful, unsuccessful);
  optimal_bst_test::require_knuth_monotonicity(result, n);
  REQUIRE(result.candidate_evaluations <= 2U * n * n);
}
