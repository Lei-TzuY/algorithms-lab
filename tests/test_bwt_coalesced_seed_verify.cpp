#include "algorithms/strings/bwt_coalesced_seed_verify.hpp"

#include "algorithms/strings/bwt_candidate_local_seed_verify.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
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

void verify_coalesced(std::string_view text, std::string_view pattern,
                      std::size_t max_edits, std::size_t sample_rate) {
  const algorithms::strings::BwtByteIndex index{text, sample_rate};
  const auto coalesced =
      algorithms::strings::locate_bwt_coalesced_seed_verify_edit_distance(
          index, pattern, max_edits);
  const auto phase32 =
      algorithms::strings::locate_bwt_candidate_local_seed_verify_edit_distance(
          index, pattern, max_edits);
  const auto direct = direct_substring_oracle(text, pattern, max_edits);

  REQUIRE_EQ(coalesced.positions, direct);
  REQUIRE_EQ(coalesced.positions, phase32.positions);
  REQUIRE_EQ(coalesced.seed_count, phase32.seed_count);
  REQUIRE_EQ(coalesced.seed_occurrences, phase32.seed_occurrences);
  REQUIRE_EQ(coalesced.unique_candidates, phase32.unique_candidates);
  REQUIRE_EQ(coalesced.verified_candidates, phase32.verified_candidates);
  REQUIRE_EQ(coalesced.verification_dp_cells, phase32.verification_dp_cells);
  REQUIRE_EQ(coalesced.candidate_windows, coalesced.verified_candidates);
  REQUIRE(coalesced.merged_extraction_windows <= coalesced.candidate_windows);
  REQUIRE(coalesced.extracted_bytes <= phase32.extracted_bytes);

  if (pattern.empty() || max_edits >= pattern.size() ||
      coalesced.candidate_windows == 0U) {
    REQUIRE_EQ(coalesced.merged_extraction_windows, 0U);
    REQUIRE_EQ(coalesced.extracted_bytes, 0U);
    REQUIRE_EQ(coalesced.extraction_lf_steps, 0U);
    return;
  }

  REQUIRE(coalesced.merged_extraction_windows != 0U);
  REQUIRE(coalesced.extracted_bytes != 0U);
  REQUIRE(coalesced.extraction_lf_steps >= coalesced.extracted_bytes);
  const std::size_t slack_per_window = sample_rate - 1U;
  if (slack_per_window == 0U) {
    REQUIRE_EQ(coalesced.extraction_lf_steps, coalesced.extracted_bytes);
  } else {
    REQUIRE(coalesced.merged_extraction_windows <=
            (std::numeric_limits<std::size_t>::max() -
             coalesced.extracted_bytes) /
                slack_per_window);
    REQUIRE(coalesced.extraction_lf_steps <=
            coalesced.extracted_bytes +
                coalesced.merged_extraction_windows * slack_per_window);
  }
}

}  // namespace

TEST_CASE(bwt_coalesced_seed_verify_basic_bytes_fast_paths_and_sparse_case) {
  verify_coalesced("", "", 0U, 1U);
  verify_coalesced("banana", "banana", 0U, 3U);
  verify_coalesced("abcdef", "abqdef", 1U, 5U);
  verify_coalesced("abcdef", "abXcdef", 1U, 4U);
  verify_coalesced("bcdef", "abcdef", 1U, 2U);
  verify_coalesced("abc", "abcdef", 6U, 3U);

  const std::string text{"\0\x80\xff\0\x81\xff", 6};
  const std::string pattern{"\0\x82\xff", 3};
  verify_coalesced(text, pattern, 1U, 3U);

  const algorithms::strings::BwtByteIndex sparse{"abcdefghijXYZ123klmnop", 7U};
  const auto coalesced =
      algorithms::strings::locate_bwt_coalesced_seed_verify_edit_distance(
          sparse, "XYZ123", 0U);
  const auto phase32 =
      algorithms::strings::locate_bwt_candidate_local_seed_verify_edit_distance(
          sparse, "XYZ123", 0U);
  REQUIRE_EQ(coalesced.positions, phase32.positions);
  REQUIRE_EQ(coalesced.candidate_windows, 1U);
  REQUIRE_EQ(coalesced.merged_extraction_windows, 1U);
  REQUIRE_EQ(coalesced.extracted_bytes, phase32.extracted_bytes);
  REQUIRE_EQ(coalesced.extraction_lf_steps, phase32.extraction_lf_steps);
}

TEST_CASE(bwt_coalesced_seed_verify_randomized_three_way_differential) {
  std::mt19937_64 rng(0x33C0A1E5ULL);
  std::uniform_int_distribution<std::size_t> text_size(0U, 36U);
  std::uniform_int_distribution<std::size_t> pattern_size(0U, 12U);
  std::uniform_int_distribution<std::size_t> budget(0U, 4U);

  for (std::size_t trial = 0U; trial < 1200U; ++trial) {
    const std::string text = random_bytes(rng, text_size(rng), 5U);
    const std::string pattern = random_bytes(rng, pattern_size(rng), 5U);
    verify_coalesced(text, pattern, budget(rng), 1U + trial % 23U);
  }
}

TEST_CASE(bwt_coalesced_seed_verify_varied_sample_rates) {
  const std::string text = "abracadabra abracadabra";
  const std::string pattern = "abrXcadabra";
  for (std::size_t sample_rate = 1U; sample_rate <= 31U; ++sample_rate) {
    verify_coalesced(text, pattern, 1U, sample_rate);
  }
}

TEST_CASE(bwt_coalesced_seed_verify_clustered_candidates_reduce_measured_work) {
  const std::string text(256U, 'a');
  const std::string pattern = "aaaaab";
  const algorithms::strings::BwtByteIndex index{text, 17U};

  const auto coalesced =
      algorithms::strings::locate_bwt_coalesced_seed_verify_edit_distance(
          index, pattern, 1U);
  const auto phase32 =
      algorithms::strings::locate_bwt_candidate_local_seed_verify_edit_distance(
          index, pattern, 1U);
  const auto direct = direct_substring_oracle(text, pattern, 1U);

  REQUIRE_EQ(coalesced.positions, direct);
  REQUIRE_EQ(coalesced.positions, phase32.positions);
  REQUIRE(coalesced.candidate_windows > 1U);
  REQUIRE_EQ(coalesced.merged_extraction_windows, 1U);
  REQUIRE(coalesced.merged_extraction_windows < phase32.extraction_windows);
  REQUIRE(coalesced.extracted_bytes < phase32.extracted_bytes);
  REQUIRE(coalesced.extraction_lf_steps < phase32.extraction_lf_steps);
}
