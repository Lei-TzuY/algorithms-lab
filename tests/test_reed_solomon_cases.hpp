#pragma once

#include "algorithms/coding/reed_solomon.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace reed_solomon_recovery_tests {

[[nodiscard]] inline std::size_t hamming_distance(
    const std::vector<std::uint64_t>& lhs,
    const std::vector<std::uint64_t>& rhs) {
  if (lhs.size() != rhs.size()) {
    throw std::logic_error("Reed-Solomon test oracle shape mismatch");
  }
  std::size_t distance = 0U;
  for (std::size_t index = 0; index < lhs.size(); ++index) {
    if (lhs[index] != rhs[index]) ++distance;
  }
  return distance;
}

[[nodiscard]] inline std::vector<std::uint64_t> naive_small_encode(
    const std::vector<std::uint64_t>& message,
    const std::vector<std::uint64_t>& points, std::uint64_t prime_modulus) {
  if (prime_modulus > 1024U) {
    throw std::logic_error("small Reed-Solomon oracle modulus too large");
  }
  std::vector<std::uint64_t> word;
  word.reserve(points.size());
  for (const auto point_value : points) {
    const std::uint64_t x = point_value % prime_modulus;
    std::uint64_t value = 0U;
    for (auto it = message.rbegin(); it != message.rend(); ++it) {
      value = (value * x + (*it % prime_modulus)) % prime_modulus;
    }
    word.push_back(value);
  }
  return word;
}

[[nodiscard]] inline std::optional<std::vector<std::uint64_t>> brute_decode(
    const std::vector<std::uint64_t>& received,
    const std::vector<std::uint64_t>& points, std::size_t message_length,
    std::size_t max_errors, std::uint64_t prime_modulus) {
  std::size_t message_count = 1U;
  for (std::size_t degree = 0; degree < message_length; ++degree) {
    message_count *= static_cast<std::size_t>(prime_modulus);
  }

  std::optional<std::vector<std::uint64_t>> found;
  std::vector<std::uint64_t> message(message_length, 0U);
  for (std::size_t code = 0; code < message_count; ++code) {
    std::size_t value = code;
    for (std::size_t degree = 0; degree < message_length; ++degree) {
      message[degree] = static_cast<std::uint64_t>(
          value % static_cast<std::size_t>(prime_modulus));
      value /= static_cast<std::size_t>(prime_modulus);
    }
    const auto encoded = naive_small_encode(message, points, prime_modulus);
    if (hamming_distance(encoded, received) <= max_errors) {
      if (found.has_value()) {
        throw std::logic_error("Reed-Solomon unique-decoding balls overlap");
      }
      found = message;
    }
  }
  return found;
}

}  // namespace reed_solomon_recovery_tests

TEST_CASE(reed_solomon_known_errors_and_zero_radius) {
  using algorithms::coding::reed_solomon_decode;
  using algorithms::coding::reed_solomon_encode;
  constexpr std::uint64_t kPrime = 17U;
  const std::vector<std::uint64_t> points{0U, 1U, 2U, 3U, 4U, 5U, 6U};
  const std::vector<std::uint64_t> message{3U, 5U, 2U};
  const auto encoded = reed_solomon_encode(message, points, kPrime);
  REQUIRE_EQ(encoded,
             std::vector<std::uint64_t>({3U, 10U, 4U, 2U, 4U, 10U, 3U}));

  const auto exact = reed_solomon_decode(encoded, points, 3U, 2U, kPrime);
  REQUIRE(exact.has_value());
  REQUIRE_EQ(exact->message_coefficients, message);
  REQUIRE(exact->error_positions.empty());

  auto corrupted = encoded;
  corrupted[1] = (corrupted[1] + 4U) % kPrime;
  corrupted[5] = (corrupted[5] + 7U) % kPrime;
  const auto decoded = reed_solomon_decode(corrupted, points, 3U, 2U, kPrime);
  REQUIRE(decoded.has_value());
  REQUIRE_EQ(decoded->message_coefficients, message);
  REQUIRE_EQ(decoded->corrected_codeword, encoded);
  REQUIRE_EQ(decoded->error_positions, std::vector<std::size_t>({1U, 5U}));

  const std::vector<std::uint64_t> zero_radius_points{0U, 1U, 2U, 3U};
  const std::vector<std::uint64_t> zero_radius_message{4U, 6U};
  const auto zero_radius_word =
      reed_solomon_encode(zero_radius_message, zero_radius_points, kPrime);
  const auto zero_radius = reed_solomon_decode(
      zero_radius_word, zero_radius_points, 2U, 0U, kPrime);
  REQUIRE(zero_radius.has_value());
  REQUIRE_EQ(zero_radius->message_coefficients, zero_radius_message);
}

