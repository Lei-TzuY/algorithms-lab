#pragma once

#include "algorithms/coding/convolutional_viterbi.hpp"
#include "test_framework.hpp"

#include <bit>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using algorithms::coding::RateHalfConvolutionalCode;

std::vector<std::uint8_t> reference_viterbi_encode(
    std::size_t constraint_length, std::uint32_t first_generator,
    std::uint32_t second_generator, const std::vector<std::uint8_t>& message) {
  const std::size_t state_mask =
      (std::size_t{1} << static_cast<unsigned>(constraint_length - 1U)) - 1U;
  std::size_t state = 0U;
  std::vector<std::uint8_t> output;
  output.reserve(message.size() * 2U);
  for (const std::uint8_t bit : message) {
    REQUIRE(bit <= 1U);
    const std::uint32_t reg =
        (static_cast<std::uint32_t>(state) << 1U) | static_cast<std::uint32_t>(bit);
    output.push_back(static_cast<std::uint8_t>(std::popcount(reg & first_generator) & 1));
    output.push_back(static_cast<std::uint8_t>(std::popcount(reg & second_generator) & 1));
    state = static_cast<std::size_t>(reg) & state_mask;
  }
  return output;
}

std::size_t reference_viterbi_final_state(std::size_t constraint_length,
                                          const std::vector<std::uint8_t>& message) {
  const std::size_t state_mask =
      (std::size_t{1} << static_cast<unsigned>(constraint_length - 1U)) - 1U;
  std::size_t state = 0U;
  for (const std::uint8_t bit : message) {
    state = ((state << 1U) | static_cast<std::size_t>(bit)) & state_mask;
  }
  return state;
}

std::size_t hamming_distance(const std::vector<std::uint8_t>& first,
                             const std::vector<std::uint8_t>& second) {
  REQUIRE_EQ(first.size(), second.size());
  std::size_t result = 0U;
  for (std::size_t index = 0U; index < first.size(); ++index) {
    result += static_cast<std::size_t>(first[index] != second[index]);
  }
  return result;
}

std::vector<std::uint8_t> message_from_ordinal(std::size_t ordinal,
                                               std::size_t length) {
  std::vector<std::uint8_t> message(length, 0U);
  for (std::size_t index = 0U; index < length; ++index) {
    const std::size_t reverse = length - 1U - index;
    message[index] = static_cast<std::uint8_t>((ordinal >> reverse) & 1U);
  }
  return message;
}

std::pair<std::size_t, std::vector<std::uint8_t>> exhaustive_viterbi_oracle(
    std::size_t constraint_length, std::uint32_t first_generator,
    std::uint32_t second_generator, const std::vector<std::uint8_t>& received) {
  REQUIRE((received.size() % 2U) == 0U);
  const std::size_t message_length = received.size() / 2U;
  REQUIRE(message_length <= 12U);

  std::size_t best_distance = std::numeric_limits<std::size_t>::max();
  std::vector<std::uint8_t> best_message;
  const std::size_t count = std::size_t{1} << static_cast<unsigned>(message_length);
  for (std::size_t ordinal = 0U; ordinal < count; ++ordinal) {
    auto message = message_from_ordinal(ordinal, message_length);
    const auto encoded = reference_viterbi_encode(
        constraint_length, first_generator, second_generator, message);
    const std::size_t distance = hamming_distance(encoded, received);
    if (distance < best_distance ||
        (distance == best_distance && message < best_message)) {
      best_distance = distance;
      best_message = std::move(message);
    }
  }
  return {best_distance, best_message};
}

TEST_CASE(convolutional_viterbi_known_code_and_validation) {
  REQUIRE_THROWS_AS(RateHalfConvolutionalCode(1U, 1U, 1U), std::invalid_argument);
  REQUIRE_THROWS_AS(RateHalfConvolutionalCode(17U, 1U, 1U), std::invalid_argument);
  REQUIRE_THROWS_AS(RateHalfConvolutionalCode(3U, 0U, 5U), std::invalid_argument);
  REQUIRE_THROWS_AS(RateHalfConvolutionalCode(3U, 8U, 5U), std::invalid_argument);

  const RateHalfConvolutionalCode code(3U, 0b111U, 0b101U);
  const std::vector<std::uint8_t> message{1U, 0U, 1U, 1U};
  const std::vector<std::uint8_t> expected{1U, 1U, 1U, 0U, 0U, 0U, 0U, 1U};
  REQUIRE_EQ(code.encode(message), expected);
  REQUIRE_EQ(code.encode(message),
             reference_viterbi_encode(3U, 0b111U, 0b101U, message));

  const auto decoded = code.decode_hard(expected);
  REQUIRE_EQ(decoded.message_bits, message);
  REQUIRE_EQ(decoded.corrected_codeword, expected);
  REQUIRE_EQ(decoded.hamming_distance, 0U);
  REQUIRE_EQ(decoded.final_state, reference_viterbi_final_state(3U, message));

  REQUIRE_THROWS_AS(code.encode(std::vector<std::uint8_t>{2U}), std::invalid_argument);
  REQUIRE_THROWS_AS(code.decode_hard(std::vector<std::uint8_t>{0U}), std::invalid_argument);
  REQUIRE_THROWS_AS(code.decode_hard(std::vector<std::uint8_t>{0U, 3U}), std::invalid_argument);
}

