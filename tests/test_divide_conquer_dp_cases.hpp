#pragma once

#include "algorithms/dynamic_programming/divide_conquer_partition.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using algorithms::dynamic_programming::SquaredSumPartitionResult;
using algorithms::dynamic_programming::minimum_squared_sum_partition;

std::uint64_t partition_cost(const std::vector<std::uint64_t>& weights,
                             const std::vector<std::size_t>& boundaries) {
  std::uint64_t total = 0U;
  for (std::size_t group = 0U; group + 1U < boundaries.size(); ++group) {
    std::uint64_t sum = 0U;
    for (std::size_t index = boundaries[group];
         index < boundaries[group + 1U]; ++index) {
      sum += weights[index];
    }
    total += sum * sum;
  }
  return total;
}

SquaredSumPartitionResult brute_partition_oracle(
    const std::vector<std::uint64_t>& weights, const std::size_t groups) {
  const std::size_t n = weights.size();
  SquaredSumPartitionResult best{std::numeric_limits<std::uint64_t>::max(), {}};
  std::vector<std::size_t> boundaries(groups + 1U, 0U);
  boundaries[groups] = n;

  const auto visit = [&](auto&& self, const std::size_t group,
                         const std::size_t previous) -> void {
    if (group == groups) {
      const std::uint64_t cost = partition_cost(weights, boundaries);
      if (cost < best.minimum_cost ||
          (cost == best.minimum_cost &&
           (best.boundaries.empty() || boundaries < best.boundaries))) {
        best = SquaredSumPartitionResult{cost, boundaries};
      }
      return;
    }
    const std::size_t remaining_groups = groups - group;
    const std::size_t last_split = n - remaining_groups;
    for (std::size_t split = previous + 1U; split <= last_split; ++split) {
      boundaries[group] = split;
      self(self, group + 1U, split);
    }
  };

  if (groups == 1U) {
    best = SquaredSumPartitionResult{partition_cost(weights, boundaries), boundaries};
  } else {
    visit(visit, 1U, 0U);
  }
  return best;
}

SquaredSumPartitionResult quadratic_partition_oracle(
    const std::vector<std::uint64_t>& weights, const std::size_t groups) {
  const std::size_t n = weights.size();
  std::vector<std::uint64_t> prefix(n + 1U, 0U);
  for (std::size_t index = 0U; index < n; ++index) {
    prefix[index + 1U] = prefix[index] + weights[index];
  }
  const auto cost = [&prefix](const std::size_t begin, const std::size_t end) {
    const std::uint64_t sum = prefix[end] - prefix[begin];
    return sum * sum;
  };
  constexpr std::uint64_t kInfinity =
      std::numeric_limits<std::uint64_t>::max();
  std::vector<std::vector<std::uint64_t>> dp(
      groups + 1U, std::vector<std::uint64_t>(n + 1U, kInfinity));
  std::vector<std::vector<std::size_t>> parent(
      groups + 1U, std::vector<std::size_t>(n + 1U, n + 1U));
  dp[0U][0U] = 0U;

  for (std::size_t group = 1U; group <= groups; ++group) {
    std::size_t last_optimum = group - 1U;
    for (std::size_t end = group; end <= n; ++end) {
      for (std::size_t split = group - 1U; split < end; ++split) {
        if (dp[group - 1U][split] == kInfinity) {
          continue;
        }
        const std::uint64_t candidate =
            dp[group - 1U][split] + cost(split, end);
        if (candidate < dp[group][end]) {
          dp[group][end] = candidate;
          parent[group][end] = split;
        }
      }
      if (parent[group][end] < last_optimum) {
        throw std::runtime_error("quadratic oracle observed non-monotone argmin");
      }
      last_optimum = parent[group][end];
    }
  }

  std::vector<std::size_t> boundaries(groups + 1U, 0U);
  boundaries[groups] = n;
  std::size_t end = n;
  for (std::size_t group = groups; group > 0U; --group) {
    end = parent[group][end];
    boundaries[group - 1U] = end;
  }
  return SquaredSumPartitionResult{dp[groups][n], std::move(boundaries)};
}

