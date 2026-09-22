#pragma once

#include "algorithms/data_structures/static_range_mode.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace static_range_mode_test_detail {

using algorithms::data_structures::StaticRangeMode;
using algorithms::data_structures::StaticRangeModeResult;

inline std::optional<StaticRangeModeResult> brute_mode(
    const std::vector<std::int64_t>& values,
    const std::size_t begin, const std::size_t end) {
  if (begin == end) {
    return std::nullopt;
  }

  std::map<std::int64_t, std::size_t> counts;
  for (std::size_t index = begin; index < end; ++index) {
    ++counts[values[index]];
  }

  StaticRangeModeResult best{};
  bool have_best = false;
  for (const auto& [value, frequency] : counts) {
    if (!have_best || frequency > best.frequency ||
        (frequency == best.frequency && value < best.value)) {
      best = StaticRangeModeResult{value, frequency};
      have_best = true;
    }
  }
  return best;
}

inline std::size_t brute_frequency(
    const std::vector<std::int64_t>& values,
    const std::int64_t value,
    const std::size_t begin, const std::size_t end) {
  return static_cast<std::size_t>(
      std::count(values.begin() + static_cast<std::ptrdiff_t>(begin),
                 values.begin() + static_cast<std::ptrdiff_t>(end),
                 value));
}

inline void require_all_ranges(
    const std::vector<std::int64_t>& values) {
  const StaticRangeMode index(values);
  for (std::size_t begin = 0U; begin <= values.size(); ++begin) {
    for (std::size_t end = begin; end <= values.size(); ++end) {
      REQUIRE(index.query(begin, end) ==
              brute_mode(values, begin, end));
    }
  }
}

}  // namespace static_range_mode_test_detail

TEST_CASE(static_range_mode_empty_bounds_and_frequency_contract) {
  using namespace static_range_mode_test_detail;

  const std::vector<std::int64_t> empty_values;
  StaticRangeMode empty(empty_values);
  REQUIRE(empty.empty());
  REQUIRE_EQ(empty.size(), 0U);
  REQUIRE_EQ(empty.distinct_value_count(), 0U);
  REQUIRE_EQ(empty.block_count(), 0U);
  REQUIRE(!empty.query(0U, 0U).has_value());
  REQUIRE_EQ(empty.frequency(7, 0U, 0U), 0U);
  REQUIRE_THROWS_AS(empty.query(0U, 1U), std::out_of_range);
  REQUIRE_THROWS_AS(empty.frequency(7, 1U, 0U), std::out_of_range);

  const std::vector<std::int64_t> values{1, 2, 3};
  StaticRangeMode index(values);
  REQUIRE_THROWS_AS(index.query(2U, 1U), std::out_of_range);
  REQUIRE_THROWS_AS(index.query(0U, 4U), std::out_of_range);
  REQUIRE_THROWS_AS(index.frequency(2, 0U, 4U), std::out_of_range);
  REQUIRE_EQ(index.frequency(2, 0U, 3U), 1U);
  REQUIRE_EQ(index.frequency(99, 0U, 3U), 0U);
}

TEST_CASE(static_range_mode_exact_ties_extremes_and_block_boundaries) {
  using namespace static_range_mode_test_detail;

  const std::vector<std::int64_t> values{
      4, 1, 4, 2, 2, 2, 4, 9,
      std::numeric_limits<std::int64_t>::min(),
      std::numeric_limits<std::int64_t>::max(),
      std::numeric_limits<std::int64_t>::min(), 9, 9, 4, 2, 1};

  const StaticRangeMode index(values);
  REQUIRE(index.block_size() >= 1U);
  REQUIRE(index.block_count() >= 1U);
  REQUIRE_EQ(index.distinct_value_count(), 6U);

  REQUIRE(index.query(0U, 7U) ==
          std::optional<StaticRangeModeResult>(
              StaticRangeModeResult{2, 3U}));
  REQUIRE(index.query(0U, 16U) ==
          brute_mode(values, 0U, 16U));
  REQUIRE(index.query(8U, 11U) ==
          std::optional<StaticRangeModeResult>(
              StaticRangeModeResult{
                  std::numeric_limits<std::int64_t>::min(), 2U}));

  require_all_ranges(values);
}

TEST_CASE(static_range_mode_all_unique_uses_smallest_value_tie_break) {
  using namespace static_range_mode_test_detail;

  const std::vector<std::int64_t> values{
      8, -4, 12, 7, 0, 99, -5, 6, 3, 20};
  const StaticRangeMode index(values);

  REQUIRE(index.query(0U, values.size()) ==
          std::optional<StaticRangeModeResult>(
              StaticRangeModeResult{-5, 1U}));
  REQUIRE(index.query(2U, 7U) ==
          std::optional<StaticRangeModeResult>(
              StaticRangeModeResult{-5, 1U}));
  require_all_ranges(values);
}

TEST_CASE(static_range_mode_duplicate_heavy_fringe_can_change_core_mode) {
  using namespace static_range_mode_test_detail;

  std::vector<std::int64_t> values(64U, 2);
  for (std::size_t index = 8U; index < 56U; ++index) {
    values[index] = (index % 3U) == 0U ? 1 : 2;
  }
  for (std::size_t index = 0U; index < 8U; ++index) {
    values[index] = 1;
  }
  for (std::size_t index = 56U; index < 64U; ++index) {
    values[index] = 1;
  }

  const StaticRangeMode index(values);
  for (std::size_t begin = 0U; begin < values.size(); begin += 3U) {
    for (std::size_t end = begin + 1U; end <= values.size(); end += 5U) {
      REQUIRE(index.query(begin, end) ==
              brute_mode(values, begin, end));
    }
  }
}

TEST_CASE(static_range_mode_exhaustive_random_small_arrays) {
  using namespace static_range_mode_test_detail;

  std::mt19937_64 random(0xA4A64D0DEULL);
  for (std::size_t trial = 0U; trial < 320U; ++trial) {
    const std::size_t size =
        static_cast<std::size_t>(random() % 25U);
    std::vector<std::int64_t> values(size);
    for (std::int64_t& value : values) {
      value = static_cast<std::int64_t>(random() % 9U) - 4;
    }
    require_all_ranges(values);
  }
}

TEST_CASE(static_range_mode_large_random_queries_match_direct_scan) {
  using namespace static_range_mode_test_detail;

  std::mt19937_64 random(0xB10C4D0DEULL);
  for (std::size_t trial = 0U; trial < 100U; ++trial) {
    const std::size_t size =
        32U + static_cast<std::size_t>(random() % 225U);
    std::vector<std::int64_t> values(size);
    for (std::int64_t& value : values) {
      if ((random() % 13U) == 0U) {
        value = (random() & 1ULL) == 0ULL
                    ? std::numeric_limits<std::int64_t>::min()
                    : std::numeric_limits<std::int64_t>::max();
      } else {
        value = static_cast<std::int64_t>(random() % 41U) - 20;
      }
    }

    const StaticRangeMode index(values);
    for (std::size_t query = 0U; query < 300U; ++query) {
      std::size_t first =
          static_cast<std::size_t>(random() % (size + 1U));
      std::size_t second =
          static_cast<std::size_t>(random() % (size + 1U));
      if (first > second) {
        std::swap(first, second);
      }

      REQUIRE(index.query(first, second) ==
              brute_mode(values, first, second));

      const std::int64_t probe =
          static_cast<std::int64_t>(random() % 47U) - 23;
      REQUIRE_EQ(index.frequency(probe, first, second),
                 brute_frequency(values, probe, first, second));
    }
  }
}
