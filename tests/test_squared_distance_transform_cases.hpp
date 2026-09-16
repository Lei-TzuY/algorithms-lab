#pragma once

#include "algorithms/optimization/squared_distance_transform.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

namespace squared_distance_transform_test_detail {
using algorithms::optimization::SquaredDistanceTransformResult;
using algorithms::optimization::squared_distance_transform_1d;

inline SquaredDistanceTransformResult naive(
    const std::vector<std::int64_t>& costs) {
  SquaredDistanceTransformResult result;
  result.values.resize(costs.size());
  result.argmin.resize(costs.size());
  for (std::size_t x = 0U; x < costs.size(); ++x) {
    bool have_best = false;
    std::int64_t best = 0;
    std::size_t best_source = 0U;
    for (std::size_t source = 0U; source < costs.size(); ++source) {
      const auto distance = static_cast<std::int64_t>(x) -
                            static_cast<std::int64_t>(source);
      const std::int64_t value = costs[source] + distance * distance;
      if (!have_best || value < best) {
        have_best = true;
        best = value;
        best_source = source;
      }
    }
    if (have_best) {
      result.values[x] = best;
      result.argmin[x] = best_source;
    }
  }
  return result;
}
}  // namespace squared_distance_transform_test_detail

TEST_CASE(squared_distance_transform_deterministic_and_ties) {
  using namespace squared_distance_transform_test_detail;
  REQUIRE_EQ(squared_distance_transform_1d(std::vector<std::int64_t>{}),
             SquaredDistanceTransformResult{});

  const std::vector<std::vector<std::int64_t>> cases{
      {7},
      {0, 0, 0, 0, 0},
      {0, 1},
      {100, 100, -20, 100, 100},
      {5, -3, 9, -3, 5},
      {-1'000'000'000'000'000LL, 1'000'000'000'000'000LL,
       1'000'000'000'000'000LL}};
  for (const auto& costs : cases) {
    REQUIRE_EQ(squared_distance_transform_1d(costs), naive(costs));
  }

  const auto tie = squared_distance_transform_1d(
      std::vector<std::int64_t>{0, 1});
  REQUIRE_EQ(tie.argmin[1], 0U);
}

TEST_CASE(squared_distance_transform_rejects_outside_exact_domain) {
  using namespace squared_distance_transform_test_detail;
  REQUIRE_THROWS_AS(
      squared_distance_transform_1d(
          std::vector<std::int64_t>{1'000'000'000'000'001LL}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      squared_distance_transform_1d(std::vector<std::int64_t>(1'000'001U, 0)),
      std::length_error);
}

TEST_CASE(squared_distance_transform_randomized_against_quadratic_oracle) {
  using namespace squared_distance_transform_test_detail;
  std::mt19937_64 rng(0xD157A6CEULL);
  std::uniform_int_distribution<int> length_dist(0, 160);
  std::uniform_int_distribution<std::int64_t> value_dist(-1'000'000, 1'000'000);

  for (int trial = 0; trial < 5'000; ++trial) {
    const auto n = static_cast<std::size_t>(length_dist(rng));
    std::vector<std::int64_t> costs(n);
    for (auto& value : costs) {
      value = value_dist(rng);
    }
    REQUIRE_EQ(squared_distance_transform_1d(costs), naive(costs));
  }
}


TEST_CASE(squared_distance_transform_exhaustive_small_integer_arrays) {
  using namespace squared_distance_transform_test_detail;
  for (std::size_t n = 0U; n <= 6U; ++n) {
    std::vector<std::int64_t> costs(n, -2);
    bool done = false;
    while (!done) {
      REQUIRE_EQ(squared_distance_transform_1d(costs), naive(costs));
      std::size_t position = 0U;
      while (position < costs.size()) {
        if (costs[position] < 2) {
          ++costs[position];
          break;
        }
        costs[position] = -2;
        ++position;
      }
      if (position == costs.size()) {
        done = true;
      }
    }
  }
}

TEST_CASE(squared_distance_transform_large_envelope_shape) {
  using namespace squared_distance_transform_test_detail;
  std::vector<std::int64_t> costs(10'000U);
  for (std::size_t index = 0U; index < costs.size(); ++index) {
    costs[index] = static_cast<std::int64_t>(index) * 13;
  }
  const auto result = squared_distance_transform_1d(costs);
  REQUIRE_EQ(result.values.size(), costs.size());
  REQUIRE_EQ(result.argmin.size(), costs.size());

  for (std::size_t sample = 0U; sample < 100U; ++sample) {
    const std::size_t x = (sample * 97U) % costs.size();
    std::int64_t best = std::numeric_limits<std::int64_t>::max();
    std::size_t best_source = 0U;
    for (std::size_t source = 0U; source < costs.size(); ++source) {
      const auto distance = static_cast<std::int64_t>(x) -
                            static_cast<std::int64_t>(source);
      const std::int64_t value = costs[source] + distance * distance;
      if (value < best) {
        best = value;
        best_source = source;
      }
    }
    REQUIRE_EQ(result.values[x], best);
    REQUIRE_EQ(result.argmin[x], best_source);
  }
}
