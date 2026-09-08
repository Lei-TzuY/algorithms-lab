#include "algorithms/strings/bwt_affine_gap.hpp"

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

using algorithms::strings::AffineGapCosts;
using algorithms::strings::BidirectionalBwtByteIndex;
using algorithms::strings::locate_bwt_affine_gap;
using algorithms::strings::locate_bwt_edit_distance;

std::size_t affine_distance(std::string_view pattern, std::string_view text,
                            AffineGapCosts costs) {
  constexpr std::size_t kInfinity = 1000000000U;
  const std::size_t rows = pattern.size() + 1U;
  const std::size_t columns = text.size() + 1U;
  std::vector<std::size_t> match(rows * columns, kInfinity);
  std::vector<std::size_t> insertion(rows * columns, kInfinity);
  std::vector<std::size_t> deletion(rows * columns, kInfinity);
  const auto at = [columns](std::size_t row, std::size_t column) {
    return row * columns + column;
  };

  match[at(0U, 0U)] = 0U;
  for (std::size_t row = 1U; row <= pattern.size(); ++row) {
    deletion[at(row, 0U)] =
        costs.gap_open + (row - 1U) * costs.gap_extend;
  }
  for (std::size_t column = 1U; column <= text.size(); ++column) {
    insertion[at(0U, column)] =
        costs.gap_open + (column - 1U) * costs.gap_extend;
  }

  for (std::size_t row = 0U; row <= pattern.size(); ++row) {
    for (std::size_t column = 0U; column <= text.size(); ++column) {
      if (row > 0U && column > 0U) {
        const std::size_t previous =
            std::min({match[at(row - 1U, column - 1U)],
                      insertion[at(row - 1U, column - 1U)],
                      deletion[at(row - 1U, column - 1U)]});
        const auto left =
            static_cast<unsigned char>(pattern[row - 1U]);
        const auto right = static_cast<unsigned char>(text[column - 1U]);
        match[at(row, column)] =
            previous + (left == right ? 0U : costs.substitution);
      }
      if (column > 0U) {
        insertion[at(row, column)] =
            std::min({match[at(row, column - 1U)] + costs.gap_open,
                      deletion[at(row, column - 1U)] + costs.gap_open,
                      insertion[at(row, column - 1U)] + costs.gap_extend});
      }
      if (row > 0U) {
        deletion[at(row, column)] =
            std::min({match[at(row - 1U, column)] + costs.gap_open,
                      insertion[at(row - 1U, column)] + costs.gap_open,
                      deletion[at(row - 1U, column)] + costs.gap_extend});
      }
    }
  }

  const std::size_t end = at(pattern.size(), text.size());
  return std::min({match[end], insertion[end], deletion[end]});
}

std::vector<std::size_t> oracle_positions(std::string_view text,
                                          std::string_view pattern,
                                          std::size_t max_score,
                                          AffineGapCosts costs) {
  std::vector<std::size_t> result;
  for (std::size_t start = 0U; start <= text.size(); ++start) {
    for (std::size_t length = 0U; length <= text.size() - start; ++length) {
      if (affine_distance(pattern, text.substr(start, length), costs) <=
          max_score) {
        result.push_back(start);
        break;
      }
    }
  }
  return result;
}

void require_same_diagnostics(
    const algorithms::strings::AffineGapBwtSearchResult& left,
    const algorithms::strings::AffineGapBwtSearchResult& right) {
  REQUIRE_EQ(left.expanded_states, right.expanded_states);
  REQUIRE_EQ(left.transitions_considered, right.transitions_considered);
  REQUIRE_EQ(left.pruned_empty_transitions, right.pruned_empty_transitions);
  REQUIRE_EQ(left.pruned_over_budget_transitions,
             right.pruned_over_budget_transitions);
  REQUIRE_EQ(left.dominated_states, right.dominated_states);
  REQUIRE_EQ(left.terminal_states, right.terminal_states);
  REQUIRE_EQ(left.peak_frontier_size, right.peak_frontier_size);
}

TEST_CASE(bwt_affine_gap_empty_and_full_deletion_boundaries) {
  const BidirectionalBwtByteIndex index("abc");
  const AffineGapCosts costs{3U, 4U, 2U};
  REQUIRE_EQ(locate_bwt_affine_gap(index, "", 0U, costs).positions,
             std::vector<std::size_t>({0U, 1U, 2U, 3U}));
  REQUIRE_EQ(locate_bwt_affine_gap(index, "xyz", 8U, costs).positions,
             std::vector<std::size_t>({0U, 1U, 2U, 3U}));
}

