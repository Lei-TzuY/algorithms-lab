#pragma once

#include "algorithms/searching/deterministic_selection.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

namespace deterministic_selection_tests {

inline std::int64_t oracle_select(std::vector<std::int64_t> values,
                                  const std::size_t kth_index) {
  std::sort(values.begin(), values.end());
  return values[kth_index];
}

inline void require_all_ranks_match(const std::vector<std::int64_t>& values) {
  const std::vector<std::int64_t> snapshot = values;
  for (std::size_t rank = 0U; rank < values.size(); ++rank) {
    REQUIRE_EQ(algorithms::searching::deterministic_select_kth(values, rank),
               oracle_select(values, rank));
    REQUIRE(values == snapshot);
  }
}

}  // namespace deterministic_selection_tests

TEST_CASE(deterministic_selection_validation_and_boundaries) {
  const std::vector<std::int64_t> empty;
  REQUIRE_THROWS_AS(algorithms::searching::deterministic_select_kth(empty, 0U),
                    std::out_of_range);

  deterministic_selection_tests::require_all_ranks_match({42});
  deterministic_selection_tests::require_all_ranks_match(
      {std::numeric_limits<std::int64_t>::min(), 0,
       std::numeric_limits<std::int64_t>::max(), -1, 1});
  deterministic_selection_tests::require_all_ranks_match({5, 5, 5, 5, 5, 5});
  deterministic_selection_tests::require_all_ranks_match({2, 1});

  const std::vector<std::int64_t> values{3, 1, 2};
  REQUIRE_THROWS_AS(algorithms::searching::deterministic_select_kth(values, 3U),
                    std::out_of_range);
}

TEST_CASE(deterministic_selection_adversarial_shapes) {
  std::vector<std::int64_t> sorted(257U);
  for (std::size_t index = 0U; index < sorted.size(); ++index) {
    sorted[index] = static_cast<std::int64_t>(index) - 128;
  }
  deterministic_selection_tests::require_all_ranks_match(sorted);

  std::vector<std::int64_t> reversed = sorted;
  std::reverse(reversed.begin(), reversed.end());
  deterministic_selection_tests::require_all_ranks_match(reversed);

  std::vector<std::int64_t> duplicate_heavy;
  duplicate_heavy.reserve(301U);
  for (std::size_t index = 0U; index < 301U; ++index) {
    const std::int64_t bucket = static_cast<std::int64_t>((index * 17U) % 9U);
    duplicate_heavy.push_back(bucket - 4);
  }
  deterministic_selection_tests::require_all_ranks_match(duplicate_heavy);

  std::vector<std::int64_t> organ_pipe;
  for (std::int64_t value = 0; value < 80; ++value) organ_pipe.push_back(value);
  for (std::int64_t value = 79; value >= 0; --value) organ_pipe.push_back(value);
  deterministic_selection_tests::require_all_ranks_match(organ_pipe);
}

TEST_CASE(deterministic_selection_randomized_differential) {
  std::mt19937_64 rng(0xBFD0A11ULL);
  std::uniform_int_distribution<std::size_t> length_distribution(1U, 320U);
  std::uniform_int_distribution<std::int64_t> duplicate_distribution(-40, 40);

  for (std::size_t trial = 0U; trial < 1400U; ++trial) {
    const std::size_t length = length_distribution(rng);
    std::vector<std::int64_t> values(length);
    for (auto& value : values) value = duplicate_distribution(rng);
    const std::size_t kth_index = static_cast<std::size_t>(rng() % length);
    const std::vector<std::int64_t> snapshot = values;
    REQUIRE_EQ(algorithms::searching::deterministic_select_kth(values, kth_index),
               deterministic_selection_tests::oracle_select(values, kth_index));
    REQUIRE(values == snapshot);
  }
}

TEST_CASE(deterministic_selection_full_width_randomized_differential) {
  std::mt19937_64 rng(0x5E1EC710ULL);
  std::uniform_int_distribution<std::size_t> length_distribution(1U, 180U);

  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const std::size_t length = length_distribution(rng);
    std::vector<std::int64_t> values(length);
    for (auto& value : values) {
      value = static_cast<std::int64_t>(rng());
    }
    const std::size_t kth_index = static_cast<std::size_t>(rng() % length);
    REQUIRE_EQ(algorithms::searching::deterministic_select_kth(values, kth_index),
               deterministic_selection_tests::oracle_select(values, kth_index));
  }
}
