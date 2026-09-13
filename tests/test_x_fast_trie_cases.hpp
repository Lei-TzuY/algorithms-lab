#pragma once

#include "algorithms/data_structures/x_fast_trie_set.hpp"
#include "test_framework.hpp"

#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <set>

namespace {

using algorithms::data_structures::XFastTrieSet;

std::optional<std::uint64_t> xfast_oracle_predecessor(
    const std::set<std::uint64_t>& values, std::uint64_t key) {
  const auto it = values.lower_bound(key);
  if (it == values.begin()) {
    return std::nullopt;
  }
  auto previous = it;
  --previous;
  return *previous;
}

std::optional<std::uint64_t> xfast_oracle_successor(
    const std::set<std::uint64_t>& values, std::uint64_t key) {
  const auto it = values.upper_bound(key);
  if (it == values.end()) {
    return std::nullopt;
  }
  return *it;
}

TEST_CASE(x_fast_trie_validation_and_boundaries) {
  REQUIRE_THROWS_AS(XFastTrieSet(0), std::invalid_argument);
  XFastTrieSet small(8);
  REQUIRE(small.empty());
  REQUIRE(!small.minimum().has_value());
  REQUIRE(!small.maximum().has_value());
  REQUIRE(!small.predecessor(0).has_value());
  REQUIRE(!small.successor(255).has_value());
  REQUIRE_THROWS_AS(small.insert(256), std::out_of_range);
  REQUIRE_THROWS_AS(small.contains(256), std::out_of_range);

  REQUIRE(small.insert(0));
  REQUIRE(small.insert(255));
  REQUIRE(small.insert(128));
  REQUIRE(!small.insert(128));
  REQUIRE_EQ(small.minimum(), std::optional<std::uint64_t>{0});
  REQUIRE_EQ(small.maximum(), std::optional<std::uint64_t>{255});
  REQUIRE_EQ(small.predecessor(128), std::optional<std::uint64_t>{0});
  REQUIRE_EQ(small.successor(128), std::optional<std::uint64_t>{255});
  REQUIRE_EQ(small.predecessor(200), std::optional<std::uint64_t>{128});
  REQUIRE_EQ(small.successor(1), std::optional<std::uint64_t>{128});
  REQUIRE(small.valid_structure());

  REQUIRE(small.erase(128));
  REQUIRE(!small.erase(128));
  REQUIRE_EQ(small.successor(0), std::optional<std::uint64_t>{255});
  REQUIRE_EQ(small.predecessor(255), std::optional<std::uint64_t>{0});
  REQUIRE(small.valid_structure());

  XFastTrieSet full(64);
  REQUIRE(full.insert(0));
  REQUIRE(full.insert(std::numeric_limits<std::uint64_t>::max()));
  REQUIRE_EQ(full.successor(0),
             std::optional<std::uint64_t>{
                 std::numeric_limits<std::uint64_t>::max()});
  REQUIRE_EQ(full.predecessor(std::numeric_limits<std::uint64_t>::max()),
             std::optional<std::uint64_t>{0});
  REQUIRE(full.valid_structure());
}

TEST_CASE(x_fast_trie_prefix_repair_after_deletions) {
  XFastTrieSet trie(16);
  const std::uint64_t values[] = {0x1000U, 0x1001U, 0x1002U,
                                  0x1fffU, 0x8000U, 0xffffU};
  for (const auto value : values) {
    REQUIRE(trie.insert(value));
  }
  REQUIRE(trie.valid_structure());
  REQUIRE(trie.erase(0x1000U));
  REQUIRE(trie.erase(0x1002U));
  REQUIRE(trie.erase(0x1fffU));
  REQUIRE_EQ(trie.predecessor(0x8000U),
             std::optional<std::uint64_t>{0x1001U});
  REQUIRE_EQ(trie.successor(0x1001U),
             std::optional<std::uint64_t>{0x8000U});
  REQUIRE(trie.valid_structure());
  REQUIRE(trie.erase(0x1001U));
  REQUIRE_EQ(trie.minimum(), std::optional<std::uint64_t>{0x8000U});
  REQUIRE(trie.valid_structure());
}

TEST_CASE(x_fast_trie_randomized_differential) {
  XFastTrieSet trie(16);
  std::set<std::uint64_t> oracle;
  std::mt19937_64 rng(0x5846415354545249ULL);

  for (std::size_t step = 0; step < 30000U; ++step) {
    const std::uint64_t key = rng() & 0xffffU;
    const std::uint64_t operation = rng() % 5U;
    if (operation == 0U) {
      REQUIRE_EQ(trie.insert(key), oracle.insert(key).second);
    } else if (operation == 1U) {
      REQUIRE_EQ(trie.erase(key), oracle.erase(key) != 0U);
    } else if (operation == 2U) {
      REQUIRE_EQ(trie.contains(key), oracle.find(key) != oracle.end());
    } else if (operation == 3U) {
      REQUIRE_EQ(trie.predecessor(key),
                 xfast_oracle_predecessor(oracle, key));
    } else {
      REQUIRE_EQ(trie.successor(key), xfast_oracle_successor(oracle, key));
    }

    REQUIRE_EQ(trie.size(), oracle.size());
    if (oracle.empty()) {
      REQUIRE(!trie.minimum().has_value());
      REQUIRE(!trie.maximum().has_value());
    } else {
      REQUIRE_EQ(trie.minimum(),
                 std::optional<std::uint64_t>{*oracle.begin()});
      REQUIRE_EQ(trie.maximum(),
                 std::optional<std::uint64_t>{*oracle.rbegin()});
    }
    if (step % 127U == 0U) {
      REQUIRE(trie.valid_structure());
    }
  }
  REQUIRE(trie.valid_structure());
}

TEST_CASE(x_fast_trie_full_width_randomized_differential) {
  XFastTrieSet trie(64);
  std::set<std::uint64_t> oracle;
  std::mt19937_64 rng(0x46554c4c57494454ULL);

  for (std::size_t step = 0; step < 15000U; ++step) {
    const std::uint64_t key = rng();
    const std::uint64_t operation = rng() % 4U;
    if (operation == 0U) {
      REQUIRE_EQ(trie.insert(key), oracle.insert(key).second);
    } else if (operation == 1U) {
      REQUIRE_EQ(trie.erase(key), oracle.erase(key) != 0U);
    } else if (operation == 2U) {
      REQUIRE_EQ(trie.predecessor(key),
                 xfast_oracle_predecessor(oracle, key));
    } else {
      REQUIRE_EQ(trie.successor(key), xfast_oracle_successor(oracle, key));
    }
    if (step % 211U == 0U) {
      REQUIRE(trie.valid_structure());
    }
  }
  REQUIRE(trie.valid_structure());
}

TEST_CASE(x_fast_trie_monotone_population_and_clear) {
  XFastTrieSet trie(20);
  for (std::uint64_t key = 0; key < 4096U; ++key) {
    REQUIRE(trie.insert(key * 17U));
  }
  REQUIRE(trie.valid_structure());
  for (std::uint64_t key = 0; key < 4096U; key += 2U) {
    REQUIRE(trie.erase(key * 17U));
  }
  REQUIRE(trie.valid_structure());
  for (std::uint64_t key = 1; key < 4096U; key += 2U) {
    REQUIRE(trie.erase(key * 17U));
  }
  REQUIRE(trie.empty());
  REQUIRE(trie.valid_structure());
}

}  // namespace
