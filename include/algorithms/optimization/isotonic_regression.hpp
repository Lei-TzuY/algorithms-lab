#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::optimization {

struct IsotonicRegressionBlock {
  std::size_t begin{};
  std::size_t end{};
  std::int64_t sum{};
  std::size_t count{};

  friend bool operator==(const IsotonicRegressionBlock&,
                         const IsotonicRegressionBlock&) = default;
};

struct IsotonicRegressionResult {
  std::size_t observation_count{};
  std::vector<IsotonicRegressionBlock> blocks;

  friend bool operator==(const IsotonicRegressionResult&,
                         const IsotonicRegressionResult&) = default;
};

// Bounded-exact unweighted L2 isotonic regression over integer observations.
// Each block's exact fitted value is the rational number sum / count.
// Adjacent equal means are coalesced, so returned block means are strictly
// increasing. These domain bounds make all sums and mean cross-products used by
// the PAVA implementation exactly representable in int64_t:
//   observations.size() <= 50'000
//   every observation is in [-1'000'000'000, 1'000'000'000]
// Complexity: O(n) time and O(n) result/auxiliary storage.
[[nodiscard]] inline IsotonicRegressionResult least_squares_isotonic_regression(
    std::span<const std::int64_t> observations) {
  constexpr std::size_t kMaxObservations = 50'000U;
  constexpr std::int64_t kMaxAbsObservation = 1'000'000'000LL;

  if (observations.size() > kMaxObservations) {
    throw std::length_error("isotonic regression observation limit exceeded");
  }

  IsotonicRegressionResult result;
  result.observation_count = observations.size();
  result.blocks.reserve(observations.size());

  const auto mean_greater_or_equal = [](const IsotonicRegressionBlock& left,
                                        const IsotonicRegressionBlock& right) {
    const auto right_count = static_cast<std::int64_t>(right.count);
    const auto left_count = static_cast<std::int64_t>(left.count);
    return left.sum * right_count >= right.sum * left_count;
  };

  for (std::size_t index = 0; index < observations.size(); ++index) {
    const std::int64_t value = observations[index];
    if (value < -kMaxAbsObservation || value > kMaxAbsObservation) {
      throw std::invalid_argument(
          "isotonic regression observation out of exact domain");
    }

    result.blocks.push_back(
        IsotonicRegressionBlock{index, index + 1U, value, 1U});

    while (result.blocks.size() >= 2U) {
      const std::size_t right_index = result.blocks.size() - 1U;
      const std::size_t left_index = right_index - 1U;
      if (!mean_greater_or_equal(result.blocks[left_index],
                                 result.blocks[right_index])) {
        break;
      }

      IsotonicRegressionBlock merged;
      merged.begin = result.blocks[left_index].begin;
      merged.end = result.blocks[right_index].end;
      merged.sum = result.blocks[left_index].sum + result.blocks[right_index].sum;
      merged.count =
          result.blocks[left_index].count + result.blocks[right_index].count;
      result.blocks.pop_back();
      result.blocks.back() = merged;
    }
  }

  return result;
}

}  // namespace algorithms::optimization
