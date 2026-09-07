#include "algorithms/polynomials/number_theoretic_transform.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::polynomials::convolution_mod_998244353;
using algorithms::polynomials::kNttModulus;
using algorithms::polynomials::number_theoretic_transform;

namespace {

std::vector<std::uint64_t> naive_convolution(
    const std::vector<std::uint64_t>& left,
    const std::vector<std::uint64_t>& right) {
  if (left.empty() || right.empty()) {
    return {};
  }
  std::vector<std::uint64_t> result(left.size() + right.size() - 1U, 0U);
  for (std::size_t left_index = 0U; left_index < left.size(); ++left_index) {
    const std::uint64_t left_value = left[left_index] % kNttModulus;
    for (std::size_t right_index = 0U; right_index < right.size();
         ++right_index) {
      const std::uint64_t right_value = right[right_index] % kNttModulus;
      const std::uint64_t term =
          (left_value * right_value) % kNttModulus;
      result[left_index + right_index] =
          (result[left_index + right_index] + term) % kNttModulus;
    }
  }
  return result;
}

}  // namespace

TEST_CASE(ntt_deterministic_convolution_and_validation) {
  REQUIRE_EQ(convolution_mod_998244353({1U, 2U, 3U}, {4U, 5U}),
             (std::vector<std::uint64_t>{4U, 13U, 22U, 15U}));
  REQUIRE(convolution_mod_998244353({}, {1U}).empty());
  REQUIRE(convolution_mod_998244353({1U}, {}).empty());

  std::vector<std::uint64_t> empty;
  number_theoretic_transform(empty, false);
  REQUIRE(empty.empty());

  std::vector<std::uint64_t> singleton{~std::uint64_t{0}};
  number_theoretic_transform(singleton, false);
  REQUIRE_EQ(singleton[0], (~std::uint64_t{0}) % kNttModulus);
  number_theoretic_transform(singleton, true);
  REQUIRE_EQ(singleton[0], (~std::uint64_t{0}) % kNttModulus);

  std::vector<std::uint64_t> non_power_of_two(3U, 0U);
  REQUIRE_THROWS_AS(number_theoretic_transform(non_power_of_two, false),
                    std::invalid_argument);
}

TEST_CASE(ntt_round_trip_randomized) {
  std::mt19937_64 rng(0x8A11CEULL);
  for (std::size_t size = 1U; size <= 4096U; size <<= 1U) {
    for (std::size_t trial = 0U; trial < 10U; ++trial) {
      std::vector<std::uint64_t> values(size, 0U);
      for (auto& value : values) {
        value = rng();
      }
      auto expected = values;
      for (auto& value : expected) {
        value %= kNttModulus;
      }
      number_theoretic_transform(values, false);
      number_theoretic_transform(values, true);
      REQUIRE_EQ(values, expected);
    }
  }
}

TEST_CASE(ntt_convolution_randomized_against_naive_oracle) {
  std::mt19937_64 rng(0xC011AB1EULL);
  std::uniform_int_distribution<std::size_t> length_dist(0U, 80U);
  for (std::size_t trial = 0U; trial < 600U; ++trial) {
    std::vector<std::uint64_t> left(length_dist(rng), 0U);
    std::vector<std::uint64_t> right(length_dist(rng), 0U);
    for (auto& value : left) {
      value = rng();
    }
    for (auto& value : right) {
      value = rng();
    }
    REQUIRE_EQ(convolution_mod_998244353(left, right),
               naive_convolution(left, right));
  }
}
