#pragma once

#include "algorithms/data_structures/y_fast_trie_set.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <set>
#include <vector>

namespace {
using algorithms::data_structures::YFastTrieSet;

std::optional<std::uint64_t> oracle_predecessor(const std::set<std::uint64_t>& s,
                                                std::uint64_t key) {
  auto it = s.lower_bound(key);
  if (it == s.begin()) {
    return std::nullopt;
  }
  --it;
  return *it;
}

std::optional<std::uint64_t> oracle_successor(const std::set<std::uint64_t>& s,
                                              std::uint64_t key) {
  auto it = s.upper_bound(key);
  if (it == s.end()) {
    return std::nullopt;
  }
  return *it;
}

TEST_CASE(y_fast_validation_and_boundaries) {
  REQUIRE_THROWS_AS(YFastTrieSet(0, 1), std::invalid_argument);
  REQUIRE_THROWS_AS(YFastTrieSet(65, 1), std::invalid_argument);
  YFastTrieSet set(8, 7);
  REQUIRE(set.empty());
  REQUIRE(!set.minimum().has_value());
  REQUIRE(!set.maximum().has_value());
  REQUIRE(!set.predecessor(0).has_value());
  REQUIRE(!set.successor(255).has_value());
  REQUIRE_THROWS_AS(set.insert(256), std::out_of_range);
  REQUIRE_THROWS_AS(set.contains(256), std::out_of_range);
  REQUIRE(set.insert(0));
  REQUIRE(set.insert(255));
  REQUIRE(!set.insert(0));
  REQUIRE_EQ(set.minimum(), std::optional<std::uint64_t>{0});
  REQUIRE_EQ(set.maximum(), std::optional<std::uint64_t>{255});
  REQUIRE_EQ(set.successor(0), std::optional<std::uint64_t>{255});
  REQUIRE_EQ(set.predecessor(255), std::optional<std::uint64_t>{0});
  REQUIRE(set.valid_structure());

  YFastTrieSet full(64, 9);
  const auto high = std::numeric_limits<std::uint64_t>::max();
  REQUIRE(full.insert(0));
  REQUIRE(full.insert(std::uint64_t{1} << 63U));
  REQUIRE(full.insert(high));
  REQUIRE_EQ(full.predecessor(high),
             std::optional<std::uint64_t>{std::uint64_t{1} << 63U});
  REQUIRE_EQ(full.successor(0),
             std::optional<std::uint64_t>{std::uint64_t{1} << 63U});
  REQUIRE(full.valid_structure());
}

TEST_CASE(y_fast_split_merge_space_invariant_and_replay) {
  YFastTrieSet first(8, 0x12345678ULL);
  YFastTrieSet second(8, 0x12345678ULL);
  for (std::uint64_t key = 0; key < 220; ++key) {
    REQUIRE(first.insert(key));
    REQUIRE(second.insert(key));
  }
  REQUIRE(first.bucket_count() > 1U);
  REQUIRE_EQ(first.values_in_order(), second.values_in_order());
  REQUIRE_EQ(first.representatives_in_order(),
             second.representatives_in_order());
  REQUIRE_EQ(first.bucket_sizes_in_order(), second.bucket_sizes_in_order());
  REQUIRE_EQ(first.bucket_roots_in_order(), second.bucket_roots_in_order());
  REQUIRE(first.valid_structure());
  REQUIRE(second.valid_structure());
  REQUIRE(first.representative_count() * first.bucket_target() <= first.size());

  for (std::uint64_t key = 0; key < 190; ++key) {
    REQUIRE(first.erase(key));
    REQUIRE(second.erase(key));
  }
  REQUIRE(first.valid_structure());
  REQUIRE(second.valid_structure());
  REQUIRE_EQ(first.values_in_order(), second.values_in_order());
  REQUIRE_EQ(first.bucket_roots_in_order(), second.bucket_roots_in_order());
  if (first.bucket_count() > 1U) {
    REQUIRE(first.representative_count() * first.bucket_target() <=
            first.size());
  }
}

TEST_CASE(y_fast_randomized_differential_16_bit) {
  YFastTrieSet actual(16, 0x9E3779B97F4A7C15ULL);
  std::set<std::uint64_t> expected;
  std::mt19937_64 rng(0xC001D00D1234ULL);
  for (std::size_t step = 0; step < 40000U; ++step) {
    const std::uint64_t key = rng() & 0xFFFFULL;
    const auto operation = rng() % 5U;
    if (operation == 0U) {
      REQUIRE_EQ(actual.insert(key), expected.insert(key).second);
    } else if (operation == 1U) {
      REQUIRE_EQ(actual.erase(key), expected.erase(key) != 0U);
    } else if (operation == 2U) {
      REQUIRE_EQ(actual.contains(key), expected.contains(key));
    } else if (operation == 3U) {
      REQUIRE_EQ(actual.predecessor(key), oracle_predecessor(expected, key));
    } else {
      REQUIRE_EQ(actual.successor(key), oracle_successor(expected, key));
    }
    REQUIRE_EQ(actual.size(), expected.size());
    if ((step % 257U) == 0U) {
      REQUIRE(actual.valid_structure());
      const std::vector<std::uint64_t> expected_values(expected.begin(),
                                                       expected.end());
      REQUIRE_EQ(actual.values_in_order(), expected_values);
      const auto expected_min =
          expected.empty() ? std::optional<std::uint64_t>{}
                           : std::optional<std::uint64_t>{*expected.begin()};
      const auto expected_max =
          expected.empty() ? std::optional<std::uint64_t>{}
                           : std::optional<std::uint64_t>{*expected.rbegin()};
      REQUIRE_EQ(actual.minimum(), expected_min);
      REQUIRE_EQ(actual.maximum(), expected_max);
      if (actual.bucket_count() > 1U) {
        REQUIRE(actual.representative_count() * actual.bucket_target() <=
                actual.size());
      }
    }
  }
}

TEST_CASE(y_fast_randomized_differential_full_width) {
  YFastTrieSet actual(64, 0xD1FF3A3ULL);
  std::set<std::uint64_t> expected;
  std::mt19937_64 rng(0xF00DFACEB00CULL);
  for (std::size_t step = 0; step < 15000U; ++step) {
    const std::uint64_t key = rng();
    if ((rng() & 3U) == 0U) {
      REQUIRE_EQ(actual.erase(key), expected.erase(key) != 0U);
    } else {
      REQUIRE_EQ(actual.insert(key), expected.insert(key).second);
    }
    if ((step % 29U) == 0U) {
      const auto query = rng();
      REQUIRE_EQ(actual.contains(query), expected.contains(query));
      REQUIRE_EQ(actual.predecessor(query), oracle_predecessor(expected, query));
      REQUIRE_EQ(actual.successor(query), oracle_successor(expected, query));
      REQUIRE(actual.valid_structure());
    }
  }
  REQUIRE(actual.valid_structure());
  const std::vector<std::uint64_t> expected_values(expected.begin(),
                                                   expected.end());
  REQUIRE_EQ(actual.values_in_order(), expected_values);
}

}  // namespace
