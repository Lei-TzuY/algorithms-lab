#pragma once

#include "algorithms/data_structures/skip_list_set.hpp"
#include "test_framework.hpp"

#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <vector>

namespace {

using algorithms::data_structures::SkipListSet;

std::vector<std::int64_t> skip_oracle_values(const std::set<std::int64_t>& values) {
  return std::vector<std::int64_t>(values.begin(), values.end());
}

void require_skip_list_matches(const SkipListSet& list,
                               const std::set<std::int64_t>& oracle) {
  REQUIRE(list.valid_structure());
  REQUIRE_EQ(list.size(), oracle.size());
  REQUIRE_EQ(list.empty(), oracle.empty());
  REQUIRE_EQ(list.values_in_order(), skip_oracle_values(oracle));
  REQUIRE(list.active_level_count() >= 1U);
  REQUIRE(list.active_level_count() <= SkipListSet::kMaxLevel);
}

TEST_CASE(skip_list_basic_set_semantics_and_boundaries) {
  SkipListSet list(0xA11CE5EEDULL);
  std::set<std::int64_t> oracle;
  const std::vector<std::int64_t> values{
      5,
      -3,
      9,
      0,
      std::numeric_limits<std::int64_t>::min(),
      std::numeric_limits<std::int64_t>::max(),
  };
  for (const std::int64_t value : values) {
    REQUIRE_EQ(list.insert(value), oracle.insert(value).second);
    require_skip_list_matches(list, oracle);
  }
  REQUIRE(!list.insert(5));
  REQUIRE(list.contains(5));
  REQUIRE(!list.contains(6));
  REQUIRE(!list.erase(6));
  REQUIRE(list.erase(std::numeric_limits<std::int64_t>::min()));
  oracle.erase(std::numeric_limits<std::int64_t>::min());
  require_skip_list_matches(list, oracle);
}

TEST_CASE(skip_list_seed_replays_exact_tower_structure) {
  SkipListSet first(0x5EED1234ULL);
  SkipListSet second(0x5EED1234ULL);
  for (std::int64_t value = -200; value <= 200; ++value) {
    REQUIRE_EQ(first.insert(value), second.insert(value));
  }
  REQUIRE_EQ(first.values_in_order(), second.values_in_order());
  REQUIRE_EQ(first.tower_heights_in_order(), second.tower_heights_in_order());
  REQUIRE_EQ(first.active_level_count(), second.active_level_count());
  REQUIRE_EQ(first.promotion_draw_count(), second.promotion_draw_count());
  REQUIRE(first.active_level_count() > 1U);
  REQUIRE(first.promotion_draw_count() >= first.size());
  REQUIRE(first.valid_structure());
  REQUIRE(second.valid_structure());
}

TEST_CASE(skip_list_duplicate_insertions_do_not_consume_randomness) {
  SkipListSet baseline(0xD0B1CA7EULL);
  SkipListSet with_duplicates(0xD0B1CA7EULL);

  for (std::int64_t value = 0; value < 512; ++value) {
    REQUIRE(baseline.insert(value));
    REQUIRE(with_duplicates.insert(value));
    if ((value % 7) == 0) {
      const std::uint64_t before = with_duplicates.promotion_draw_count();
      REQUIRE(!with_duplicates.insert(value));
      REQUIRE_EQ(with_duplicates.promotion_draw_count(), before);
    }
  }

  REQUIRE_EQ(baseline.values_in_order(), with_duplicates.values_in_order());
  REQUIRE_EQ(baseline.tower_heights_in_order(),
             with_duplicates.tower_heights_in_order());
  REQUIRE_EQ(baseline.active_level_count(),
             with_duplicates.active_level_count());
  REQUIRE_EQ(baseline.promotion_draw_count(),
             with_duplicates.promotion_draw_count());
  REQUIRE(baseline.valid_structure());
  REQUIRE(with_duplicates.valid_structure());
}

TEST_CASE(skip_list_sorted_insertions_and_erasure_preserve_structure) {
  SkipListSet list(0x51A17E57ULL);
  std::set<std::int64_t> oracle;
  for (std::int64_t value = 0; value < 4096; ++value) {
    REQUIRE(list.insert(value));
    oracle.insert(value);
  }
  require_skip_list_matches(list, oracle);
  REQUIRE(list.active_level_count() > 1U);

  for (std::int64_t value = 0; value < 4096; value += 2) {
    REQUIRE(list.erase(value));
    oracle.erase(value);
  }
  require_skip_list_matches(list, oracle);
  for (std::int64_t value = 1; value < 4096; value += 2) {
    REQUIRE(list.contains(value));
  }
}

TEST_CASE(skip_list_randomized_differential_against_std_set) {
  SkipListSet list(0xBADC0FFEEULL);
  std::set<std::int64_t> oracle;
  std::mt19937_64 rng(0x5A17D1FFULL);
  std::uniform_int_distribution<std::int64_t> key_distribution(-750, 750);
  std::uniform_int_distribution<int> operation_distribution(0, 99);

  for (std::size_t step = 0U; step < 30000U; ++step) {
    const std::int64_t key = key_distribution(rng);
    const int operation = operation_distribution(rng);
    if (operation < 43) {
      REQUIRE_EQ(list.insert(key), oracle.insert(key).second);
    } else if (operation < 75) {
      const bool expected = oracle.erase(key) != 0U;
      REQUIRE_EQ(list.erase(key), expected);
    } else {
      REQUIRE_EQ(list.contains(key), oracle.contains(key));
    }

    REQUIRE(list.valid_structure());
    REQUIRE_EQ(list.size(), oracle.size());
    if ((step % 131U) == 0U) {
      REQUIRE_EQ(list.values_in_order(), skip_oracle_values(oracle));
    }
  }
  require_skip_list_matches(list, oracle);
}

}  // namespace
