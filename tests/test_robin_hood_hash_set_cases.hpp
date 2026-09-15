#pragma once

#include "algorithms/data_structures/robin_hood_hash_set.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <vector>

namespace {

using algorithms::data_structures::RobinHoodHashSet64;

std::uint64_t robin_test_mix(std::uint64_t value, std::uint64_t seed) {
  std::uint64_t z = value + seed + 0x9e3779b97f4a7c15ULL;
  z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
  z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
  return z ^ (z >> 31U);
}

std::vector<std::int64_t> set_values(const std::set<std::int64_t>& oracle) {
  return std::vector<std::int64_t>(oracle.begin(), oracle.end());
}

TEST_CASE(robin_hood_basic_replay_and_boundaries) {
  RobinHoodHashSet64 set(1, 0x123456789abcdef0ULL);
  REQUIRE(set.empty());
  REQUIRE_EQ(set.capacity(), std::size_t{8});
  REQUIRE(set.valid_structure());

  const std::vector<std::int64_t> values = {
      0, -1, 1, std::numeric_limits<std::int64_t>::min(),
      std::numeric_limits<std::int64_t>::max(), 17, -99};
  for (const std::int64_t value : values) {
    REQUIRE(set.insert(value));
    REQUIRE(set.contains(value));
    REQUIRE(set.valid_structure());
  }
  REQUIRE(!set.insert(17));
  REQUIRE_EQ(set.size(), values.size());

  std::vector<std::int64_t> expected = values;
  std::sort(expected.begin(), expected.end());
  REQUIRE_EQ(set.values_sorted(), expected);

  RobinHoodHashSet64 replay(1, 0x123456789abcdef0ULL);
  for (const std::int64_t value : values) {
    REQUIRE(replay.insert(value));
  }
  REQUIRE_EQ(replay.debug_slots(), set.debug_slots());

  REQUIRE(set.erase(std::numeric_limits<std::int64_t>::min()));
  REQUIRE(!set.contains(std::numeric_limits<std::int64_t>::min()));
  REQUIRE(!set.erase(std::numeric_limits<std::int64_t>::min()));
  REQUIRE(set.valid_structure());
}

TEST_CASE(robin_hood_collision_cluster_and_backward_shift) {
  constexpr std::uint64_t seed = 0x42ULL;
  constexpr std::size_t capacity = 8;
  std::vector<std::int64_t> colliders;
  for (std::int64_t key = 0; colliders.size() < 6; ++key) {
    const std::size_t home = static_cast<std::size_t>(
        robin_test_mix(static_cast<std::uint64_t>(key), seed)) &
                             (capacity - 1);
    if (home == 3) {
      colliders.push_back(key);
    }
  }

  RobinHoodHashSet64 set(capacity, seed);
  for (const auto key : colliders) {
    REQUIRE(set.insert(key));
  }
  REQUIRE_EQ(set.capacity(), capacity);
  REQUIRE(set.valid_structure());

  std::vector<std::size_t> distances;
  for (const auto& slot : set.debug_slots()) {
    if (slot.occupied && slot.home_index == 3) {
      distances.push_back(slot.probe_distance);
    }
  }
  std::sort(distances.begin(), distances.end());
  REQUIRE_EQ(distances,
             (std::vector<std::size_t>{0, 1, 2, 3, 4, 5}));

  REQUIRE(set.erase(colliders[2]));
  REQUIRE(set.valid_structure());
  for (std::size_t index = 0; index < colliders.size(); ++index) {
    REQUIRE_EQ(set.contains(colliders[index]), index != 2);
  }
  REQUIRE(set.erase(colliders[0]));
  REQUIRE(set.valid_structure());
  for (std::size_t index = 1; index < colliders.size(); ++index) {
    REQUIRE_EQ(set.contains(colliders[index]), index != 2);
  }
}

TEST_CASE(robin_hood_growth_preserves_set_and_replay) {
  RobinHoodHashSet64 first(8, 77);
  RobinHoodHashSet64 second(8, 77);
  std::vector<std::int64_t> values;
  for (std::int64_t i = -300; i <= 300; ++i) {
    const std::int64_t value = i * 1000003 + 17;
    values.push_back(value);
    REQUIRE(first.insert(value));
    REQUIRE(second.insert(value));
  }
  REQUIRE(first.capacity() > 8);
  REQUIRE(first.valid_structure());
  REQUIRE(second.valid_structure());
  REQUIRE_EQ(first.debug_slots(), second.debug_slots());
  for (const auto value : values) {
    REQUIRE(first.contains(value));
  }

  for (std::size_t index = 0; index < values.size(); index += 3) {
    REQUIRE(first.erase(values[index]));
    REQUIRE(second.erase(values[index]));
  }
  REQUIRE(first.valid_structure());
  REQUIRE_EQ(first.debug_slots(), second.debug_slots());
}

TEST_CASE(robin_hood_randomized_differential_and_structure) {
  constexpr std::uint64_t table_seed = 0x0ddc0ffee1234567ULL;
  RobinHoodHashSet64 set(8, table_seed);
  RobinHoodHashSet64 replay(8, table_seed);
  std::set<std::int64_t> oracle;
  std::mt19937_64 rng(0x726f62696e686f6fULL);

  for (std::size_t step = 0; step < 30000; ++step) {
    const std::int64_t key = static_cast<std::int64_t>(rng() % 4001ULL) - 2000;
    const unsigned operation = static_cast<unsigned>(rng() % 3ULL);
    if (operation == 0U) {
      const bool expected = oracle.insert(key).second;
      REQUIRE_EQ(set.insert(key), expected);
      REQUIRE_EQ(replay.insert(key), expected);
    } else if (operation == 1U) {
      const bool expected = oracle.erase(key) != 0;
      REQUIRE_EQ(set.erase(key), expected);
      REQUIRE_EQ(replay.erase(key), expected);
    } else {
      const bool expected = oracle.find(key) != oracle.end();
      REQUIRE_EQ(set.contains(key), expected);
      REQUIRE_EQ(replay.contains(key), expected);
    }

    if ((step % 61) == 0) {
      REQUIRE(set.valid_structure());
      REQUIRE(replay.valid_structure());
      REQUIRE_EQ(set.values_sorted(), set_values(oracle));
      REQUIRE_EQ(set.debug_slots(), replay.debug_slots());
    }
  }

  REQUIRE(set.valid_structure());
  REQUIRE_EQ(set.values_sorted(), set_values(oracle));
  REQUIRE_EQ(set.debug_slots(), replay.debug_slots());
}

}  // namespace
