#pragma once

#include "algorithms/coding/bch_15_7_5.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

using algorithms::coding::Bch1575Codeword;
using algorithms::coding::Bch1575DecodeResult;
using algorithms::coding::Bch1575Message;
using algorithms::coding::bch_15_7_5_decode;
using algorithms::coding::bch_15_7_5_encode;

namespace bch_15_7_5_test_detail {

inline constexpr std::uint16_t kGenerator =
    UINT16_C(0x01D1);
inline constexpr std::uint16_t kCodewordMask =
    UINT16_C(0x7FFF);

[[nodiscard]] inline Bch1575Message message_from_mask(
    const std::uint16_t mask) {
  Bch1575Message message{};
  for (std::size_t index = 0U; index < message.size(); ++index) {
    message[index] =
        static_cast<std::uint8_t>((mask >> index) & UINT16_C(1));
  }
  return message;
}

[[nodiscard]] inline std::uint16_t mask_from_codeword(
    const Bch1575Codeword& codeword) {
  std::uint16_t mask = 0U;
  for (std::size_t index = 0U; index < codeword.size(); ++index) {
    if (codeword[index] != 0U) {
      mask |= static_cast<std::uint16_t>(
          UINT16_C(1) << index);
    }
  }
  return mask;
}

// Independent codebook construction: every degree<7 multiplier q(x) produces
// one cyclic codeword q(x)g(x). This does not use systematic division,
// syndromes, Berlekamp-Massey, or Chien search.
[[nodiscard]] inline std::vector<std::uint16_t>
independent_codebook() {
  std::vector<std::uint16_t> words;
  words.reserve(128U);
  for (std::uint16_t multiplier = 0U;
       multiplier < 128U; ++multiplier) {
    std::uint16_t codeword = 0U;
    for (std::size_t degree = 0U; degree < 7U; ++degree) {
      if (((multiplier >> degree) & UINT16_C(1)) != 0U) {
        codeword ^= static_cast<std::uint16_t>(
            kGenerator << degree);
      }
    }
    words.push_back(
        static_cast<std::uint16_t>(codeword & kCodewordMask));
  }
  std::sort(words.begin(), words.end());
  return words;
}

[[nodiscard]] inline std::size_t hamming_distance(
    const std::uint16_t first,
    const std::uint16_t second) {
  return static_cast<std::size_t>(
      std::popcount(static_cast<unsigned int>(
          (first ^ second) & kCodewordMask)));
}

[[nodiscard]] inline std::optional<std::uint16_t>
unique_codeword_within_two(
    const std::uint16_t received,
    const std::vector<std::uint16_t>& codebook) {
  std::size_t best_distance = 16U;
  std::uint16_t best = 0U;
  bool tied = false;

  for (const std::uint16_t candidate : codebook) {
    const std::size_t distance =
        hamming_distance(received, candidate);
    if (distance < best_distance) {
      best_distance = distance;
      best = candidate;
      tied = false;
    } else if (distance == best_distance) {
      tied = true;
    }
  }

  if (best_distance > 2U || tied) {
    return std::nullopt;
  }
  return best;
}

[[nodiscard]] inline Bch1575Codeword codeword_from_mask(
    const std::uint16_t mask) {
  Bch1575Codeword codeword{};
  for (std::size_t index = 0U; index < codeword.size(); ++index) {
    codeword[index] =
        static_cast<std::uint8_t>((mask >> index) & UINT16_C(1));
  }
  return codeword;
}

[[nodiscard]] inline std::vector<std::uint16_t>
error_patterns_up_to_two() {
  std::vector<std::uint16_t> patterns;
  patterns.reserve(121U);
  patterns.push_back(0U);
  for (std::size_t first = 0U; first < 15U; ++first) {
    patterns.push_back(static_cast<std::uint16_t>(
        UINT16_C(1) << first));
  }
  for (std::size_t first = 0U; first < 15U; ++first) {
    for (std::size_t second = first + 1U;
         second < 15U; ++second) {
      patterns.push_back(static_cast<std::uint16_t>(
          (UINT16_C(1) << first) |
          (UINT16_C(1) << second)));
    }
  }
  return patterns;
}

}  // namespace bch_15_7_5_test_detail

TEST_CASE(bch_15_7_5_independent_codebook_has_128_distance_five_words) {
  using namespace bch_15_7_5_test_detail;

  const auto codebook = independent_codebook();
  REQUIRE_EQ(codebook.size(), 128U);
  REQUIRE(std::adjacent_find(codebook.begin(), codebook.end()) ==
          codebook.end());

  std::size_t minimum_distance = 15U;
  for (std::size_t first = 0U; first < codebook.size(); ++first) {
    for (std::size_t second = first + 1U;
         second < codebook.size(); ++second) {
      minimum_distance = std::min(
          minimum_distance,
          hamming_distance(codebook[first], codebook[second]));
    }
  }
  REQUIRE_EQ(minimum_distance, 5U);
}