TEST_CASE(bwt_affine_gap_insertion_deletion_and_gap_switch) {
  const AffineGapCosts run_costs{9U, 2U, 1U};
  {
    const std::string text = "axxxbbb";
    const std::string pattern = "abbb";
    const BidirectionalBwtByteIndex index(text);
    const auto at_four =
        locate_bwt_affine_gap(index, pattern, 4U, run_costs).positions;
    REQUIRE_EQ(at_four, oracle_positions(text, pattern, 4U, run_costs));
    REQUIRE(std::find(at_four.begin(), at_four.end(), 0U) != at_four.end());
    const auto at_three =
        locate_bwt_affine_gap(index, pattern, 3U, run_costs).positions;
    REQUIRE(std::find(at_three.begin(), at_three.end(), 0U) == at_three.end());
  }
  {
    const std::string text = "abbb";
    const std::string pattern = "axxxbbb";
    const BidirectionalBwtByteIndex index(text);
    const auto actual =
        locate_bwt_affine_gap(index, pattern, 4U, run_costs).positions;
    REQUIRE_EQ(actual, oracle_positions(text, pattern, 4U, run_costs));
    REQUIRE(std::find(actual.begin(), actual.end(), 0U) != actual.end());
  }

  const std::string switch_text = "abXde";
  const std::string switch_pattern = "abcde";
  const AffineGapCosts switch_costs{9U, 2U, 3U};
  const BidirectionalBwtByteIndex switch_index(switch_text);
  const auto at_four = locate_bwt_affine_gap(
      switch_index, switch_pattern, 4U, switch_costs).positions;
  REQUIRE_EQ(at_four,
             oracle_positions(switch_text, switch_pattern, 4U, switch_costs));
  REQUIRE(std::find(at_four.begin(), at_four.end(), 0U) != at_four.end());
  const auto at_three = locate_bwt_affine_gap(
      switch_index, switch_pattern, 3U, switch_costs).positions;
  REQUIRE_EQ(at_three,
             oracle_positions(switch_text, switch_pattern, 3U, switch_costs));
  REQUIRE(std::find(at_three.begin(), at_three.end(), 0U) == at_three.end());
}

TEST_CASE(bwt_affine_gap_arbitrary_bytes_and_diagnostics_replay) {
  std::string text;
  text.push_back(static_cast<char>(0x00));
  text.push_back(static_cast<char>(0xFF));
  text.push_back('x');
  std::string pattern;
  pattern.push_back(static_cast<char>(0x00));
  pattern.push_back(static_cast<char>(0xFE));

  const BidirectionalBwtByteIndex index(text);
  const AffineGapCosts costs{1U, 3U, 1U};
  const auto first = locate_bwt_affine_gap(index, pattern, 1U, costs);
  const auto second = locate_bwt_affine_gap(index, pattern, 1U, costs);
  REQUIRE_EQ(first.positions, second.positions);
  require_same_diagnostics(first, second);
  REQUIRE_EQ(first.positions, oracle_positions(text, pattern, 1U, costs));
}

TEST_CASE(bwt_affine_gap_validation_and_size_t_boundary) {
  const BidirectionalBwtByteIndex index("b");
  REQUIRE_THROWS_AS(
      locate_bwt_affine_gap(index, "a", 1U, AffineGapCosts{0U, 1U, 1U}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      locate_bwt_affine_gap(index, "a", 1U, AffineGapCosts{1U, 0U, 1U}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      locate_bwt_affine_gap(index, "a", 1U, AffineGapCosts{1U, 1U, 0U}),
      std::invalid_argument);

  const std::size_t maximum = std::numeric_limits<std::size_t>::max();
  REQUIRE_EQ(locate_bwt_affine_gap(
                 index, "a", maximum,
                 AffineGapCosts{maximum, maximum, maximum})
                 .positions,
             std::vector<std::size_t>({0U, 1U}));
}

TEST_CASE(bwt_affine_gap_unit_cost_equals_sealed_edit_distance) {
  std::mt19937_64 rng(0xAFF1E280ULL);
  for (std::size_t trial = 0U; trial < 80U; ++trial) {
    const std::size_t text_size = static_cast<std::size_t>(rng() % 7U);
    const std::size_t pattern_size = static_cast<std::size_t>(rng() % 5U);
    std::string text(text_size, 'a');
    std::string pattern(pattern_size, 'a');
    for (char& value : text) {
      value = static_cast<char>('a' + static_cast<char>(rng() % 3U));
    }
    for (char& value : pattern) {
      value = static_cast<char>('a' + static_cast<char>(rng() % 3U));
    }
    const std::size_t budget = static_cast<std::size_t>(rng() % 4U);
    const BidirectionalBwtByteIndex index(text);
    REQUIRE_EQ(locate_bwt_affine_gap(index, pattern, budget,
                                     AffineGapCosts{1U, 1U, 1U})
                   .positions,
               locate_bwt_edit_distance(index, pattern, budget).positions);
  }
}

TEST_CASE(bwt_affine_gap_randomized_independent_dp_differential) {
  std::mt19937_64 rng(0xAFFF1E28ULL);
  for (std::size_t trial = 0U; trial < 120U; ++trial) {
    const std::size_t text_size = static_cast<std::size_t>(rng() % 7U);
    const std::size_t pattern_size = static_cast<std::size_t>(rng() % 5U);
    std::string text(text_size, 'a');
    std::string pattern(pattern_size, 'a');
    for (char& value : text) {
      value = static_cast<char>('a' + static_cast<char>(rng() % 3U));
    }
    for (char& value : pattern) {
      value = static_cast<char>('a' + static_cast<char>(rng() % 3U));
    }
    const AffineGapCosts costs{
        1U + static_cast<std::size_t>(rng() % 4U),
        1U + static_cast<std::size_t>(rng() % 4U),
        1U + static_cast<std::size_t>(rng() % 4U)};
    const std::size_t budget = static_cast<std::size_t>(rng() % 6U);
    const BidirectionalBwtByteIndex index(text);
    REQUIRE_EQ(locate_bwt_affine_gap(index, pattern, budget, costs).positions,
               oracle_positions(text, pattern, budget, costs));
  }
}

}  // namespace
