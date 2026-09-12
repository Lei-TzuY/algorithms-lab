#include "algorithms/coding/lz78.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {
using algorithms::coding::Lz78Codeword;
using algorithms::coding::lz78_decode;
using algorithms::coding::lz78_encode;

std::vector<Lz78Codeword> naive_encode(std::string_view input) {
  std::vector<std::string> dictionary{std::string{}};
  std::vector<Lz78Codeword> result;
  std::size_t position = 0;
  while (position < input.size()) {
    std::size_t best = 0;
    std::size_t best_length = 0;
    for (std::size_t index = 1; index < dictionary.size(); ++index) {
      const std::string& phrase = dictionary[index];
      if (phrase.size() <= best_length ||
          phrase.size() > input.size() - position) {
        continue;
      }
      const auto begin = input.begin() + static_cast<std::ptrdiff_t>(position);
      if (std::equal(phrase.begin(), phrase.end(), begin)) {
        best = index;
        best_length = phrase.size();
      }
    }
    if (best_length == input.size() - position) {
      result.push_back({best, std::nullopt});
      break;
    }
    const auto byte = static_cast<std::uint8_t>(
        static_cast<unsigned char>(input[position + best_length]));
    result.push_back({best, byte});
    std::string phrase = dictionary[best];
    phrase.push_back(static_cast<char>(byte));
    dictionary.push_back(std::move(phrase));
    position += best_length + 1;
  }
  return result;
}
}  // namespace

TEST_CASE(lz78_empty_and_terminal_phrase_semantics) {
  REQUIRE(lz78_encode("").empty());
  REQUIRE(lz78_decode({}).empty());
  const std::vector<Lz78Codeword> expected{
      {0, static_cast<std::uint8_t>('a')},
      {1, static_cast<std::uint8_t>('a')},
      {2, std::nullopt}};
  const auto actual = lz78_encode("aaaaa");
  REQUIRE_EQ(actual, expected);
  REQUIRE_EQ(lz78_decode(actual), std::string("aaaaa"));
}

TEST_CASE(lz78_classic_dictionary_growth) {
  const std::vector<Lz78Codeword> expected{
      {0, static_cast<std::uint8_t>('A')},
      {0, static_cast<std::uint8_t>('B')},
      {1, static_cast<std::uint8_t>('A')},
      {2, static_cast<std::uint8_t>('A')},
      {4, static_cast<std::uint8_t>('A')},
      {4, static_cast<std::uint8_t>('B')}};
  const auto actual = lz78_encode("ABAABABAABAB");
  REQUIRE_EQ(actual, expected);
  REQUIRE_EQ(lz78_decode(actual), std::string("ABAABABAABAB"));
}

TEST_CASE(lz78_arbitrary_byte_round_trip_and_naive_parse) {
  std::string input;
  for (std::uint8_t byte :
       std::vector<std::uint8_t>{0, 255, 0, 128, 255, 0, 128, 255, 17, 17, 0}) {
    input.push_back(static_cast<char>(byte));
  }
  const auto actual = lz78_encode(input);
  REQUIRE_EQ(actual, naive_encode(input));
  REQUIRE_EQ(lz78_decode(actual), input);
}

TEST_CASE(lz78_decoder_rejects_invalid_references_and_terminal_placement) {
  const std::vector<Lz78Codeword> future{{1, static_cast<std::uint8_t>('x')}};
  REQUIRE_THROWS_AS(lz78_decode(future), std::invalid_argument);
  const std::vector<Lz78Codeword> empty_terminal{{0, std::nullopt}};
  REQUIRE_THROWS_AS(lz78_decode(empty_terminal), std::invalid_argument);
  const std::vector<Lz78Codeword> early_terminal{
      {0, static_cast<std::uint8_t>('a')},
      {1, std::nullopt},
      {0, static_cast<std::uint8_t>('b')}};
  REQUIRE_THROWS_AS(lz78_decode(early_terminal), std::invalid_argument);
}

TEST_CASE(lz78_randomized_differential_and_round_trip) {
  std::mt19937_64 rng(0x1A278ULL);
  for (int trial = 0; trial < 2500; ++trial) {
    const std::size_t length = static_cast<std::size_t>(rng() % 97U);
    std::string input;
    input.reserve(length);
    for (std::size_t index = 0; index < length; ++index) {
      input.push_back(
          static_cast<char>(static_cast<unsigned char>(rng() & 0xFFU)));
    }
    const auto actual = lz78_encode(input);
    REQUIRE_EQ(actual, naive_encode(input));
    REQUIRE_EQ(lz78_decode(actual), input);
  }
}
