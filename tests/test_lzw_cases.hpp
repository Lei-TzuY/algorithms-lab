#pragma once

#include "algorithms/coding/lzw.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace lzw_test_detail {

using algorithms::coding::LzwCode;
using algorithms::coding::lzw_decode_bytes;
using algorithms::coding::lzw_encode_bytes;

inline std::vector<LzwCode> naive_lzw_encode(const std::string_view input) {
  std::vector<std::string> dictionary;
  dictionary.reserve(256U + input.size());
  for (std::size_t value = 0U; value < 256U; ++value) {
    dictionary.emplace_back(
        1U, static_cast<char>(static_cast<unsigned char>(value)));
  }

  std::vector<LzwCode> output;
  std::size_t position = 0U;
  while (position < input.size()) {
    std::size_t best_code = 0U;
    std::size_t best_length = 0U;
    for (std::size_t code = 0U; code < dictionary.size(); ++code) {
      const std::string& phrase = dictionary[code];
      if (phrase.size() <= best_length ||
          phrase.size() > input.size() - position) {
        continue;
      }
      if (input.substr(position, phrase.size()) == phrase) {
        best_code = code;
        best_length = phrase.size();
      }
    }

    if (best_length == 0U) {
      throw std::logic_error("naive LZW oracle failed to match one byte");
    }
    output.push_back(static_cast<LzwCode>(best_code));

    if (position + best_length < input.size() &&
        dictionary.size() <=
            static_cast<std::size_t>(std::numeric_limits<LzwCode>::max())) {
      std::string next_phrase = dictionary[best_code];
      next_phrase.push_back(input[position + best_length]);
      dictionary.push_back(std::move(next_phrase));
    }
    position += best_length;
  }
  return output;
}

inline char byte_char(const unsigned value) {
  return static_cast<char>(static_cast<unsigned char>(value));
}

}  // namespace lzw_test_detail

TEST_CASE(lzw_known_phrase_and_empty_input) {
  using namespace lzw_test_detail;

  REQUIRE(lzw_encode_bytes("").empty());
  REQUIRE(lzw_decode_bytes({}).empty());

  const std::string input = "TOBEORNOTTOBEORTOBEORNOT";
  const std::vector<LzwCode> expected = {
      84U, 79U, 66U, 69U, 79U, 82U, 78U, 79U,
      84U, 256U, 258U, 260U, 265U, 259U, 261U, 263U};
  REQUIRE(lzw_encode_bytes(input) == expected);
  REQUIRE(lzw_decode_bytes(expected) == input);
}

TEST_CASE(lzw_kwkwk_case_and_arbitrary_bytes_round_trip) {
  using namespace lzw_test_detail;

  const std::vector<LzwCode> kwkwk = {65U, 66U, 256U, 258U};
  REQUIRE(lzw_encode_bytes("ABABABA") == kwkwk);
  REQUIRE(lzw_decode_bytes(kwkwk) == "ABABABA");
  REQUIRE(lzw_decode_bytes({65U, 256U}) == "AAA");

  std::string bytes;
  for (std::size_t repeat = 0U; repeat < 24U; ++repeat) {
    bytes.push_back(byte_char(0x00U));
    bytes.push_back(byte_char(0x80U));
    bytes.push_back(byte_char(0xffU));
    bytes.push_back(byte_char(0x00U));
  }
  const auto codes = lzw_encode_bytes(bytes);
  REQUIRE(lzw_decode_bytes(codes) == bytes);
}

TEST_CASE(lzw_rejects_malformed_code_streams) {
  using namespace lzw_test_detail;

  REQUIRE_THROWS_AS(lzw_decode_bytes({256U}), std::invalid_argument);
  REQUIRE_THROWS_AS(lzw_decode_bytes({65U, 258U}), std::invalid_argument);
  REQUIRE_THROWS_AS(
      lzw_decode_bytes({65U, std::numeric_limits<LzwCode>::max()}),
      std::invalid_argument);
}

TEST_CASE(lzw_repeated_phrase_chain_remains_replayable) {
  using namespace lzw_test_detail;

  const std::string input(4096U, 'a');
  const auto codes = lzw_encode_bytes(input);
  REQUIRE(!codes.empty());
  REQUIRE(lzw_decode_bytes(codes) == input);
  REQUIRE(codes == naive_lzw_encode(input));
}

TEST_CASE(lzw_randomized_parser_matches_independent_dictionary_oracle) {
  using namespace lzw_test_detail;

  std::mt19937_64 random(0x1A2B3C4DULL);
  for (std::size_t trial = 0U; trial < 2500U; ++trial) {
    const std::size_t length = static_cast<std::size_t>(random() % 97U);
    std::string input;
    input.reserve(length);
    for (std::size_t index = 0U; index < length; ++index) {
      input.push_back(byte_char(static_cast<unsigned>(random() % 256U)));
    }

    const auto production = lzw_encode_bytes(input);
    const auto oracle = naive_lzw_encode(input);
    REQUIRE(production == oracle);
    REQUIRE(lzw_decode_bytes(production) == input);
    REQUIRE(lzw_encode_bytes(input) == production);
  }
}
