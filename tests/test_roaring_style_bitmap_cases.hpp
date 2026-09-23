#pragma once

#include "algorithms/data_structures/roaring_style_bitmap.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <random>
#include <set>
#include <vector>

using algorithms::data_structures::RoaringStyleBitmap32;

namespace roaring_style_bitmap_test_detail {

inline std::size_t oracle_rank(
    const std::set<std::uint32_t>& values,
    const std::uint32_t key) {
  return static_cast<std::size_t>(
      std::distance(values.begin(), values.upper_bound(key)));
}

inline std::optional<std::uint32_t> oracle_select(
    const std::set<std::uint32_t>& values,
    const std::size_t index) {
  if (index >= values.size()) {
    return std::nullopt;
  }
  auto it = values.begin();
  std::advance(it, static_cast<std::ptrdiff_t>(index));
  return *it;
}

inline void require_matches(
    const RoaringStyleBitmap32& bitmap,
    const std::set<std::uint32_t>& oracle,
    const std::vector<std::uint32_t>& probes) {
  REQUIRE_EQ(bitmap.size(), oracle.size());
  REQUIRE(bitmap.empty() == oracle.empty());
  REQUIRE(bitmap.valid_structure());

  for (const std::uint32_t value : probes) {
    REQUIRE(bitmap.contains(value) ==
            oracle.contains(value));
    REQUIRE_EQ(bitmap.rank(value),
               oracle_rank(oracle, value));
  }

  if (oracle.empty()) {
    REQUIRE(!bitmap.select(0U).has_value());
  } else {
    REQUIRE(bitmap.select(0U) ==
            oracle_select(oracle, 0U));
    REQUIRE(bitmap.select(oracle.size() - 1U) ==
            oracle_select(oracle, oracle.size() - 1U));
  }
  REQUIRE(!bitmap.select(oracle.size()).has_value());
}

}  // namespace roaring_style_bitmap_test_detail

TEST_CASE(roaring_style_bitmap_empty_basic_boundaries_and_rank_select) {
  RoaringStyleBitmap32 bitmap;
  REQUIRE(bitmap.empty());
  REQUIRE_EQ(bitmap.size(), 0U);
  REQUIRE_EQ(bitmap.container_count(), 0U);
  REQUIRE(bitmap.valid_structure());
  REQUIRE_EQ(bitmap.rank(0U), 0U);
  REQUIRE(!bitmap.select(0U).has_value());

  const std::vector<std::uint32_t> values{
      0U,
      1U,
      UINT32_C(65535),
      UINT32_C(65536),
      UINT32_C(65537),
      std::numeric_limits<std::uint32_t>::max()};

  for (const std::uint32_t value : values) {
    REQUIRE(bitmap.insert(value));
    REQUIRE(bitmap.contains(value));
    REQUIRE(!bitmap.insert(value));
  }

  REQUIRE_EQ(bitmap.size(), values.size());
  REQUIRE_EQ(bitmap.container_count(), 3U);
  REQUIRE_EQ(bitmap.array_container_count(), 3U);
  REQUIRE_EQ(bitmap.bitmap_container_count(), 0U);
  REQUIRE(bitmap.valid_structure());

  REQUIRE_EQ(bitmap.rank(0U), 1U);
  REQUIRE_EQ(bitmap.rank(UINT32_C(65535)), 3U);
  REQUIRE_EQ(bitmap.rank(UINT32_C(65536)), 4U);
  REQUIRE_EQ(
      bitmap.rank(std::numeric_limits<std::uint32_t>::max()),
      values.size());

  for (std::size_t index = 0U; index < values.size(); ++index) {
    REQUIRE(bitmap.select(index) ==
            std::optional<std::uint32_t>(values[index]));
  }
  REQUIRE(!bitmap.select(values.size()).has_value());

  REQUIRE(bitmap.erase(UINT32_C(65536)));
  REQUIRE(!bitmap.contains(UINT32_C(65536)));
  REQUIRE(!bitmap.erase(UINT32_C(65536)));
  REQUIRE_EQ(bitmap.size(), values.size() - 1U);
  REQUIRE(bitmap.valid_structure());
}