TEST_CASE(bch_15_7_5_systematic_encode_spans_the_independent_code) {
  using namespace bch_15_7_5_test_detail;

  const auto codebook = independent_codebook();
  std::vector<std::uint16_t> production_words;
  production_words.reserve(128U);

  for (std::uint16_t message_mask = 0U;
       message_mask < 128U; ++message_mask) {
    const Bch1575Message message =
        message_from_mask(message_mask);
    const Bch1575Codeword encoded =
        bch_15_7_5_encode(message);
    const std::uint16_t encoded_mask =
        mask_from_codeword(encoded);

    REQUIRE_EQ(
        static_cast<std::uint16_t>(encoded_mask >> 8U),
        message_mask);
    REQUIRE(std::binary_search(
        codebook.begin(), codebook.end(), encoded_mask));
    production_words.push_back(encoded_mask);
  }

  std::sort(production_words.begin(), production_words.end());
  REQUIRE(production_words == codebook);
}

TEST_CASE(bch_15_7_5_exhaustively_corrects_every_zero_one_two_bit_error) {
  using namespace bch_15_7_5_test_detail;

  const auto codebook = independent_codebook();
  const auto errors = error_patterns_up_to_two();

  for (std::uint16_t message_mask = 0U;
       message_mask < 128U; ++message_mask) {
    const Bch1575Message message =
        message_from_mask(message_mask);
    const Bch1575Codeword encoded =
        bch_15_7_5_encode(message);
    const std::uint16_t original =
        mask_from_codeword(encoded);

    for (const std::uint16_t error_mask : errors) {
      const std::uint16_t received_mask =
          static_cast<std::uint16_t>(
              original ^ error_mask);
      const auto oracle =
          unique_codeword_within_two(received_mask, codebook);
      REQUIRE(oracle.has_value());
      REQUIRE_EQ(*oracle, original);

      const Bch1575Codeword received =
          codeword_from_mask(received_mask);
      const auto decoded = bch_15_7_5_decode(received);
      REQUIRE(decoded.has_value());
      REQUIRE(decoded->message == message);
      REQUIRE_EQ(
          mask_from_codeword(decoded->corrected_codeword),
          original);

      std::vector<std::size_t> expected_positions;
      for (std::size_t position = 0U;
           position < 15U; ++position) {
        if (((error_mask >> position) & UINT16_C(1)) != 0U) {
          expected_positions.push_back(position);
        }
      }
      REQUIRE(decoded->error_positions == expected_positions);
    }
  }
}

TEST_CASE(bch_15_7_5_beyond_radius_never_returns_an_invalid_correction) {
  using namespace bch_15_7_5_test_detail;

  const auto codebook = independent_codebook();
  std::mt19937_64 random(0xBCH1575ULL);

  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const std::uint16_t message_mask =
        static_cast<std::uint16_t>(random() % 128U);
    const Bch1575Codeword encoded =
        bch_15_7_5_encode(message_from_mask(message_mask));
    std::uint16_t received =
        mask_from_codeword(encoded);

    std::array<std::size_t, 15U> positions{};
    for (std::size_t index = 0U; index < positions.size(); ++index) {
      positions[index] = index;
    }
    std::shuffle(positions.begin(), positions.end(), random);

    const std::size_t error_count =
        3U + static_cast<std::size_t>(random() % 4U);
    for (std::size_t index = 0U; index < error_count; ++index) {
      received ^= static_cast<std::uint16_t>(
          UINT16_C(1) << positions[index]);
    }

    const auto decoded =
        bch_15_7_5_decode(codeword_from_mask(received));
    if (!decoded.has_value()) {
      continue;
    }

    const std::uint16_t corrected =
        mask_from_codeword(decoded->corrected_codeword);
    REQUIRE(std::binary_search(
        codebook.begin(), codebook.end(), corrected));
    REQUIRE(
        hamming_distance(received, corrected) <= 2U);
    REQUIRE(
        bch_15_7_5_encode(decoded->message) ==
        decoded->corrected_codeword);
  }
}

TEST_CASE(bch_15_7_5_rejects_non_binary_or_wrong_length_inputs) {
  const std::vector<std::uint8_t> short_message(6U, 0U);
  const std::vector<std::uint8_t> long_message(8U, 0U);
  const std::vector<std::uint8_t> short_word(14U, 0U);
  const std::vector<std::uint8_t> long_word(16U, 0U);

  REQUIRE_THROWS_AS(
      bch_15_7_5_encode(short_message), std::invalid_argument);
  REQUIRE_THROWS_AS(
      bch_15_7_5_encode(long_message), std::invalid_argument);
  REQUIRE_THROWS_AS(
      bch_15_7_5_decode(short_word), std::invalid_argument);
  REQUIRE_THROWS_AS(
      bch_15_7_5_decode(long_word), std::invalid_argument);

  Bch1575Message bad_message{};
  bad_message[3U] = 2U;
  REQUIRE_THROWS_AS(
      bch_15_7_5_encode(bad_message), std::invalid_argument);

  Bch1575Codeword bad_word{};
  bad_word[11U] = 3U;
  REQUIRE_THROWS_AS(
      bch_15_7_5_decode(bad_word), std::invalid_argument);
}
