#pragma once

#include "algorithms/data_structures/veb_layout_static_set.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <random>
#include <vector>

using algorithms::data_structures::VebLayoutStaticSet64;

namespace veb_layout_static_set_test_detail {

inline std::vector<std::int64_t> normalize(
    std::vector<std::int64_t> values) {
  std::sort(values.begin(), values.end());
  values.erase(
      std::unique(values.begin(), values.end()),
      values.end());
  return values;
}

inline std::optional<std::int64_t> oracle_lower_bound(
    const std::vector<std::int64_t>& sorted,
    const std::int64_t value) {
  const auto it =
      std::lower_bound(sorted.begin(), sorted.end(), value);
  if (it == sorted.end()) {
    return std::nullopt;
  }
  return *it;
}

inline std::optional<std::int64_t> oracle_predecessor(
    const std::vector<std::int64_t>& sorted,
    const std::int64_t value) {
  const auto it =
      std::upper_bound(sorted.begin(), sorted.end(), value);
  if (it == sorted.begin()) {
    return std::nullopt;
  }
  return *std::prev(it);
}

inline void require_matches(
    const std::vector<std::int64_t>& input,
    const std::vector<std::int64_t>& probes) {
  const std::vector<std::int64_t> sorted = normalize(input);
  const VebLayoutStaticSet64 set(input);

  REQUIRE_EQ(set.size(), sorted.size());
  REQUIRE(set.empty() == sorted.empty());
  REQUIRE(set.valid_structure());

  for (const std::int64_t probe : probes) {
    REQUIRE(
        set.contains(probe) ==
        std::binary_search(sorted.begin(), sorted.end(), probe));
    REQUIRE(
        set.lower_bound(probe) ==
        oracle_lower_bound(sorted, probe));
    REQUIRE(
        set.predecessor(probe) ==
        oracle_predecessor(sorted, probe));
    REQUIRE_EQ(
        set.rank(probe),
        static_cast<std::size_t>(
            std::lower_bound(
                sorted.begin(), sorted.end(), probe) -
            sorted.begin()));
  }

  for (std::size_t order = 0U; order < sorted.size(); ++order) {
    REQUIRE(set.select(order) ==
            std::optional<std::int64_t>(sorted[order]));
  }
  REQUIRE(!set.select(sorted.size()).has_value());
}

}  // namespace veb_layout_static_set_test_detail

TEST_CASE(veb_layout_static_set_empty_and_duplicate_normalization) {
  using namespace veb_layout_static_set_test_detail;

  require_matches({}, {-1, 0, 1});
  require_matches(
      {5, 1, 5, 3, 1, 9, 3},
      {-1, 0, 1, 2, 3, 4, 5, 8, 9, 10});
}

TEST_CASE(veb_layout_static_set_locks_recursive_layout) {
  std::vector<std::int64_t> values;
  for (std::int64_t value = 1; value <= 15; ++value) {
    values.push_back(value);
  }

  const VebLayoutStaticSet64 set(values);
  const std::vector<std::int64_t> expected{
      8, 4, 12,
      2, 1, 3,
      6, 5, 7,
      10, 9, 11,
      14, 13, 15};

  REQUIRE(set.layout_values() == expected);
  REQUIRE(set.valid_structure());
}

TEST_CASE(veb_layout_static_set_extreme_keys_and_boundaries) {
  using namespace veb_layout_static_set_test_detail;

  const std::vector<std::int64_t> values{
      std::numeric_limits<std::int64_t>::min(),
      -10,
      0,
      10,
      std::numeric_limits<std::int64_t>::max(),
      10,
      -10};

  const std::vector<std::int64_t> probes{
      std::numeric_limits<std::int64_t>::min(),
      std::numeric_limits<std::int64_t>::min() + 1,
      -11, -10, -9, 0, 9, 10, 11,
      std::numeric_limits<std::int64_t>::max() - 1,
      std::numeric_limits<std::int64_t>::max()};

  require_matches(values, probes);
}

TEST_CASE(veb_layout_static_set_all_small_cardinalities) {
  using namespace veb_layout_static_set_test_detail;

  for (std::size_t size = 0U; size <= 128U; ++size) {
    std::vector<std::int64_t> values;
    values.reserve(size);
    for (std::size_t index = 0U; index < size; ++index) {
      values.push_back(
          static_cast<std::int64_t>(index * 3U) - 100);
    }

    std::vector<std::int64_t> probes;
    for (std::int64_t value = -105; value <= 290; value += 7) {
      probes.push_back(value);
    }
    require_matches(values, probes);
  }
}

TEST_CASE(veb_layout_static_set_randomized_sorted_vector_oracle) {
  using namespace veb_layout_static_set_test_detail;

  std::mt19937_64 random(0x0BEEB1A70ULL);
  for (std::size_t trial = 0U; trial < 240U; ++trial) {
    const std::size_t size =
        static_cast<std::size_t>(random() % 320U);
    std::vector<std::int64_t> values(size);
    for (std::int64_t& value : values) {
      const std::uint64_t pick = random() % 29U;
      if (pick == 0U) {
        value = std::numeric_limits<std::int64_t>::min();
      } else if (pick == 1U) {
        value = std::numeric_limits<std::int64_t>::max();
      } else {
        value = static_cast<std::int64_t>(
                    random() % 1001U) -
                500;
      }
    }

    std::vector<std::int64_t> probes{
        std::numeric_limits<std::int64_t>::min(),
        std::numeric_limits<std::int64_t>::max(),
        -501, -500, -1, 0, 1, 500, 501};
    for (std::size_t query = 0U; query < 80U; ++query) {
      probes.push_back(
          static_cast<std::int64_t>(
              random() % 1201U) -
          600);
    }

    require_matches(values, probes);
  }
}
