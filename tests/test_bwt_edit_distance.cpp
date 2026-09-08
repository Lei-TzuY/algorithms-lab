#include "algorithms/strings/bwt_edit_distance.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {

std::size_t direct_levenshtein_distance(std::string_view left,
                                        std::string_view right) {
  std::vector<std::size_t> previous(right.size() + 1U);
  std::vector<std::size_t> current(right.size() + 1U);
  for (std::size_t index = 0U; index <= right.size(); ++index) {
    previous[index] = index;
  }
  for (std::size_t row = 1U; row <= left.size(); ++row) {
    current[0U] = row;
    for (std::size_t column = 1U; column <= right.size(); ++column) {
      const bool equal =
          static_cast<unsigned char>(left[row - 1U]) ==
          static_cast<unsigned char>(right[column - 1U]);
      const std::size_t substitution =
          previous[column - 1U] + (equal ? 0U : 1U);
      const std::size_t deletion = previous[column] + 1U;
      const std::size_t insertion = current[column - 1U] + 1U;
      current[column] = std::min({substitution, deletion, insertion});
    }
    previous.swap(current);
  }
  return previous[right.size()];
}

std::vector<std::size_t> direct_edit_distance_positions(
    std::string_view text, std::string_view pattern, std::size_t budget) {
  std::vector<std::size_t> positions;
  if (pattern.empty() || budget >= pattern.size()) {
    positions.reserve(text.size() + 1U);
    for (std::size_t position = 0U; position <= text.size(); ++position) {
      positions.push_back(position);
    }
    return positions;
  }

  const std::size_t minimum_length = pattern.size() - budget;
  const std::size_t maximum_length = pattern.size() + budget;
  for (std::size_t start = 0U; start <= text.size(); ++start) {
    const std::size_t remaining = text.size() - start;
    if (remaining < minimum_length) {
      continue;
    }
    const std::size_t last_length = std::min(maximum_length, remaining);
    bool matched = false;
    for (std::size_t length = minimum_length; length <= last_length; ++length) {
      if (direct_levenshtein_distance(pattern,
                                      text.substr(start, length)) <= budget) {
        matched = true;
        break;
      }
    }
    if (matched) {
      positions.push_back(start);
    }
  }
  return positions;
}

void verify_diagnostics(
    const algorithms::strings::EditDistanceBwtSearchResult& result) {
  REQUIRE(result.pruned_empty_transitions <= result.transitions_considered);
  REQUIRE(result.dominated_states <= result.transitions_considered);
  REQUIRE(result.terminal_states <= result.expanded_states + 1U);
}

}  // namespace

TEST_CASE(bwt_edit_distance_substitution_insertion_and_deletion) {
  using algorithms::strings::BidirectionalBwtByteIndex;
  using algorithms::strings::locate_bwt_edit_distance;

  const BidirectionalBwtByteIndex substitution_index{"axc"};
  const auto substitution =
      locate_bwt_edit_distance(substitution_index, "abc", 1U);
  REQUIRE_EQ(substitution.positions,
             direct_edit_distance_positions("axc", "abc", 1U));
  REQUIRE_EQ(substitution.positions,
             (std::vector<std::size_t>{std::size_t{0}}));

  const BidirectionalBwtByteIndex insertion_index{"Xabc"};
  const auto insertion = locate_bwt_edit_distance(insertion_index, "abc", 1U);
  REQUIRE_EQ(insertion.positions,
             direct_edit_distance_positions("Xabc", "abc", 1U));
  REQUIRE(std::find(insertion.positions.begin(), insertion.positions.end(), 0U) !=
          insertion.positions.end());

  const BidirectionalBwtByteIndex deletion_index{"acd"};
  const auto deletion =
      locate_bwt_edit_distance(deletion_index, "abcd", 1U);
  REQUIRE_EQ(deletion.positions,
             (std::vector<std::size_t>{std::size_t{0}}));

  verify_diagnostics(substitution);
  verify_diagnostics(insertion);
  verify_diagnostics(deletion);
}

TEST_CASE(bwt_edit_distance_zero_budget_and_fast_boundaries) {
  using algorithms::strings::BidirectionalBwtByteIndex;
  using algorithms::strings::locate_bwt_edit_distance;

  const BidirectionalBwtByteIndex index{"banana"};
  const auto exact = locate_bwt_edit_distance(index, "ana", 0U);
  REQUIRE_EQ(exact.positions, index.locate_hamming("ana", 0U).positions);

  const auto empty = locate_bwt_edit_distance(index, std::string_view{}, 0U);
  REQUIRE_EQ(empty.positions,
             (std::vector<std::size_t>{0U, 1U, 2U, 3U, 4U, 5U, 6U}));
  REQUIRE_EQ(empty.expanded_states, std::size_t{0});
  REQUIRE_EQ(empty.terminal_states, std::size_t{1});

  const auto delete_all = locate_bwt_edit_distance(index, "toolong", 7U);
  REQUIRE_EQ(delete_all.positions, empty.positions);
  REQUIRE_EQ(delete_all.transitions_considered, std::size_t{0});
}

TEST_CASE(bwt_edit_distance_arbitrary_bytes_and_deterministic_replay) {
  const std::string text{"\0\xff\x01\0\xfe", 5U};
  const std::string pattern{"\0\xfe\x01", 3U};
  const algorithms::strings::BidirectionalBwtByteIndex index{text};

  const auto first =
      algorithms::strings::locate_bwt_edit_distance(index, pattern, 1U);
  const auto replay =
      algorithms::strings::locate_bwt_edit_distance(index, pattern, 1U);
  REQUIRE_EQ(first.positions,
             direct_edit_distance_positions(text, pattern, 1U));
  REQUIRE_EQ(replay.positions, first.positions);
  REQUIRE_EQ(replay.expanded_states, first.expanded_states);
  REQUIRE_EQ(replay.transitions_considered, first.transitions_considered);
  REQUIRE_EQ(replay.pruned_empty_transitions,
             first.pruned_empty_transitions);
  REQUIRE_EQ(replay.dominated_states, first.dominated_states);
  REQUIRE_EQ(replay.terminal_states, first.terminal_states);
  REQUIRE_EQ(replay.peak_frontier_size, first.peak_frontier_size);
}

TEST_CASE(bwt_edit_distance_randomized_direct_dp_differential) {
  std::mt19937_64 rng(0xED17B27ULL);
  std::uniform_int_distribution<std::size_t> text_length_dist(0U, 7U);
  std::uniform_int_distribution<std::size_t> pattern_length_dist(0U, 5U);
  std::uniform_int_distribution<std::size_t> budget_dist(0U, 1U);
  std::uniform_int_distribution<unsigned int> byte_dist(0U, 255U);

  for (std::size_t trial = 0U; trial < 120U; ++trial) {
    std::string text(text_length_dist(rng), '\0');
    std::string pattern(pattern_length_dist(rng), '\0');
    for (char& value : text) {
      value = static_cast<char>(static_cast<unsigned char>(byte_dist(rng)));
    }
    for (char& value : pattern) {
      value = static_cast<char>(static_cast<unsigned char>(byte_dist(rng)));
    }

    // Regularly create a nearby instance so substitution/insertion/deletion
    // branches survive instead of every random byte transition pruning at once.
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
    const auto actual =
        algorithms::strings::locate_bwt_edit_distance(index, pattern, budget);
    REQUIRE_EQ(actual.positions,
               direct_edit_distance_positions(text, pattern, budget));
    verify_diagnostics(actual);
  }
}
