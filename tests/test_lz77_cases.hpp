#pragma once

#include "algorithms/coding/lz77.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace lz77_recovery_tests {
using algorithms::coding::Lz77Token;
using algorithms::coding::lz77_decode;
using algorithms::coding::lz77_encode;

inline std::vector<Lz77Token> naive_encode(std::string_view input,
                                           std::size_t window,
                                           std::size_t max_length) {
  if (window == 0 || max_length < 3) {
    throw std::invalid_argument("invalid LZ77 oracle configuration");
  }
  std::vector<Lz77Token> out;
  std::size_t position = 0;
  while (position < input.size()) {
    std::size_t best_distance = 0;
    std::size_t best_length = 0;
    const std::size_t max_distance = std::min(window, position);
    const std::size_t limit = std::min(max_length, input.size() - position);
    for (std::size_t distance = 1; distance <= max_distance; ++distance) {
      std::size_t length = 0;
      while (length < limit &&
             input[position + length] == input[position - distance + length]) {
        ++length;
      }
      if (length >= 3 && length > best_length) {
        best_length = length;
        best_distance = distance;
      }
    }
    if (best_length >= 3) {
      out.push_back({best_distance, best_length, std::nullopt});
      position += best_length;
    } else {
      out.push_back({0, 0, static_cast<std::uint8_t>(
                               static_cast<unsigned char>(input[position]))});
      ++position;
    }
  }
  return out;
}

}  // namespace lz77_recovery_tests

TEST_CASE(lz77_empty_literals_and_overlapping_match) {
  using namespace lz77_recovery_tests;
  REQUIRE(lz77_encode("", 8, 16).empty());
  REQUIRE(lz77_decode({}).empty());
  const std::vector<Lz77Token> expected{
      {0, 0, static_cast<std::uint8_t>('a')}, {1, 5, std::nullopt}};
  const auto actual = lz77_encode("aaaaaa", 8, 16);
  REQUIRE_EQ(actual, expected);
  REQUIRE_EQ(lz77_decode(actual), std::string("aaaaaa"));
}

TEST_CASE(lz77_window_length_and_nearest_tie_semantics) {
  using namespace lz77_recovery_tests;
  const std::string input = "abcabcabcabcX";
  const auto actual = lz77_encode(input, 9, 6);
  REQUIRE_EQ(actual, naive_encode(input, 9, 6));
  REQUIRE_EQ(lz77_decode(actual), input);
  bool saw_limited = false;
  for (const auto& token : actual) {
    if (!token.literal.has_value() && token.length == 6) saw_limited = true;
  }
  REQUIRE(saw_limited);

  const std::string tie = "abcXabcYabcZ";
  const auto tied = lz77_encode(tie, 12, 3);
  REQUIRE_EQ(tied, naive_encode(tie, 12, 3));
  REQUIRE_EQ(lz77_decode(tied), tie);
  REQUIRE(tied.size() >= 2);
  REQUIRE(!tied[tied.size() - 2].literal.has_value());
  REQUIRE_EQ(tied[tied.size() - 2].distance, std::size_t{4});
}

TEST_CASE(lz77_arbitrary_bytes_and_decoder_validation) {
  using namespace lz77_recovery_tests;
  std::string input;
  for (std::uint8_t byte : std::vector<std::uint8_t>{
           0, 128, 255, 0, 128, 255, 0, 128, 255, 17}) {
    input.push_back(static_cast<char>(byte));
  }
  const auto actual = lz77_encode(input, 16, 32);
  REQUIRE_EQ(actual, naive_encode(input, 16, 32));
  REQUIRE_EQ(lz77_decode(actual), input);
  REQUIRE_THROWS_AS(lz77_encode(input, 0, 8), std::invalid_argument);
  REQUIRE_THROWS_AS(lz77_encode(input, 8, 2), std::invalid_argument);

  const std::vector<Lz77Token> bad_literal{
      {1, 0, static_cast<std::uint8_t>('x')}};
  REQUIRE_THROWS_AS(lz77_decode(bad_literal), std::invalid_argument);
  const std::vector<Lz77Token> bad_match{{0, 3, std::nullopt}};
  REQUIRE_THROWS_AS(lz77_decode(bad_match), std::invalid_argument);
  const std::vector<Lz77Token> short_match{{1, 2, std::nullopt}};
  REQUIRE_THROWS_AS(lz77_decode(short_match), std::invalid_argument);
  const std::vector<Lz77Token> future_match{{2, 3, std::nullopt}};
  REQUIRE_THROWS_AS(lz77_decode(future_match), std::invalid_argument);
}

TEST_CASE(lz77_randomized_differential_and_round_trip) {
  using namespace lz77_recovery_tests;
  std::mt19937_64 rng(0x1A277ULL);
  for (int trial = 0; trial < 3000; ++trial) {
    const std::size_t length = static_cast<std::size_t>(rng() % 129U);
    const std::size_t window = 1U + static_cast<std::size_t>(rng() % 32U);
    const std::size_t max_length =
        3U + static_cast<std::size_t>(rng() % 30U);
    std::string input;
    input.reserve(length);
    for (std::size_t index = 0; index < length; ++index) {
      const std::uint64_t mode = rng() % 4U;
      const auto byte = static_cast<unsigned char>(
          mode == 0 ? (rng() & 0xFFU) : (rng() % 7U));
      input.push_back(static_cast<char>(byte));
    }
    const auto actual = lz77_encode(input, window, max_length);
    REQUIRE_EQ(actual, naive_encode(input, window, max_length));
    REQUIRE_EQ(lz77_decode(actual), input);
  }
}
