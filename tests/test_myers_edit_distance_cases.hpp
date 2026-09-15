#pragma once

#include "algorithms/strings/myers_edit_distance.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

std::vector<std::size_t> myers_oracle_final_column(std::string_view pattern,
                                                   std::string_view text) {
  std::vector<std::size_t> previous(pattern.size() + 1U);
  std::vector<std::size_t> current(pattern.size() + 1U);
  for (std::size_t row = 0; row <= pattern.size(); ++row) {
    previous[row] = row;
  }

  for (std::size_t column = 0; column < text.size(); ++column) {
    current[0] = column + 1U;
    for (std::size_t row = 1; row <= pattern.size(); ++row) {
      const std::size_t substitution =
          previous[row - 1U] +
          (pattern[row - 1U] == text[column] ? 0U : 1U);
      const std::size_t deletion = current[row - 1U] + 1U;
      const std::size_t insertion = previous[row] + 1U;
      current[row] = std::min({substitution, deletion, insertion});
    }
    previous.swap(current);
  }
  return previous;
}

void require_myers_vertical_state(
    const algorithms::strings::MyersEditDistance64Result& result,
    const std::vector<std::size_t>& final_column) {
  REQUIRE(!final_column.empty());
  const std::size_t pattern_size = final_column.size() - 1U;
  for (std::size_t index = 0; index < pattern_size; ++index) {
    const std::uint64_t bit = std::uint64_t{1} << index;
    const bool positive = (result.positive_vertical & bit) != 0U;
    const bool negative = (result.negative_vertical & bit) != 0U;
    REQUIRE(!(positive && negative));

    if (final_column[index + 1U] == final_column[index] + 1U) {
      REQUIRE(positive);
      REQUIRE(!negative);
    } else if (final_column[index] == final_column[index + 1U] + 1U) {
      REQUIRE(!positive);
      REQUIRE(negative);
    } else {
      REQUIRE_EQ(final_column[index + 1U], final_column[index]);
      REQUIRE(!positive);
      REQUIRE(!negative);
    }
  }
}

std::string myers_random_bytes(std::mt19937_64& rng, std::size_t length) {
  std::string result(length, '\0');
  for (char& value : result) {
    value = static_cast<char>(static_cast<unsigned char>(rng() & 0xffU));
  }
  return result;
}

}  // namespace

TEST_CASE(myers_edit_distance_contract_and_known_cases) {
  using algorithms::strings::myers_levenshtein_distance_64;

  REQUIRE_EQ(myers_levenshtein_distance_64("", "abc").distance, 3U);
  REQUIRE_EQ(myers_levenshtein_distance_64("kitten", "sitting").distance, 3U);
  REQUIRE_EQ(myers_levenshtein_distance_64("flaw", "lawn").distance, 2U);
  REQUIRE_EQ(myers_levenshtein_distance_64("same", "same").distance, 0U);

  std::string too_long(65U, 'x');
  REQUIRE_THROWS_AS(myers_levenshtein_distance_64(too_long, "x"),
                    std::length_error);
}

TEST_CASE(myers_edit_distance_arbitrary_bytes_and_full_word_boundary) {
  using algorithms::strings::myers_levenshtein_distance_64;

  const std::string pattern{static_cast<char>(0x00), static_cast<char>(0x80),
                            static_cast<char>(0xff), 'A'};
  const std::string text{static_cast<char>(0x00), static_cast<char>(0x81),
                         static_cast<char>(0xff), 'A', static_cast<char>(0x00)};
  const auto result = myers_levenshtein_distance_64(pattern, text);
  const auto oracle = myers_oracle_final_column(pattern, text);
  REQUIRE_EQ(result.distance, oracle.back());
  require_myers_vertical_state(result, oracle);

  std::string word(64U, 'a');
  std::string changed = word;
  changed[0] = 'b';
  changed[63] = 'c';
  const auto full_word = myers_levenshtein_distance_64(word, changed);
  REQUIRE_EQ(full_word.distance, 2U);
  require_myers_vertical_state(full_word,
                               myers_oracle_final_column(word, changed));
}

TEST_CASE(myers_edit_distance_vertical_state_known_columns) {
  using algorithms::strings::myers_levenshtein_distance_64;

  for (const auto& pair : std::vector<std::pair<std::string, std::string>>{
           {"abc", ""}, {"abc", "a"}, {"abc", "ax"},
           {"aaaa", "baaa"}, {"abcdef", "azced"}}) {
    const auto result = myers_levenshtein_distance_64(pair.first, pair.second);
    const auto oracle = myers_oracle_final_column(pair.first, pair.second);
    REQUIRE_EQ(result.distance, oracle.back());
    require_myers_vertical_state(result, oracle);
  }
}

TEST_CASE(myers_edit_distance_randomized_two_row_differential) {
  using algorithms::strings::myers_levenshtein_distance_64;

  std::mt19937_64 rng(0x4d5952535f3634ULL);
  for (std::size_t trial = 0; trial < 1600U; ++trial) {
    const std::size_t pattern_length = static_cast<std::size_t>(rng() % 65U);
    const std::size_t text_length = static_cast<std::size_t>(rng() % 97U);
    const std::string pattern = myers_random_bytes(rng, pattern_length);
    const std::string text = myers_random_bytes(rng, text_length);

    const auto result = myers_levenshtein_distance_64(pattern, text);
    const auto oracle = myers_oracle_final_column(pattern, text);
    REQUIRE_EQ(result.distance, oracle.back());
    require_myers_vertical_state(result, oracle);

    if (text.size() <= 64U) {
      REQUIRE_EQ(myers_levenshtein_distance_64(text, pattern).distance,
                 result.distance);
    }
  }
}