TEST_CASE(convolutional_viterbi_empty_ties_and_replay) {
  const RateHalfConvolutionalCode code(2U, 0b10U, 0b10U);
  const auto empty = code.decode_hard({});
  REQUIRE(empty.message_bits.empty());
  REQUIRE(empty.corrected_codeword.empty());
  REQUIRE_EQ(empty.hamming_distance, 0U);
  REQUIRE_EQ(empty.final_state, 0U);

  const std::vector<std::uint8_t> received{0U, 0U, 0U, 0U, 0U, 0U};
  const auto expected = exhaustive_viterbi_oracle(2U, 0b10U, 0b10U, received);
  const auto first = code.decode_hard(received);
  const auto second = code.decode_hard(received);
  REQUIRE_EQ(first, second);
  REQUIRE_EQ(first.hamming_distance, expected.first);
  REQUIRE_EQ(first.message_bits, expected.second);
  REQUIRE_EQ(first.final_state,
             reference_viterbi_final_state(2U, first.message_bits));
}

TEST_CASE(convolutional_viterbi_corruption_matches_exhaustive_ml) {
  const RateHalfConvolutionalCode code(3U, 0b111U, 0b101U);
  const std::vector<std::uint8_t> message{1U, 1U, 0U, 1U, 0U, 1U, 1U, 0U};
  auto received = reference_viterbi_encode(3U, 0b111U, 0b101U, message);
  received[3U] ^= 1U;
  received[10U] ^= 1U;

  const auto oracle = exhaustive_viterbi_oracle(3U, 0b111U, 0b101U, received);
  const auto decoded = code.decode_hard(received);
  REQUIRE_EQ(decoded.hamming_distance, oracle.first);
  REQUIRE_EQ(decoded.message_bits, oracle.second);
  REQUIRE_EQ(decoded.message_bits, message);
  REQUIRE_EQ(decoded.corrected_codeword,
             reference_viterbi_encode(3U, 0b111U, 0b101U, decoded.message_bits));
  REQUIRE_EQ(hamming_distance(decoded.corrected_codeword, received),
             decoded.hamming_distance);
}

TEST_CASE(convolutional_viterbi_randomized_exhaustive_differential) {
  std::mt19937_64 random(0x51A7EULL);
  for (std::size_t trial = 0U; trial < 900U; ++trial) {
    const std::size_t constraint_length = 2U + static_cast<std::size_t>(random() % 4U);
    const std::uint32_t mask =
        (std::uint32_t{1} << static_cast<unsigned>(constraint_length)) - 1U;
    const std::uint32_t first_generator =
        1U + static_cast<std::uint32_t>(random() % mask);
    const std::uint32_t second_generator =
        1U + static_cast<std::uint32_t>(random() % mask);
    const RateHalfConvolutionalCode code(
        constraint_length, first_generator, second_generator);

    const std::size_t message_length = static_cast<std::size_t>(random() % 10U);
    std::vector<std::uint8_t> reference_message(message_length, 0U);
    for (auto& bit : reference_message) {
      bit = static_cast<std::uint8_t>(random() & 1U);
    }
    REQUIRE_EQ(code.encode(reference_message),
               reference_viterbi_encode(constraint_length, first_generator,
                                        second_generator, reference_message));

    std::vector<std::uint8_t> received(2U * message_length, 0U);
    for (auto& bit : received) {
      bit = static_cast<std::uint8_t>(random() & 1U);
    }

    const auto oracle = exhaustive_viterbi_oracle(
        constraint_length, first_generator, second_generator, received);
    const auto decoded = code.decode_hard(received);
    REQUIRE_EQ(decoded.hamming_distance, oracle.first);
    REQUIRE_EQ(decoded.message_bits, oracle.second);
    REQUIRE_EQ(decoded.corrected_codeword,
               reference_viterbi_encode(constraint_length, first_generator,
                                        second_generator, decoded.message_bits));
    REQUIRE_EQ(decoded.final_state,
               reference_viterbi_final_state(constraint_length,
                                             decoded.message_bits));
    REQUIRE_EQ(hamming_distance(decoded.corrected_codeword, received),
               decoded.hamming_distance);
  }
}

}  // namespace