TEST_CASE(roaring_style_bitmap_crosses_array_bitmap_threshold_both_ways) {
  RoaringStyleBitmap32 bitmap;
  const std::uint32_t prefix = UINT32_C(37) << 16U;

  for (std::uint32_t low = 0U; low < UINT32_C(4096); ++low) {
    REQUIRE(bitmap.insert(prefix | low));
  }
  REQUIRE_EQ(bitmap.array_container_count(), 1U);
  REQUIRE_EQ(bitmap.bitmap_container_count(), 0U);
  REQUIRE(bitmap.valid_structure());

  REQUIRE(bitmap.insert(prefix | UINT32_C(4096)));
  REQUIRE_EQ(bitmap.size(), 4097U);
  REQUIRE_EQ(bitmap.array_container_count(), 0U);
  REQUIRE_EQ(bitmap.bitmap_container_count(), 1U);
  REQUIRE(bitmap.valid_structure());

  REQUIRE(bitmap.contains(prefix));
  REQUIRE(bitmap.contains(prefix | UINT32_C(4096)));
  REQUIRE_EQ(bitmap.rank(prefix | UINT32_C(4096)), 4097U);
  REQUIRE(bitmap.select(4096U) ==
          std::optional<std::uint32_t>(
              prefix | UINT32_C(4096)));

  REQUIRE(bitmap.erase(prefix | UINT32_C(2048)));
  REQUIRE_EQ(bitmap.size(), 4096U);
  REQUIRE_EQ(bitmap.array_container_count(), 1U);
  REQUIRE_EQ(bitmap.bitmap_container_count(), 0U);
  REQUIRE(!bitmap.contains(prefix | UINT32_C(2048)));
  REQUIRE(bitmap.valid_structure());

  REQUIRE(bitmap.insert(prefix | UINT32_C(5000)));
  REQUIRE_EQ(bitmap.size(), 4097U);
  REQUIRE_EQ(bitmap.bitmap_container_count(), 1U);
  REQUIRE(bitmap.valid_structure());
}

TEST_CASE(roaring_style_bitmap_erases_empty_container_and_preserves_order) {
  RoaringStyleBitmap32 bitmap;

  REQUIRE(bitmap.insert(UINT32_C(0x00020005)));
  REQUIRE(bitmap.insert(UINT32_C(0x00000007)));
  REQUIRE(bitmap.insert(UINT32_C(0x00010003)));
  REQUIRE_EQ(bitmap.container_count(), 3U);
  REQUIRE(bitmap.valid_structure());

  REQUIRE(bitmap.select(0U) ==
          std::optional<std::uint32_t>(UINT32_C(0x00000007)));
  REQUIRE(bitmap.select(1U) ==
          std::optional<std::uint32_t>(UINT32_C(0x00010003)));
  REQUIRE(bitmap.select(2U) ==
          std::optional<std::uint32_t>(UINT32_C(0x00020005)));

  REQUIRE(bitmap.erase(UINT32_C(0x00010003)));
  REQUIRE_EQ(bitmap.container_count(), 2U);
  REQUIRE_EQ(bitmap.rank(UINT32_C(0x0001ffff)), 1U);
  REQUIRE(bitmap.valid_structure());
}

TEST_CASE(roaring_style_bitmap_randomized_matches_std_set) {
  using namespace roaring_style_bitmap_test_detail;

  std::mt19937_64 random(0x524F4152494E47ULL);
  RoaringStyleBitmap32 bitmap;
  std::set<std::uint32_t> oracle;

  for (std::size_t step = 0U; step < 18000U; ++step) {
    std::uint32_t value{};
    if ((random() % 3U) != 0U) {
      const std::uint32_t prefix =
          static_cast<std::uint32_t>(random() % 5U) << 16U;
      value = prefix |
              static_cast<std::uint32_t>(random() % 8192U);
    } else {
      value = static_cast<std::uint32_t>(random());
    }

    if ((random() & 1ULL) == 0ULL) {
      const bool expected = oracle.insert(value).second;
      REQUIRE_EQ(bitmap.insert(value), expected);
    } else {
      const bool expected = oracle.erase(value) != 0U;
      REQUIRE_EQ(bitmap.erase(value), expected);
    }

    if (step % 41U == 0U) {
      const std::vector<std::uint32_t> probes{
          0U,
          1U,
          UINT32_C(65535),
          UINT32_C(65536),
          static_cast<std::uint32_t>(random()),
          value,
          std::numeric_limits<std::uint32_t>::max()};
      require_matches(bitmap, oracle, probes);

      if (!oracle.empty()) {
        const std::size_t index =
            static_cast<std::size_t>(
                random() % oracle.size());
        REQUIRE(bitmap.select(index) ==
                oracle_select(oracle, index));
      }
    }
  }

  const std::vector<std::uint32_t> final_probes{
      0U,
      UINT32_C(4096),
      UINT32_C(65535),
      UINT32_C(65536),
      UINT32_C(0x0004ffff),
      std::numeric_limits<std::uint32_t>::max()};
  require_matches(bitmap, oracle, final_probes);
}
