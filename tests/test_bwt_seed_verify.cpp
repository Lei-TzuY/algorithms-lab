#include "algorithms/strings/bwt_seed_verify.hpp"

#include "algorithms/strings/bwt_edit_distance.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::size_t direct_edit_distance(std::string_view pattern,
                                 std::string_view text) {
  std::vector<std::size_t> previous(text.size() + 1U);
  std::vector<std::size_t> current(text.size() + 1U);
  for (std::size_t column = 0U; column <= text.size(); ++column) {
    previous[column] = column;
  }
  for (std::size_t row = 1U; row <= pattern.size(); ++row) {
    current[0] = row;
    for (std::size_t column = 1U; column <= text.size(); ++column) {
      const std::size_t substitution =
          previous[column - 1U] +
          (pattern[row - 1U] == text[column - 1U] ? 0U : 1U);
      current[column] = std::min(
          {substitution, previous[column] + 1U, current[column - 1U] + 1U});
    }
    previous.swap(current);
  }
  return previous[text.size()];
}

std::vector<std::size_t> direct_substring_oracle(
    std::string_view text, std::string_view pattern, std::size_t max_edits) {
  std::vector<std::size_t> result;
  if (pattern.empty() || max_edits >= pattern.size()) {
    result.resize(text.size() + 1U);
    for (std::size_t position = 0U; position <= text.size(); ++position) {
      result[position] = position;
    }
    return result;
  }

  const std::size_t min_length = pattern.size() - max_edits;
  std::size_t desired_max = std::numeric_limits<std::size_t>::max();
  if (max_edits <=
      std::numeric_limits<std::size_t>::max() - pattern.size()) {
    desired_max = pattern.size() + max_edits;
  }

  for (std::size_t start = 0U; start <= text.size(); ++start) {
    const std::size_t remaining = text.size() - start;
    if (remaining < min_length) {
      continue;
    }
    const std::size_t max_length = std::min(remaining, desired_max);
    for (std::size_t length = min_length; length <= max_length; ++length) {
      if (direct_edit_distance(pattern, text.substr(start, length)) <=
          max_edits) {
        result.push_back(start);
        break;
      }
    }
  }
  return result;
}

std::string random_bytes(std::mt19937_64& rng, std::size_t size,
                         std::size_t alphabet_size) {
  std::uniform_int_distribution<std::size_t> byte(0U, alphabet_size - 1U);
  std::string result(size, '\0');
  for (char& value : result) {
    value = static_cast<char>(byte(rng));
  }
  return result;
}

void require_diagnostics(const algorithms::strings::SeedVerifyEditDistanceResult& result,
                         std::size_t text_size, std::string_view pattern,
                         std::size_t max_edits) {
  REQUIRE(std::is_sorted(result.positions.begin(), result.positions.end()));
  REQUIRE(std::adjacent_find(result.positions.begin(), result.positions.end()) ==
          result.positions.end());
  REQUIRE(result.verified_candidates <= result.unique_candidates);

  if (pattern.empty() || max_edits >= pattern.size()) {
    REQUIRE_EQ(result.seed_count, 0U);
    REQUIRE_EQ(result.seed_occurrences, 0U);
    REQUIRE_EQ(result.unique_candidates, 0U);
    REQUIRE_EQ(result.verified_candidates, 0U);
    REQUIRE_EQ(result.verification_dp_cells, 0U);
    REQUIRE_EQ(result.reconstruction_lf_steps, 0U);
    return;
  }

  REQUIRE_EQ(result.seed_count, max_edits + 1U);
  REQUIRE_EQ(result.reconstruction_lf_steps,
             result.unique_candidates == 0U ? 0U : text_size);
  if (result.verified_candidates != 0U) {
    REQUIRE(result.verification_dp_cells != 0U);
  }
}

