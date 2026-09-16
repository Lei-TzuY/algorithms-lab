#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>

namespace algorithms::optimization {

struct SquaredDistanceTransformResult {
  std::vector<std::int64_t> values;
  std::vector<std::size_t> argmin;

  friend bool operator==(const SquaredDistanceTransformResult&,
                         const SquaredDistanceTransformResult&) = default;
};

// Exact bounded 1D transform
//   g[x] = min_j(cost[j] + (x-j)^2)
// with deterministic leftmost argmin on ties.
//
// Domain bounds keep all squared terms, envelope-intersection numerators, and
// public output exactly representable in signed int64_t without wider integers:
//   costs.size() <= 1'000'000
//   every cost lies in [-1'000'000'000'000'000, 1'000'000'000'000'000]
//
// The lower envelope of equal-curvature parabolas is maintained in monotone
// source-index order. Every source is pushed once and popped at most once.
// Complexity: O(n) time and O(n) result/auxiliary storage.
[[nodiscard]] inline SquaredDistanceTransformResult squared_distance_transform_1d(
    std::span<const std::int64_t> costs) {
  constexpr std::size_t kMaxSize = 1'000'000U;
  constexpr std::int64_t kMaxAbsCost = 1'000'000'000'000'000LL;

  if (costs.size() > kMaxSize) {
    throw std::length_error("squared distance transform size limit exceeded");
  }
  for (const std::int64_t value : costs) {
    if (value < -kMaxAbsCost || value > kMaxAbsCost) {
      throw std::invalid_argument(
          "squared distance transform cost out of exact domain");
    }
  }

  SquaredDistanceTransformResult result;
  result.values.resize(costs.size());
  result.argmin.resize(costs.size());
  if (costs.empty()) {
    return result;
  }

  const auto square_index = [](std::size_t index) -> std::int64_t {
    const auto signed_index = static_cast<std::int64_t>(index);
    return signed_index * signed_index;
  };

  // Mathematical floor(numerator / denominator) for denominator > 0.
  const auto floor_div = [](std::int64_t numerator,
                            std::int64_t denominator) -> std::int64_t {
    std::int64_t quotient = numerator / denominator;
    const std::int64_t remainder = numerator % denominator;
    if (remainder != 0 && numerator < 0) {
      --quotient;
    }
    return quotient;
  };

  // First integer x where new_index is strictly better than old_index. Strict
  // improvement is deliberate: equality keeps the smaller (older) index.
  const auto first_strictly_better = [&](std::size_t old_index,
                                         std::size_t new_index) {
    const std::int64_t numerator =
        (costs[new_index] + square_index(new_index)) -
        (costs[old_index] + square_index(old_index));
    const auto delta = static_cast<std::int64_t>(new_index - old_index);
    const std::int64_t denominator = 2 * delta;
    return floor_div(numerator, denominator) + 1;
  };

  std::vector<std::size_t> sites;
  std::vector<std::int64_t> starts;
  sites.reserve(costs.size());
  starts.reserve(costs.size());
  sites.push_back(0U);
  starts.push_back(0);

  for (std::size_t candidate = 1U; candidate < costs.size(); ++candidate) {
    std::int64_t start = 0;
    while (true) {
      start = first_strictly_better(sites.back(), candidate);
      if (start > starts.back()) {
        break;
      }
      sites.pop_back();
      starts.pop_back();
      if (sites.empty()) {
        start = 0;
        break;
      }
    }
    sites.push_back(candidate);
    starts.push_back(start);
  }

  std::size_t envelope_index = 0U;
  for (std::size_t x = 0U; x < costs.size(); ++x) {
    const auto signed_x = static_cast<std::int64_t>(x);
    while (envelope_index + 1U < sites.size() &&
           starts[envelope_index + 1U] <= signed_x) {
      ++envelope_index;
    }
    const std::size_t source = sites[envelope_index];
    const auto signed_source = static_cast<std::int64_t>(source);
    const std::int64_t distance = signed_x - signed_source;
    result.values[x] = costs[source] + distance * distance;
    result.argmin[x] = source;
  }

  return result;
}

}  // namespace algorithms::optimization
