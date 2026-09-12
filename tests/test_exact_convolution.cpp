#include "algorithms/combinatorial/subset_convolution.hpp"
#include "algorithms/polynomials/exact_convolution.hpp"
#include "test_framework.hpp"

#include <bit>
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

std::vector<std::uint64_t> naive_subset_convolution(
    const std::vector<std::uint64_t>& first,
    const std::vector<std::uint64_t>& second, std::uint64_t modulus) {
  std::vector<std::uint64_t> result(first.size(), 0U);
  for (std::size_t mask = 0U; mask < first.size(); ++mask) {
    std::size_t subset = mask;
    while (true) {
      const std::size_t complement = mask ^ subset;
      const std::uint64_t left = first[subset] % modulus;
      const std::uint64_t right = second[complement] % modulus;
      // Recovery randomized tests keep modulus <= 1,000,003, so this direct
      // product cannot overflow uint64_t and stays independent of multiply_mod.
      const std::uint64_t product = (left * right) % modulus;
      result[mask] = (result[mask] + product) % modulus;
      if (subset == 0U) {
        break;
      }
      subset = (subset - 1U) & mask;
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

TEST_CASE(subset_convolution_validates_shape_and_modulus) {
  const std::vector<std::uint64_t> empty;
  const std::vector<std::uint64_t> one{1U};
  const std::vector<std::uint64_t> two{1U, 2U};
  const std::vector<std::uint64_t> three{1U, 2U, 3U};

  REQUIRE_THROWS_AS(algorithms::combinatorial::subset_convolution_mod(
                        empty, empty, 17U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(algorithms::combinatorial::subset_convolution_mod(
                        one, two, 17U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(algorithms::combinatorial::subset_convolution_mod(
                        three, three, 17U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(algorithms::combinatorial::subset_convolution_mod(
                        one, one, 0U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(algorithms::combinatorial::subset_convolution_mod(
                        one, one, 1U),
                    std::invalid_argument);
}

TEST_CASE(subset_convolution_handles_singleton_identity_and_composite_modulus) {
  {
    const std::vector<std::uint64_t> first{1234U};
    const std::vector<std::uint64_t> second{9876U};
    const auto result = algorithms::combinatorial::subset_convolution_mod(
        first, second, 1000U);
    REQUIRE_EQ(result, std::vector<std::uint64_t>({984U}));
  }

  const std::vector<std::uint64_t> values{1001U, 2002U, 3003U, 4004U,
                                          5005U, 6006U, 7007U, 8008U};
  std::vector<std::uint64_t> identity(values.size(), 0U);
  identity[0] = 1U;
  const auto result = algorithms::combinatorial::subset_convolution_mod(
      identity, values, 1000U);
  std::vector<std::uint64_t> expected(values.size(), 0U);
  for (std::size_t index = 0U; index < values.size(); ++index) {
    expected[index] = values[index] % 1000U;
  }
  REQUIRE_EQ(result, expected);
}

TEST_CASE(subset_convolution_preserves_rank_layers_and_commutativity) {
  constexpr std::uint64_t modulus = 1'000'003U;
  constexpr std::size_t table_size = 16U;
  std::vector<std::uint64_t> first(table_size, 0U);
  std::vector<std::uint64_t> second(table_size, 0U);
  for (std::size_t mask = 0U; mask < table_size; ++mask) {
    const unsigned int rank = static_cast<unsigned int>(std::popcount(mask));
    if (rank == 1U) {
      first[mask] = static_cast<std::uint64_t>(mask + 1U);
    }
    if (rank == 2U) {
      second[mask] = static_cast<std::uint64_t>(2U * mask + 3U);
    }
  }

  const auto result = algorithms::combinatorial::subset_convolution_mod(
      first, second, modulus);
  const auto reversed = algorithms::combinatorial::subset_convolution_mod(
      second, first, modulus);
  REQUIRE_EQ(result, reversed);
  for (std::size_t mask = 0U; mask < table_size; ++mask) {
    if (std::popcount(mask) != 3) {
      REQUIRE_EQ(result[mask], 0U);
    }
  }
  REQUIRE_EQ(result, naive_subset_convolution(first, second, modulus));
}

TEST_CASE(subset_convolution_matches_full_width_known_answer) {
  constexpr std::uint64_t modulus =
      std::numeric_limits<std::uint64_t>::max() - 58U;
  const std::vector<std::uint64_t> first{
      std::numeric_limits<std::uint64_t>::max(),
      std::numeric_limits<std::uint64_t>::max() - 1U,
      12'345'678'901'234'567'890ULL, 9'876'543'210'987'654'321ULL};
  const std::vector<std::uint64_t> second{
      std::numeric_limits<std::uint64_t>::max() - 2U, 17U,
      5'555'555'555'555'555'555ULL,
      std::numeric_limits<std::uint64_t>::max() - 3U};
  const std::vector<std::uint64_t> expected{
      3248U, 4178U, 17'456'060'711'042'239'952ULL,
      9'718'471'527'808'975'625ULL};

  REQUIRE_EQ(algorithms::combinatorial::subset_convolution_mod(
                 first, second, modulus),
             expected);
}

TEST_CASE(subset_convolution_randomized_differential_against_naive) {
  std::mt19937_64 rng(0x5A17C0A7ULL);
  std::uniform_int_distribution<unsigned int> bits_distribution(0U, 8U);
  std::uniform_int_distribution<std::uint64_t> modulus_distribution(2U,
                                                                    1'000'003U);

  for (std::size_t trial = 0U; trial < 240U; ++trial) {
    const unsigned int bit_count = bits_distribution(rng);
    const std::size_t table_size = std::size_t{1U} << bit_count;
    const std::uint64_t modulus = modulus_distribution(rng);
    std::vector<std::uint64_t> first(table_size, 0U);
    std::vector<std::uint64_t> second(table_size, 0U);
    for (std::size_t index = 0U; index < table_size; ++index) {
      first[index] = rng();
      second[index] = rng();
    }

    const auto actual = algorithms::combinatorial::subset_convolution_mod(
        first, second, modulus);
    const auto expected = naive_subset_convolution(first, second, modulus);
    REQUIRE_EQ(actual, expected);
  }
}