TEST_CASE(reed_solomon_validation_and_outside_radius_rejection) {
  using algorithms::coding::reed_solomon_decode;
  using algorithms::coding::reed_solomon_encode;
  const std::vector<std::uint64_t> message{1U, 2U};
  REQUIRE_THROWS_AS(reed_solomon_encode(std::vector<std::uint64_t>{},
                                        std::vector<std::uint64_t>{}, 17U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(reed_solomon_encode(
                        message, std::vector<std::uint64_t>{0U}, 17U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(reed_solomon_encode(
                        message, std::vector<std::uint64_t>{0U, 17U}, 17U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(reed_solomon_encode(
                        message, std::vector<std::uint64_t>{0U, 1U}, 15U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(reed_solomon_decode(
                        std::vector<std::uint64_t>{1U},
                        std::vector<std::uint64_t>{0U, 1U}, 1U, 0U, 17U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(reed_solomon_decode(
                        std::vector<std::uint64_t>{1U, 2U},
                        std::vector<std::uint64_t>{0U, 1U}, 0U, 0U, 17U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(reed_solomon_decode(
                        std::vector<std::uint64_t>{1U, 2U, 3U},
                        std::vector<std::uint64_t>{0U, 1U, 2U}, 2U, 1U, 17U),
                    std::invalid_argument);

  const std::vector<std::uint64_t> no_code{0U, 1U, 2U};
  const std::vector<std::uint64_t> tiny_points{0U, 1U, 2U};
  REQUIRE(!reed_solomon_decode(no_code, tiny_points, 1U, 1U, 7U).has_value());
}

TEST_CASE(reed_solomon_randomized_recovery_within_radius) {
  using algorithms::coding::reed_solomon_decode;
  using algorithms::coding::reed_solomon_encode;
  std::mt19937_64 random(0x5EED5010ULL);
  constexpr std::uint64_t kPrime = 257U;
  const std::vector<std::uint64_t> points{1U, 2U, 3U, 4U, 5U,
                                          6U, 7U, 8U, 9U};
  for (int trial = 0; trial < 600; ++trial) {
    std::vector<std::uint64_t> message(5U);
    for (auto& value : message) value = random() % kPrime;
    const auto encoded = reed_solomon_encode(message, points, kPrime);
    auto received = encoded;
    const std::size_t error_count =
        static_cast<std::size_t>(random() % 3U);
    std::vector<std::size_t> positions;
    while (positions.size() < error_count) {
      const auto position =
          static_cast<std::size_t>(random() % received.size());
      if (std::find(positions.begin(), positions.end(), position) ==
          positions.end()) {
        positions.push_back(position);
      }
    }
    std::sort(positions.begin(), positions.end());
    for (const auto position : positions) {
      const std::uint64_t delta = 1U + (random() % (kPrime - 1U));
      received[position] = (received[position] + delta) % kPrime;
    }
    const auto decoded =
        reed_solomon_decode(received, points, 5U, 2U, kPrime);
    REQUIRE(decoded.has_value());
    REQUIRE_EQ(decoded->message_coefficients, message);
    REQUIRE_EQ(decoded->corrected_codeword, encoded);
    REQUIRE_EQ(decoded->error_positions, positions);
  }
}

TEST_CASE(reed_solomon_small_field_exhaustive_unique_decoding_oracle) {
  using algorithms::coding::reed_solomon_decode;
  using reed_solomon_recovery_tests::brute_decode;
  using reed_solomon_recovery_tests::hamming_distance;
  std::mt19937_64 random(0xBEE7CAFEULL);
  constexpr std::uint64_t kPrime = 5U;
  const std::vector<std::uint64_t> points{0U, 1U, 2U, 3U, 4U};
  for (int trial = 0; trial < 500; ++trial) {
    std::vector<std::uint64_t> received(points.size());
    for (auto& value : received) value = random() % kPrime;
    const auto expected = brute_decode(received, points, 3U, 1U, kPrime);
    const auto actual =
        reed_solomon_decode(received, points, 3U, 1U, kPrime);
    REQUIRE_EQ(actual.has_value(), expected.has_value());
    if (expected.has_value()) {
      REQUIRE_EQ(actual->message_coefficients, *expected);
      REQUIRE(hamming_distance(actual->corrected_codeword, received) <= 1U);
    }
  }
}
