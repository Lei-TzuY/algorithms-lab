#pragma once

#include "algorithms/strings/lyndon_factorization.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

using algorithms::strings::LyndonFactor;

[[nodiscard]] unsigned int lyndon_test_byte(std::string_view text,
                                            std::size_t index) {
  return static_cast<unsigned char>(text[index]);
}

[[nodiscard]] int lyndon_compare_ranges(std::string_view text,
                                        LyndonFactor first,
                                        LyndonFactor second) {
  const std::size_t first_length = first.end - first.begin;
  const std::size_t second_length = second.end - second.begin;
  const std::size_t shared = std::min(first_length, second_length);
  for (std::size_t offset = 0U; offset < shared; ++offset) {
    const unsigned int left = lyndon_test_byte(text, first.begin + offset);
    const unsigned int right = lyndon_test_byte(text, second.begin + offset);
    if (left < right) return -1;
    if (left > right) return 1;
  }
  if (first_length < second_length) return -1;
  if (first_length > second_length) return 1;
  return 0;
}

[[nodiscard]] bool lyndon_is_word_by_rotation_definition(
    std::string_view text, LyndonFactor factor) {
  const std::size_t length = factor.end - factor.begin;
  if (length == 0U) return false;
  for (std::size_t shift = 1U; shift < length; ++shift) {
    int comparison = 0;
    for (std::size_t offset = 0U; offset < length; ++offset) {
      const unsigned int left =
          lyndon_test_byte(text, factor.begin + offset);
      const unsigned int right = lyndon_test_byte(
          text, factor.begin + ((offset + shift) % length));
      if (left < right) {
        comparison = -1;
        break;
      }
      if (left > right) {
        comparison = 1;
        break;
      }
    }
    if (comparison >= 0) return false;
  }
  return true;
}

[[nodiscard]] bool lyndon_valid_factorization(
    std::string_view text, const std::vector<LyndonFactor>& factors) {
  if (text.empty()) return factors.empty();
  if (factors.empty()) return false;

  std::size_t next = 0U;
  for (std::size_t index = 0U; index < factors.size(); ++index) {
    const LyndonFactor factor = factors[index];
    if (factor.begin != next || factor.begin >= factor.end ||
        factor.end > text.size()) {
      return false;
    }
    if (!lyndon_is_word_by_rotation_definition(text, factor)) return false;
    if (index > 0U &&
        lyndon_compare_ranges(text, factors[index - 1U], factor) < 0) {
      return false;
    }
    next = factor.end;
  }
  return next == text.size();
}

struct LyndonBruteResult {
  std::vector<LyndonFactor> factorization;
  std::size_t valid_partition_count;
};

[[nodiscard]] LyndonBruteResult lyndon_brute_unique_factorization(
    std::string_view text) {
  if (text.empty()) return LyndonBruteResult{{}, 1U};

  const std::size_t cut_positions = text.size() - 1U;
  REQUIRE(cut_positions < 63U);
  const std::uint64_t partition_count = std::uint64_t{1} << cut_positions;
  LyndonBruteResult result{{}, 0U};

  for (std::uint64_t mask = 0U; mask < partition_count; ++mask) {
    std::vector<LyndonFactor> candidate;
    std::size_t begin = 0U;
    for (std::size_t cut = 0U; cut < cut_positions; ++cut) {
      if ((mask & (std::uint64_t{1} << cut)) != 0U) {
        candidate.push_back(LyndonFactor{begin, cut + 1U});
        begin = cut + 1U;
      }
    }
    candidate.push_back(LyndonFactor{begin, text.size()});
    if (lyndon_valid_factorization(text, candidate)) {
      ++result.valid_partition_count;
      result.factorization = std::move(candidate);
    }
  }
  return result;
}

