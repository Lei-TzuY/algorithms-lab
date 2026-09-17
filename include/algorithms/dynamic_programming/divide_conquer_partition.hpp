#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::dynamic_programming {

struct SquaredSumPartitionResult {
  std::uint64_t minimum_cost{};
  // Half-open group boundaries. For g groups this has g+1 entries,
  // starts at 0, ends at weights.size(), and is strictly increasing.
  std::vector<std::size_t> boundaries;
};

// Partition non-negative weights into exactly `groups` non-empty contiguous
// groups, minimizing the sum of squared group sums.
//
// Exact-arithmetic domain: sum(weights) <= 2^32-1. This implies every segment
// square and every complete partition cost fits in uint64_t.
//
// For prefix sums S, C(j,i)=(S[i]-S[j])^2 is Monge for non-negative weights.
// Therefore the leftmost optimal split index is monotone across increasing i,
// enabling divide-and-conquer DP optimization.
[[nodiscard]] inline SquaredSumPartitionResult minimum_squared_sum_partition(
    const std::vector<std::uint64_t>& weights, const std::size_t groups) {
  const std::size_t n = weights.size();
  if (n == 0U) {
    if (groups != 0U) {
      throw std::invalid_argument("empty sequence requires zero groups");
    }
    return SquaredSumPartitionResult{0U, {0U}};
  }
  if (groups == 0U || groups > n) {
    throw std::invalid_argument("groups must be in [1, weights.size()]");
  }

  constexpr std::uint64_t kMaxExactTotal = 0xFFFF'FFFFULL;
  std::vector<std::uint64_t> prefix(n + 1U, 0U);
  for (std::size_t index = 0U; index < n; ++index) {
    if (weights[index] > kMaxExactTotal - prefix[index]) {
      throw std::overflow_error(
          "total weight exceeds exact uint64 squared-cost domain");
    }
    prefix[index + 1U] = prefix[index] + weights[index];
  }

  constexpr std::uint64_t kInfinity =
      std::numeric_limits<std::uint64_t>::max();
  std::vector<std::uint64_t> previous(n + 1U, kInfinity);
  std::vector<std::uint64_t> current(n + 1U, kInfinity);
  previous[0U] = 0U;
  std::vector<std::vector<std::size_t>> parent(
      groups + 1U, std::vector<std::size_t>(n + 1U, n + 1U));

  const auto segment_cost = [&prefix](const std::size_t begin,
                                       const std::size_t end) {
    const std::uint64_t sum = prefix[end] - prefix[begin];
    return sum * sum;
  };

  for (std::size_t group = 1U; group <= groups; ++group) {
    std::fill(current.begin(), current.end(), kInfinity);

    const auto solve = [&](auto&& self, const std::size_t left,
                           const std::size_t right,
                           const std::size_t optimal_left,
                           const std::size_t optimal_right) -> void {
      if (left > right) {
        return;
      }
      const std::size_t middle = left + (right - left) / 2U;
      const std::size_t search_end = std::min(middle - 1U, optimal_right);
      std::uint64_t best_cost = kInfinity;
      std::size_t best_split = n + 1U;

      for (std::size_t split = optimal_left; split <= search_end; ++split) {
        if (previous[split] == kInfinity) {
          continue;
        }
        const std::uint64_t candidate =
            previous[split] + segment_cost(split, middle);
        if (candidate < best_cost ||
            (candidate == best_cost && split < best_split)) {
          best_cost = candidate;
          best_split = split;
        }
      }

      if (best_split == n + 1U) {
        throw std::logic_error("divide-and-conquer DP found no valid split");
      }
      current[middle] = best_cost;
      parent[group][middle] = best_split;

      if (middle > left) {
        self(self, left, middle - 1U, optimal_left, best_split);
      }
      if (middle < right) {
        self(self, middle + 1U, right, best_split, optimal_right);
      }
    };

    solve(solve, group, n, group - 1U, n - 1U);
    previous.swap(current);
  }

  std::vector<std::size_t> boundaries(groups + 1U, 0U);
  boundaries[groups] = n;
  std::size_t end = n;
  for (std::size_t group = groups; group > 0U; --group) {
    const std::size_t split = parent[group][end];
    if (split > n) {
      throw std::logic_error("partition reconstruction lost a split");
    }
    boundaries[group - 1U] = split;
    end = split;
  }
  return SquaredSumPartitionResult{previous[n], std::move(boundaries)};
}

}  // namespace algorithms::dynamic_programming
