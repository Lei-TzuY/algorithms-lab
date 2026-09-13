#pragma once

#include "algorithms/combinatorial/bitwise_convolution.hpp"
#include "test_framework.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace bitwise_convolution_test_detail {

inline std::vector<std::uint64_t> naive(
    const std::vector<std::uint64_t>& first,
    const std::vector<std::uint64_t>& second, std::uint64_t modulus,
    algorithms::combinatorial::BitwiseConvolutionKind kind) {
  std::vector<std::uint64_t> result(first.size(), 0U);
  for (std::size_t left_mask = 0U; left_mask < first.size(); ++left_mask) {
    for (std::size_t right_mask = 0U; right_mask < second.size(); ++right_mask) {
      std::size_t target = 0U;
      switch (kind) {
        case algorithms::combinatorial::BitwiseConvolutionKind::bit_or:
          target = left_mask | right_mask;
          break;
        case algorithms::combinatorial::BitwiseConvolutionKind::bit_and:
          target = left_mask & right_mask;
          break;
        case algorithms::combinatorial::BitwiseConvolutionKind::bit_xor:
          target = left_mask ^ right_mask;
          break;
      }
      const std::uint64_t left = first[left_mask] % modulus;
      const std::uint64_t right = second[right_mask] % modulus;
      // Randomized tests use modulus <= 1,000,003, so this independent direct
      // product is safely representable and does not reuse multiply_mod.
      const std::uint64_t product = (left * right) % modulus;
      result[target] = (result[target] + product) % modulus;
    }
  }
  return result;
}

}  // namespace bitwise_convolution_test_detail

TEST_CASE(bitwise_convolution_validates_shape_modulus_and_xor_invertibility) {
  using algorithms::combinatorial::BitwiseConvolutionKind;
  using algorithms::combinatorial::bitwise_convolution_mod;
  const std::vector<std::uint64_t> empty;
  const std::vector<std::uint64_t> one{1U};
  const std::vector<std::uint64_t> two{1U, 2U};
  const std::vector<std::uint64_t> three{1U, 2U, 3U};

  REQUIRE_THROWS_AS(bitwise_convolution_mod(
                        one, two, 17U, BitwiseConvolutionKind::bit_or),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(bitwise_convolution_mod(
                        empty, empty, 17U, BitwiseConvolutionKind::bit_or),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(bitwise_convolution_mod(
                        three, three, 17U, BitwiseConvolutionKind::bit_or),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(bitwise_convolution_mod(
                        one, one, 1U, BitwiseConvolutionKind::bit_and),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(bitwise_convolution_mod(
                        two, two, 18U, BitwiseConvolutionKind::bit_xor),
                    std::invalid_argument);
}

TEST_CASE(bitwise_convolution_known_vectors_and_composite_moduli) {
  using algorithms::combinatorial::BitwiseConvolutionKind;
  using algorithms::combinatorial::bitwise_convolution_mod;
  const std::vector<std::uint64_t> first{1U, 2U, 3U, 4U};
  const std::vector<std::uint64_t> second{5U, 6U, 7U, 8U};

  for (const auto kind : {BitwiseConvolutionKind::bit_or,
                          BitwiseConvolutionKind::bit_and,
                          BitwiseConvolutionKind::bit_xor}) {
    REQUIRE_EQ(bitwise_convolution_mod(first, second, 15U, kind),
               bitwise_convolution_test_detail::naive(first, second, 15U,
                                                       kind));
  }

  REQUIRE_EQ(bitwise_convolution_mod(first, second, 1000U,
                                     BitwiseConvolutionKind::bit_or),
             bitwise_convolution_test_detail::naive(
                 first, second, 1000U, BitwiseConvolutionKind::bit_or));
  REQUIRE_EQ(bitwise_convolution_mod(first, second, 1000U,
                                     BitwiseConvolutionKind::bit_and),
             bitwise_convolution_test_detail::naive(
                 first, second, 1000U, BitwiseConvolutionKind::bit_and));
}

TEST_CASE(bitwise_convolution_matches_full_width_known_answers) {
  using algorithms::combinatorial::BitwiseConvolutionKind;
  using algorithms::combinatorial::bitwise_convolution_mod;
  constexpr std::uint64_t modulus = std::numeric_limits<std::uint64_t>::max();
  const std::vector<std::uint64_t> first{
      modulus - 1U, modulus - 2U, modulus - 3U, modulus - 4U};
  const std::vector<std::uint64_t> second{
      modulus - 5U, modulus - 6U, modulus - 7U, modulus - 8U};

  REQUIRE_EQ(bitwise_convolution_mod(first, second, modulus,
                                     BitwiseConvolutionKind::bit_or),
             (std::vector<std::uint64_t>{5U, 28U, 43U, 184U}));
  REQUIRE_EQ(bitwise_convolution_mod(first, second, modulus,
                                     BitwiseConvolutionKind::bit_and),
             (std::vector<std::uint64_t>{103U, 52U, 73U, 32U}));
  REQUIRE_EQ(bitwise_convolution_mod(first, second, modulus,
                                     BitwiseConvolutionKind::bit_xor),
             (std::vector<std::uint64_t>{70U, 68U, 62U, 60U}));
}

TEST_CASE(bitwise_convolution_randomized_differential_against_direct_pairs) {
  using algorithms::combinatorial::BitwiseConvolutionKind;
  using algorithms::combinatorial::bitwise_convolution_mod;
  constexpr std::array<std::uint64_t, 6> moduli{
      2U, 3U, 17U, 97U, 1000U, 1'000'003U};
  std::mt19937_64 rng(0xB17C0A7ULL);

  for (std::size_t trial = 0U; trial < 600U; ++trial) {
    const unsigned int bit_count = static_cast<unsigned int>(rng() % 8U);
    const std::size_t table_size = std::size_t{1U} << bit_count;
    std::vector<std::uint64_t> first(table_size, 0U);
    std::vector<std::uint64_t> second(table_size, 0U);
    for (std::uint64_t& value : first) {
      value = rng();
    }
    for (std::uint64_t& value : second) {
      value = rng();
    }

    for (const auto kind : {BitwiseConvolutionKind::bit_or,
                            BitwiseConvolutionKind::bit_and,
                            BitwiseConvolutionKind::bit_xor}) {
      for (const std::uint64_t modulus : moduli) {
        if (kind == BitwiseConvolutionKind::bit_xor &&
            (modulus & 1U) == 0U) {
          continue;
        }
        REQUIRE_EQ(bitwise_convolution_mod(first, second, modulus, kind),
                   bitwise_convolution_test_detail::naive(
                       first, second, modulus, kind));
      }
    }
  }
}
