#pragma once

#include "algorithms/coding/rans_byte.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>

using algorithms::coding::RansByteBlock;
using algorithms::coding::kRansByteLowerBound;
using algorithms::coding::kRansByteTotalFrequency;
using algorithms::coding::rans_decode_bytes;
using algorithms::coding::rans_encode_bytes;

namespace rans_byte_test_detail {

inline std::uint32_t frequency_sum(const RansByteBlock& block) {
  std::uint32_t total = 0U;
  for (const std::uint16_t frequency : block.frequencies) {
    total += frequency;
  }
  return total;
}

inline void require_observed_have_positive_frequency(
    const std::string& input, const RansByteBlock& block) {
  std::array<bool, 256U> observed{};
  for (const char byte : input) {
    observed[static_cast<std::uint8_t>(
        static_cast<unsigned char>(byte))] = true;
  }
  for (std::size_t symbol = 0U; symbol < observed.size(); ++symbol) {
    if (observed[symbol]) {
      REQUIRE(block.frequencies[symbol] > 0U);
    }
  }
}

}  // namespace rans_byte_test_detail

TEST_CASE(rans_byte_empty_and_single_symbol_contract) {
  using namespace rans_byte_test_detail;

  const RansByteBlock empty = rans_encode_bytes("");
  REQUIRE_EQ(empty.symbol_count, 0U);
  REQUIRE_EQ(empty.state, kRansByteLowerBound);
  REQUIRE(empty.bytes.empty());
  REQUIRE_EQ(frequency_sum(empty), 0U);
  REQUIRE_EQ(rans_decode_bytes(empty), std::string{});

  const std::string repeated(4096U, static_cast<char>(0xA5));
  const RansByteBlock block = rans_encode_bytes(repeated);
  REQUIRE_EQ(block.symbol_count, repeated.size());
  REQUIRE_EQ(frequency_sum(block), kRansByteTotalFrequency);
  REQUIRE_EQ(block.frequencies[0xA5U], kRansByteTotalFrequency);
  REQUIRE_EQ(block.state, kRansByteLowerBound);
  REQUIRE(block.bytes.empty());
  REQUIRE_EQ(rans_decode_bytes(block), repeated);
}

TEST_CASE(rans_byte_known_balanced_model_is_deterministic) {
  using namespace rans_byte_test_detail;

  const std::string input = "abab";
  const RansByteBlock first = rans_encode_bytes(input);
  const RansByteBlock second = rans_encode_bytes(input);

  REQUIRE(first == second);
  REQUIRE_EQ(first.frequencies[static_cast<std::uint8_t>('a')], 2048U);
  REQUIRE_EQ(first.frequencies[static_cast<std::uint8_t>('b')], 2048U);
  REQUIRE_EQ(frequency_sum(first), kRansByteTotalFrequency);
  REQUIRE_EQ(first.state, UINT64_C(34359758848));
  REQUIRE(first.bytes.empty());
  REQUIRE_EQ(rans_decode_bytes(first), input);
}

TEST_CASE(rans_byte_all_byte_values_and_skewed_distribution_roundtrip) {
  using namespace rans_byte_test_detail;

  std::string all_bytes;
  for (std::size_t repeat = 0U; repeat < 7U; ++repeat) {
    for (std::size_t symbol = 0U; symbol < 256U; ++symbol) {
      all_bytes.push_back(static_cast<char>(symbol));
    }
  }

  const RansByteBlock uniform = rans_encode_bytes(all_bytes);
  REQUIRE_EQ(frequency_sum(uniform), kRansByteTotalFrequency);
  require_observed_have_positive_frequency(all_bytes, uniform);
  REQUIRE_EQ(rans_decode_bytes(uniform), all_bytes);

  std::string skewed(100000U, '\0');
  for (std::size_t symbol = 1U; symbol < 256U; ++symbol) {
    skewed.push_back(static_cast<char>(symbol));
  }

  const RansByteBlock skewed_block = rans_encode_bytes(skewed);
  REQUIRE_EQ(frequency_sum(skewed_block), kRansByteTotalFrequency);
  require_observed_have_positive_frequency(skewed, skewed_block);
  REQUIRE_EQ(rans_decode_bytes(skewed_block), skewed);
}

TEST_CASE(rans_byte_arbitrary_binary_random_roundtrip) {
  using namespace rans_byte_test_detail;

  std::mt19937_64 random(0x52414E535EEDULL);
  for (std::size_t trial = 0U; trial < 900U; ++trial) {
    const std::size_t size =
        static_cast<std::size_t>(random() % 513U);
    std::string input(size, '\0');

    const std::uint64_t alphabet_pick = random() % 4U;
    const std::size_t alphabet =
        alphabet_pick == 0U ? 2U :
        alphabet_pick == 1U ? 7U :
        alphabet_pick == 2U ? 31U : 256U;

    for (char& byte : input) {
      byte = static_cast<char>(random() % alphabet);
    }

    const RansByteBlock block = rans_encode_bytes(input);
    if (input.empty()) {
      REQUIRE_EQ(frequency_sum(block), 0U);
    } else {
      REQUIRE_EQ(frequency_sum(block), kRansByteTotalFrequency);
      require_observed_have_positive_frequency(input, block);
    }
    REQUIRE_EQ(rans_decode_bytes(block), input);
    REQUIRE(rans_encode_bytes(input) == block);
  }
}

TEST_CASE(rans_byte_rejects_malformed_blocks) {
  RansByteBlock empty_with_frequency;
  empty_with_frequency.frequencies[0U] = 1U;
  REQUIRE_THROWS_AS(
      rans_decode_bytes(empty_with_frequency),
      std::invalid_argument);

  RansByteBlock empty_with_bytes;
  empty_with_bytes.bytes.push_back(0U);
  REQUIRE_THROWS_AS(
      rans_decode_bytes(empty_with_bytes),
      std::invalid_argument);

  RansByteBlock wrong_sum;
  wrong_sum.symbol_count = 1U;
  wrong_sum.frequencies[7U] =
      static_cast<std::uint16_t>(kRansByteTotalFrequency - 1U);
  REQUIRE_THROWS_AS(
      rans_decode_bytes(wrong_sum),
      std::invalid_argument);

  const std::string source =
      "banana banana banana banana banana";
  const RansByteBlock valid = rans_encode_bytes(source);
  REQUIRE_EQ(rans_decode_bytes(valid), source);

  RansByteBlock low_state = valid;
  low_state.state = kRansByteLowerBound - 1U;
  REQUIRE_THROWS_AS(
      rans_decode_bytes(low_state),
      std::invalid_argument);

  RansByteBlock truncated = valid;
  if (!truncated.bytes.empty()) {
    truncated.bytes.pop_back();
    REQUIRE_THROWS_AS(
        rans_decode_bytes(truncated),
        std::invalid_argument);
  }

  RansByteBlock trailing = valid;
  trailing.bytes.push_back(0x42U);
  REQUIRE_THROWS_AS(
      rans_decode_bytes(trailing),
      std::invalid_argument);

  RansByteBlock wrong_count = valid;
  ++wrong_count.symbol_count;
  REQUIRE_THROWS_AS(
      rans_decode_bytes(wrong_count),
      std::invalid_argument);
}