[[nodiscard]] std::string lyndon_string_from_bytes(
    const std::vector<unsigned int>& bytes) {
  std::string result;
  result.reserve(bytes.size());
  for (const unsigned int byte : bytes) {
    REQUIRE(byte <= 255U);
    result.push_back(static_cast<char>(static_cast<unsigned char>(byte)));
  }
  return result;
}

TEST_CASE(lyndon_factorization_known_decompositions) {
  using algorithms::strings::duval_lyndon_factorization;

  REQUIRE(duval_lyndon_factorization("").empty());
  REQUIRE_EQ(duval_lyndon_factorization("banana"),
             (std::vector<LyndonFactor>{{0U, 1U}, {1U, 3U}, {3U, 5U},
                                        {5U, 6U}}));
  REQUIRE_EQ(duval_lyndon_factorization("aaaa"),
             (std::vector<LyndonFactor>{{0U, 1U}, {1U, 2U}, {2U, 3U},
                                        {3U, 4U}}));
  REQUIRE_EQ(duval_lyndon_factorization("abcd"),
             (std::vector<LyndonFactor>{{0U, 4U}}));
  REQUIRE_EQ(duval_lyndon_factorization("dcba"),
             (std::vector<LyndonFactor>{{0U, 1U}, {1U, 2U}, {2U, 3U},
                                        {3U, 4U}}));
}

TEST_CASE(lyndon_factorization_uses_unsigned_byte_order) {
  using algorithms::strings::duval_lyndon_factorization;
  const std::string text = lyndon_string_from_bytes({255U, 0U, 128U});
  REQUIRE_EQ(duval_lyndon_factorization(text),
             (std::vector<LyndonFactor>{{0U, 1U}, {1U, 3U}}));

  const std::string with_nul =
      lyndon_string_from_bytes({0U, 255U, 0U, 128U, 0U});
  const LyndonBruteResult brute = lyndon_brute_unique_factorization(with_nul);
  REQUIRE_EQ(brute.valid_partition_count, 1U);
  REQUIRE_EQ(duval_lyndon_factorization(with_nul), brute.factorization);
}

TEST_CASE(lyndon_factorization_exhaustive_small_partition_oracle) {
  using algorithms::strings::duval_lyndon_factorization;
  constexpr std::array<unsigned int, 3U> alphabet{0U, 128U, 255U};

  std::size_t checked = 0U;
  std::size_t word_count = 1U;
  for (std::size_t length = 0U; length <= 7U; ++length) {
    if (length > 0U) word_count *= alphabet.size();
    for (std::size_t code = 0U; code < word_count; ++code) {
      std::size_t value = code;
      std::vector<unsigned int> bytes(length, 0U);
      for (std::size_t index = 0U; index < length; ++index) {
        bytes[index] = alphabet[value % alphabet.size()];
        value /= alphabet.size();
      }
      const std::string text = lyndon_string_from_bytes(bytes);
      const LyndonBruteResult brute = lyndon_brute_unique_factorization(text);
      REQUIRE_EQ(brute.valid_partition_count, 1U);
      const auto actual = duval_lyndon_factorization(text);
      REQUIRE_EQ(actual, brute.factorization);
      REQUIRE(lyndon_valid_factorization(text, actual));
      ++checked;
    }
  }
  REQUIRE_EQ(checked, 3280U);
}

TEST_CASE(lyndon_factorization_randomized_arbitrary_byte_replay) {
  using algorithms::strings::duval_lyndon_factorization;
  std::mt19937_64 rng(0x4C594E444F4EULL);

  for (std::size_t trial = 0U; trial < 700U; ++trial) {
    const std::size_t length = static_cast<std::size_t>(rng() % 129U);
    std::string text;
    text.reserve(length);
    for (std::size_t index = 0U; index < length; ++index) {
      const auto byte = static_cast<unsigned char>(rng() & 0xFFU);
      text.push_back(static_cast<char>(byte));
    }
    const auto factors = duval_lyndon_factorization(text);
    REQUIRE(lyndon_valid_factorization(text, factors));
  }
}

}  // namespace
