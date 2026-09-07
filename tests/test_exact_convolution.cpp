#include "algorithms/polynomials/exact_convolution.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::polynomials::convolution_exact_int64;
using algorithms::polynomials::kExactConvolutionCenteredLimit;

namespace {

std::vector<std::int64_t> naive_exact_convolution(
    const std::vector<std::int64_t>& left,
    const std::vector<std::int64_t>& right) {
  if (left.empty() || right.empty()) {
    return {};
  }
  std::vector<std::int64_t> result(left.size() + right.size() - 1U, 0);
  for (std::size_t left_index = 0U; left_index < left.size(); ++left_index) {
    for (std::size_t right_index = 0U; right_index < right.size();
         ++right_index) {
      result[left_index + right_index] +=
          left[left_index] * right[right_index];
    }
  }
  return result;
}

}  // namespace

TEST_CASE(exact_convolution_deterministic_and_boundaries) {
  REQUIRE(convolution_exact_int64({}, {1}).empty());
  REQUIRE(convolution_exact_int64({1}, {}).empty());
  REQUIRE_EQ(convolution_exact_int64({1, -2, 3}, {-4, 5}),
             (std::vector<std::int64_t>{-4, 13, -22, 15}));

  const auto limit =
      static_cast<std::int64_t>(kExactConvolutionCenteredLimit);
  REQUIRE_EQ(convolution_exact_int64({limit}, {1}),
             (std::vector<std::int64_t>{limit}));
  REQUIRE_EQ(convolution_exact_int64({-limit}, {1}),
             (std::vector<std::int64_t>{-limit}));
  REQUIRE_THROWS_AS(convolution_exact_int64({limit + 1}, {1}),
                    std::overflow_error);

  REQUIRE_EQ(convolution_exact_int64(
                 {std::numeric_limits<std::int64_t>::min()}, {0}),
             (std::vector<std::int64_t>{0}));
}

TEST_CASE(exact_convolution_conservative_bound_rejects_unproven_inputs) {
  const auto limit =
      static_cast<std::int64_t>(kExactConvolutionCenteredLimit);
  const std::int64_t value = limit / 2 + 1;
  REQUIRE_THROWS_AS(convolution_exact_int64({value, -value}, {1, 1}),
                    std::overflow_error);
}

TEST_CASE(exact_convolution_randomized_against_naive_oracle) {
  std::mt19937_64 rng(0xC47E55ULL);
  std::uniform_int_distribution<std::size_t> length_dist(0U, 32U);
  std::uniform_int_distribution<std::int64_t> coefficient_dist(-1000000,
                                                                1000000);

  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    std::vector<std::int64_t> left(length_dist(rng), 0);
    std::vector<std::int64_t> right(length_dist(rng), 0);
    for (auto& value : left) {
      value = coefficient_dist(rng);
    }
    for (auto& value : right) {
      value = coefficient_dist(rng);
    }
    REQUIRE_EQ(convolution_exact_int64(left, right),
               naive_exact_convolution(left, right));
  }
}
