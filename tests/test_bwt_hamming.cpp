#include "algorithms/strings/bwt_index.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {
std::vector<std::size_t> direct_hamming_positions(
    std::string_view text, std::string_view pattern, std::size_t budget) {
  std::vector<std::size_t> result;
  if (pattern.empty()) {
    result.reserve(text.size() + 1U);
    for (std::size_t position = 0U; position <= text.size(); ++position) {
      result.push_back(position);
    }
    return result;
  }
  if (pattern.size() > text.size()) return result;
  const std::size_t last = text.size() - pattern.size();
  for (std::size_t start = 0U; start <= last; ++start) {
    std::size_t mismatches = 0U;
    for (std::size_t offset = 0U; offset < pattern.size(); ++offset) {
      if (static_cast<unsigned char>(text[start + offset]) !=
          static_cast<unsigned char>(pattern[offset])) {
        ++mismatches;
      }
    }
    if (mismatches <= budget) result.push_back(start);
  }
  return result;
}
void verify_diagnostics(
    const algorithms::strings::HammingBwtSearchResult& result,
    bool empty_pattern) {
  REQUIRE(result.pruned_empty_transitions <= result.transitions_considered);
  REQUIRE(result.terminal_states <= result.peak_frontier_size);
  if (empty_pattern) {
    REQUIRE_EQ(result.expanded_states, std::size_t{0});
    REQUIRE_EQ(result.transitions_considered, std::size_t{0});
    REQUIRE_EQ(result.pruned_empty_transitions, std::size_t{0});
    REQUIRE_EQ(result.terminal_states, std::size_t{1});
    REQUIRE_EQ(result.peak_frontier_size, std::size_t{1});
  }
}
}  // namespace

TEST_CASE(bwt_hamming_exact_budget_and_center_out_overlap) {
  const algorithms::strings::BidirectionalBwtByteIndex banana{"banana"};
  const auto exact = banana.locate_hamming("ana", 0U);
  REQUIRE_EQ(exact.positions,
             (std::vector<std::size_t>{std::size_t{1}, std::size_t{3}}));
  REQUIRE_EQ(exact.transitions_considered, exact.expanded_states);
  const auto one_substitution = banana.locate_hamming("banena", 1U);
  REQUIRE_EQ(one_substitution.positions,
             (std::vector<std::size_t>{std::size_t{0}}));

  const algorithms::strings::BidirectionalBwtByteIndex repeated{"aaaaa"};
  const auto overlap = repeated.locate_hamming("aba", 1U);
  REQUIRE_EQ(overlap.positions,
             (std::vector<std::size_t>{std::size_t{0}, std::size_t{1},
                                       std::size_t{2}}));
  const auto replay = repeated.locate_hamming("aba", 1U);
  REQUIRE_EQ(replay.positions, overlap.positions);
  REQUIRE_EQ(replay.expanded_states, overlap.expanded_states);
  REQUIRE_EQ(replay.transitions_considered, overlap.transitions_considered);
  REQUIRE_EQ(replay.pruned_empty_transitions,
             overlap.pruned_empty_transitions);
  REQUIRE_EQ(replay.terminal_states, overlap.terminal_states);
  REQUIRE_EQ(replay.peak_frontier_size, overlap.peak_frontier_size);
}

TEST_CASE(bwt_hamming_empty_length_and_full_budget_boundaries) {
  const algorithms::strings::BidirectionalBwtByteIndex index{"banana"};
  const auto empty_pattern = index.locate_hamming(std::string_view{}, 99U);
  REQUIRE_EQ(empty_pattern.positions,
             (std::vector<std::size_t>{0U, 1U, 2U, 3U, 4U, 5U, 6U}));
  verify_diagnostics(empty_pattern, true);

  const auto every_window = index.locate_hamming("xyz", 999U);
  REQUIRE_EQ(every_window.positions,
             (std::vector<std::size_t>{0U, 1U, 2U, 3U}));

  const auto too_long = index.locate_hamming("bananas", 7U);
  REQUIRE(too_long.positions.empty());
  REQUIRE_EQ(too_long.expanded_states, std::size_t{0});
  REQUIRE_EQ(too_long.transitions_considered, std::size_t{0});
  REQUIRE_EQ(too_long.terminal_states, std::size_t{0});
  REQUIRE_EQ(too_long.peak_frontier_size, std::size_t{1});

  const algorithms::strings::BidirectionalBwtByteIndex empty_index{
      std::string_view{}};
  REQUIRE(empty_index.locate_hamming("x", 1U).positions.empty());
}

TEST_CASE(bwt_hamming_arbitrary_bytes) {
  const std::string text{"\0\xff\x01\0\xff", 5U};
  const std::string pattern{"\0\xfe", 2U};
  const algorithms::strings::BidirectionalBwtByteIndex index{text};
  const auto result = index.locate_hamming(pattern, 1U);
  REQUIRE_EQ(result.positions, direct_hamming_positions(text, pattern, 1U));
}

TEST_CASE(bwt_hamming_randomized_direct_scan_differential) {
  std::mt19937_64 rng(0xA226B17ULL);
  std::uniform_int_distribution<std::size_t> text_length_dist(0U, 10U);
  std::uniform_int_distribution<std::size_t> pattern_length_dist(0U, 5U);
  std::uniform_int_distribution<std::size_t> budget_dist(0U, 2U);
  std::uniform_int_distribution<unsigned int> byte_dist(0U, 255U);

  for (std::size_t trial = 0U; trial < 240U; ++trial) {
    std::string text(text_length_dist(rng), '\0');
    std::string pattern(pattern_length_dist(rng), '\0');
    for (char& value : text) {
      value = static_cast<char>(static_cast<unsigned char>(byte_dist(rng)));
    }
    for (char& value : pattern) {
      value = static_cast<char>(static_cast<unsigned char>(byte_dist(rng)));
    }
    if (!text.empty() && !pattern.empty() && trial % 3U == 0U) {
      const std::size_t copied = std::min(text.size(), pattern.size());
      for (std::size_t index = 0U; index < copied; ++index) {
        pattern[index] = text[index];
      }
      if (copied > 0U && trial % 6U == 0U) {
        pattern[copied / 2U] = static_cast<char>(
            static_cast<unsigned char>(pattern[copied / 2U]) ^ 1U);
      }
    }
    const std::size_t budget = budget_dist(rng);
    const algorithms::strings::BidirectionalBwtByteIndex index{text};
    const auto result = index.locate_hamming(pattern, budget);
    REQUIRE_EQ(result.positions,
               direct_hamming_positions(text, pattern, budget));
    verify_diagnostics(result, pattern.empty());
  }
}