TEST_CASE(divide_conquer_partition_validates_domain_and_boundaries) {
  const auto empty = minimum_squared_sum_partition({}, 0U);
  REQUIRE_EQ(empty.minimum_cost, 0U);
  REQUIRE_EQ(empty.boundaries, (std::vector<std::size_t>{0U}));
  REQUIRE_THROWS_AS(minimum_squared_sum_partition({}, 1U), std::invalid_argument);
  REQUIRE_THROWS_AS(minimum_squared_sum_partition({1U}, 0U), std::invalid_argument);
  REQUIRE_THROWS_AS(minimum_squared_sum_partition({1U, 2U}, 3U),
                    std::invalid_argument);

  constexpr std::uint64_t kExactLimit = 0xFFFF'FFFFULL;
  const auto boundary = minimum_squared_sum_partition({kExactLimit}, 1U);
  REQUIRE_EQ(boundary.minimum_cost, 0xFFFF'FFFE'0000'0001ULL);
  REQUIRE_THROWS_AS(minimum_squared_sum_partition({kExactLimit, 1U}, 2U),
                    std::overflow_error);
}

TEST_CASE(divide_conquer_partition_reconstructs_deterministic_optimum) {
  const auto result = minimum_squared_sum_partition({1U, 2U, 3U, 4U}, 2U);
  REQUIRE_EQ(result.minimum_cost, 52U);
  REQUIRE_EQ(result.boundaries, (std::vector<std::size_t>{0U, 3U, 4U}));

  const auto ties = minimum_squared_sum_partition({0U, 0U, 0U, 0U}, 2U);
  REQUIRE_EQ(ties.minimum_cost, 0U);
  REQUIRE_EQ(ties.boundaries, (std::vector<std::size_t>{0U, 1U, 4U}));

  const auto singletons = minimum_squared_sum_partition({2U, 3U, 5U}, 3U);
  REQUIRE_EQ(singletons.minimum_cost, 38U);
  REQUIRE_EQ(singletons.boundaries,
             (std::vector<std::size_t>{0U, 1U, 2U, 3U}));
}

TEST_CASE(divide_conquer_partition_exhaustive_small_matches_all_cut_sets) {
  for (std::size_t n = 1U; n <= 7U; ++n) {
    std::uint64_t encodings = 1U;
    for (std::size_t index = 0U; index < n; ++index) {
      encodings *= 3U;
    }
    for (std::uint64_t code = 0U; code < encodings; ++code) {
      std::uint64_t value = code;
      std::vector<std::uint64_t> weights(n, 0U);
      for (std::size_t index = 0U; index < n; ++index) {
        weights[index] = value % 3U;
        value /= 3U;
      }
      for (std::size_t groups = 1U; groups <= n; ++groups) {
        const auto expected = brute_partition_oracle(weights, groups);
        const auto actual = minimum_squared_sum_partition(weights, groups);
        REQUIRE_EQ(actual.minimum_cost, expected.minimum_cost);
        REQUIRE_EQ(actual.boundaries, expected.boundaries);
      }
    }
  }
}

TEST_CASE(divide_conquer_partition_random_larger_matches_quadratic_dp) {
  std::mt19937_64 rng(0xD1C0A11ULL);
  for (std::size_t trial = 0U; trial < 400U; ++trial) {
    const std::size_t n = 1U + static_cast<std::size_t>(rng() % 48U);
    const std::size_t groups = 1U + static_cast<std::size_t>(rng() % n);
    std::vector<std::uint64_t> weights(n, 0U);
    for (auto& weight : weights) {
      weight = rng() % 1000U;
    }
    const auto expected = quadratic_partition_oracle(weights, groups);
    const auto actual = minimum_squared_sum_partition(weights, groups);
    REQUIRE_EQ(actual.minimum_cost, expected.minimum_cost);
    REQUIRE_EQ(actual.boundaries, expected.boundaries);
    REQUIRE_EQ(partition_cost(weights, actual.boundaries), actual.minimum_cost);
  }
}

}  // namespace
