#pragma once

#include "algorithms/coding/huffman_byte_code.hpp"
#include "algorithms/coding/length_limited_huffman.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace length_limited_huffman_tests {

using algorithms::coding::HuffmanByteCodebook;
using algorithms::coding::LengthLimitedHuffmanByteCodebook;

inline std::uint64_t exhaustive_optimum_cost(
    const std::vector<std::uint64_t>& weights, std::size_t limit) {
  if (weights.empty()) {
    return 0U;
  }
  if (weights.size() == 1U) {
    return weights.front();
  }
  if (limit == 0U || limit >= 63U) {
    throw std::invalid_argument("test oracle expects 1 <= limit < 63");
  }

  const std::uint64_t kraft_scale = std::uint64_t{1} << limit;
  std::uint64_t best = std::numeric_limits<std::uint64_t>::max();
  std::vector<std::size_t> lengths(weights.size(), 1U);

  const auto search = [&](auto&& self, std::size_t index) -> void {
    if (index == weights.size()) {
      std::uint64_t kraft = 0U;
      std::uint64_t cost = 0U;
      for (std::size_t item = 0U; item < weights.size(); ++item) {
        kraft += std::uint64_t{1} << (limit - lengths[item]);
        cost += weights[item] * static_cast<std::uint64_t>(lengths[item]);
      }
      if (kraft == kraft_scale) {
        best = std::min(best, cost);
      }
      return;
    }
    for (std::size_t length = 1U; length <= limit; ++length) {
      lengths[index] = length;
      self(self, index + 1U);
    }
  };
  search(search, 0U);

  if (best == std::numeric_limits<std::uint64_t>::max()) {
    throw std::invalid_argument("test oracle found no feasible length vector");
  }
  return best;
}

inline std::size_t minimum_feasible_limit(std::size_t symbol_count) {
  std::size_t limit = 0U;
  std::size_t capacity = 1U;
  while (capacity < symbol_count) {
    capacity *= 2U;
    ++limit;
  }
  return limit;
}

}  // namespace length_limited_huffman_tests

TEST_CASE(length_limited_huffman_empty_singleton_and_infeasible_bounds) {
  using namespace length_limited_huffman_tests;

  std::array<std::uint64_t, 256> empty{};
  LengthLimitedHuffmanByteCodebook empty_codebook(empty, 0U);
  REQUIRE_EQ(empty_codebook.symbol_count(), 0U);
  REQUIRE_EQ(empty_codebook.weighted_bit_count(), 0U);
  REQUIRE(empty_codebook.valid_codebook());

  std::array<std::uint64_t, 256> singleton{};
  singleton[7] = 9U;
  REQUIRE_THROWS_AS(LengthLimitedHuffmanByteCodebook(singleton, 0U),
                    std::invalid_argument);
  LengthLimitedHuffmanByteCodebook one(singleton, 10'000U);
  REQUIRE_EQ(one.symbol_count(), 1U);
  REQUIRE_EQ(one.code_lengths()[7], 1U);
  REQUIRE_EQ(one.code(7).value(), std::string_view("0"));
  REQUIRE_EQ(one.weighted_bit_count(), 9U);
  REQUIRE(one.valid_codebook());

  std::array<std::uint64_t, 256> five{};
  for (std::size_t symbol = 0U; symbol < 5U; ++symbol) {
    five[symbol] = 1U;
  }
  REQUIRE_THROWS_AS(LengthLimitedHuffmanByteCodebook(five, 2U),
                    std::invalid_argument);
}

TEST_CASE(length_limited_huffman_known_optimum_and_canonical_replay) {
  using namespace length_limited_huffman_tests;

  std::array<std::uint64_t, 256> frequencies{};
  frequencies[0] = 1U;
  frequencies[1] = 1U;
  frequencies[2] = 1U;
  frequencies[3] = 100U;

  LengthLimitedHuffmanByteCodebook codebook(frequencies, 3U);
  REQUIRE_EQ(codebook.weighted_bit_count(), 108U);
  REQUIRE_EQ(codebook.code_lengths()[3], 1U);
  REQUIRE(codebook.valid_codebook());

  LengthLimitedHuffmanByteCodebook replay(frequencies, 3U);
  REQUIRE_EQ(replay.code_lengths(), codebook.code_lengths());
  for (std::size_t symbol = 0U; symbol < frequencies.size(); ++symbol) {
    REQUIRE_EQ(replay.code(static_cast<std::uint8_t>(symbol)),
               codebook.code(static_cast<std::uint8_t>(symbol)));
  }

  std::array<std::uint64_t, 256> equal{};
  for (std::size_t symbol = 0U; symbol < 4U; ++symbol) {
    equal[symbol] = 5U;
  }
  LengthLimitedHuffmanByteCodebook equal_codebook(equal, 2U);
  REQUIRE_EQ(equal_codebook.code(0).value(), std::string_view("00"));
  REQUIRE_EQ(equal_codebook.code(1).value(), std::string_view("01"));
  REQUIRE_EQ(equal_codebook.code(2).value(), std::string_view("10"));
  REQUIRE_EQ(equal_codebook.code(3).value(), std::string_view("11"));
  REQUIRE(equal_codebook.valid_codebook());
}

TEST_CASE(length_limited_huffman_checked_cost_boundaries) {
  using namespace length_limited_huffman_tests;

  const std::uint64_t half = std::numeric_limits<std::uint64_t>::max() / 2U;
  std::array<std::uint64_t, 256> representable{};
  representable[0] = half;
  representable[1] = half;
  LengthLimitedHuffmanByteCodebook high(representable, 1U);
  REQUIRE_EQ(high.weighted_bit_count(), half + half);
  REQUIRE(high.valid_codebook());

  std::array<std::uint64_t, 256> overflowing{};
  overflowing[0] = half + 1U;
  overflowing[1] = half + 1U;
  REQUIRE_THROWS_AS(LengthLimitedHuffmanByteCodebook(overflowing, 1U),
                    std::overflow_error);
}

TEST_CASE(length_limited_huffman_randomized_exhaustive_and_unconstrained_crosscheck) {
  using namespace length_limited_huffman_tests;

  std::mt19937_64 rng(0xA77C0DEULL);
  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const std::size_t symbol_count = 2U + static_cast<std::size_t>(rng() % 5U);
    const std::size_t minimum_limit = minimum_feasible_limit(symbol_count);
    const std::size_t limit =
        minimum_limit + static_cast<std::size_t>(rng() % (6U - minimum_limit));

    std::array<std::uint64_t, 256> frequencies{};
    std::vector<std::uint64_t> weights;
    weights.reserve(symbol_count);
    for (std::size_t symbol = 0U; symbol < symbol_count; ++symbol) {
      frequencies[symbol] = 1U + (rng() % 20U);
      weights.push_back(frequencies[symbol]);
    }

    LengthLimitedHuffmanByteCodebook limited(frequencies, limit);
    REQUIRE(limited.valid_codebook());
    REQUIRE_EQ(limited.weighted_bit_count(),
               exhaustive_optimum_cost(weights, limit));

    LengthLimitedHuffmanByteCodebook unconstrained(frequencies, 255U);
    HuffmanByteCodebook ordinary(frequencies);
    REQUIRE_EQ(unconstrained.weighted_bit_count(), ordinary.weighted_bit_count());
  }
}