void verify_against_direct(std::string_view text, std::string_view pattern,
                           std::size_t max_edits,
                           std::size_t locate_sample_rate = 7U) {
  const algorithms::strings::BwtByteIndex index{text, locate_sample_rate};
  const auto result =
      algorithms::strings::locate_bwt_seed_verify_edit_distance(
          index, pattern, max_edits);
  REQUIRE_EQ(result.positions,
             direct_substring_oracle(text, pattern, max_edits));
  require_diagnostics(result, text.size(), pattern, max_edits);
}

}  // namespace

TEST_CASE(bwt_seed_verify_fast_paths_and_basic_edits) {
  verify_against_direct("", "", 0U);
  verify_against_direct("banana", "banana", 0U);
  verify_against_direct("abcdef", "abqdef", 1U);   // substitution
  verify_against_direct("abcdef", "abXcdef", 1U); // insertion in pattern
  verify_against_direct("abcdef", "abdef", 1U);    // deletion in pattern
  verify_against_direct("abc", "abcdef", 6U);      // delete pattern -> empty

  const algorithms::strings::BwtByteIndex absent{"abcdef", 3U};
  const auto absent_result =
      algorithms::strings::locate_bwt_seed_verify_edit_distance(
          absent, "ZZZZ", 1U);
  REQUIRE(absent_result.positions.empty());
  REQUIRE_EQ(absent_result.unique_candidates, 0U);
  REQUIRE_EQ(absent_result.reconstruction_lf_steps, 0U);
}

TEST_CASE(bwt_seed_verify_indel_displacement_repetition_and_bytes) {
  // The exact seed hit can lie before its pattern offset after a deletion;
  // candidate generation must retain starts implied by a negative center.
  verify_against_direct("bcdef", "abcdef", 1U, 1U);
  verify_against_direct("aaaaabaaaaa", "aaaac", 1U, 2U);
  verify_against_direct("abababababab", "abXabab", 1U, 5U);

  const std::string text{"\0\x80\xff\0\x81\xff", 6};
  const std::string pattern{"\0\x82\xff", 3};
  verify_against_direct(text, pattern, 1U, 3U);
}

TEST_CASE(bwt_seed_verify_randomized_direct_dp_differential) {
  std::mt19937_64 rng(0x30E017D15ULL);
  std::uniform_int_distribution<std::size_t> text_size(0U, 32U);
  std::uniform_int_distribution<std::size_t> pattern_size(0U, 12U);
  std::uniform_int_distribution<std::size_t> budget(0U, 4U);

  for (std::size_t trial = 0U; trial < 1200U; ++trial) {
    const std::string text = random_bytes(rng, text_size(rng), 5U);
    const std::string pattern = random_bytes(rng, pattern_size(rng), 5U);
    verify_against_direct(text, pattern, budget(rng), 1U + trial % 19U);
  }
}

TEST_CASE(bwt_seed_verify_cross_checks_phase27_state_expansion) {
  std::mt19937_64 rng(0x30C2055ULL);
  std::uniform_int_distribution<std::size_t> text_size(0U, 18U);
  std::uniform_int_distribution<std::size_t> pattern_size(0U, 7U);
  std::uniform_int_distribution<std::size_t> budget(0U, 2U);

  for (std::size_t trial = 0U; trial < 180U; ++trial) {
    const std::string text = random_bytes(rng, text_size(rng), 3U);
    const std::string pattern = random_bytes(rng, pattern_size(rng), 3U);
    const std::size_t max_edits = budget(rng);

    const algorithms::strings::BwtByteIndex index{text, 1U + trial % 11U};
    const auto seeded =
        algorithms::strings::locate_bwt_seed_verify_edit_distance(
            index, pattern, max_edits);

    const algorithms::strings::BidirectionalBwtByteIndex bidirectional{text};
    const auto expanded = algorithms::strings::locate_bwt_edit_distance(
        bidirectional, pattern, max_edits);

    REQUIRE_EQ(seeded.positions, expanded.positions);
    REQUIRE_EQ(seeded.positions,
               direct_substring_oracle(text, pattern, max_edits));
  }
}
