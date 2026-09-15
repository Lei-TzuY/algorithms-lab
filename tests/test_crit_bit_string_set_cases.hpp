#pragma once

#include "algorithms/data_structures/crit_bit_string_set.hpp"
#include "test_framework.hpp"

#include <initializer_list>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace {
using algorithms::data_structures::CritBitStringSet;

struct UnsignedLess {
  bool operator()(std::string_view first, std::string_view second) const {
    const std::size_t common = std::min(first.size(), second.size());
    for (std::size_t index = 0; index < common; ++index) {
      const auto left = static_cast<unsigned char>(first[index]);
      const auto right = static_cast<unsigned char>(second[index]);
      if (left != right) return left < right;
    }
    return first.size() < second.size();
  }
};

std::vector<std::string> oracle_values(const std::set<std::string, UnsignedLess>& oracle) {
  return {oracle.begin(), oracle.end()};
}

std::string bytes(std::initializer_list<unsigned> values) {
  std::string result;
  result.reserve(values.size());
  for (unsigned value : values) result.push_back(static_cast<char>(value));
  return result;
}

TEST_CASE(crit_bit_prefix_and_arbitrary_byte_semantics) {
  CritBitStringSet set;
  REQUIRE(set.empty());
  REQUIRE(set.valid_structure());
  REQUIRE(set.insert(""));
  REQUIRE(set.insert("a"));
  REQUIRE(set.insert("ab"));
  REQUIRE(set.insert(bytes({0x00U})));
  REQUIRE(set.insert(bytes({0x00U, 0xFFU})));
  REQUIRE(set.insert(bytes({0xFFU})));
  REQUIRE(!set.insert("ab"));
  REQUIRE_EQ(set.size(), 6U);
  REQUIRE_EQ(set.internal_node_count(), 5U);
  REQUIRE(set.contains(""));
  REQUIRE(set.contains("a"));
  REQUIRE(set.contains("ab"));
  REQUIRE(set.contains(bytes({0x00U, 0xFFU})));
  REQUIRE(set.contains(bytes({0xFFU})));
  REQUIRE(!set.contains("abc"));
  REQUIRE(set.valid_structure());

  std::set<std::string, UnsignedLess> oracle{
      "", "a", "ab", bytes({0x00U}), bytes({0x00U, 0xFFU}), bytes({0xFFU})};
  REQUIRE(set.valid_structure());
  REQUIRE_EQ(set.values_unsigned_lexicographic(), oracle_values(oracle));
}

TEST_CASE(crit_bit_erase_compresses_internal_nodes) {
  CritBitStringSet set;
  std::set<std::string, UnsignedLess> oracle;
  const std::vector<std::string> values{
      "alpha", "alphabet", "alpine", "beta", "", bytes({0x80U, 0x00U}), bytes({0x80U})};
  for (const auto& value : values) {
    REQUIRE(set.insert(value));
    oracle.insert(value);
    REQUIRE(set.valid_structure());
  }
  for (const auto& value : std::vector<std::string>{"alphabet", "", "beta", "missing",
                                                    bytes({0x80U, 0x00U}), "alpha", "alpine",
                                                    bytes({0x80U})}) {
    const bool expected = oracle.erase(value) != 0U;
    REQUIRE_EQ(set.erase(value), expected);
    REQUIRE(set.valid_structure());
    REQUIRE_EQ(set.values_unsigned_lexicographic(), oracle_values(oracle));
    REQUIRE_EQ(set.internal_node_count(), set.empty() ? 0U : set.size() - 1U);
  }
  REQUIRE(set.empty());
}

TEST_CASE(crit_bit_insertion_order_preserves_set_semantics_and_validity) {
  const std::vector<std::string> values{
      "", "a", "aa", "ab", "b", "ba", bytes({0x00U}), bytes({0x01U}),
      bytes({0x7FU}), bytes({0x80U}), bytes({0xFFU}), bytes({0xFFU, 0x00U})};
  CritBitStringSet forward;
  CritBitStringSet reverse;
  for (const auto& value : values) REQUIRE(forward.insert(value));
  for (auto it = values.rbegin(); it != values.rend(); ++it) REQUIRE(reverse.insert(*it));
  REQUIRE(forward.valid_structure());
  REQUIRE(reverse.valid_structure());
  REQUIRE_EQ(forward.values_unsigned_lexicographic(), reverse.values_unsigned_lexicographic());
  REQUIRE_EQ(forward.internal_node_count(), values.size() - 1U);
  REQUIRE_EQ(reverse.internal_node_count(), values.size() - 1U);
}

TEST_CASE(crit_bit_exhaustive_small_byte_prefix_corpus) {
  std::vector<std::string> keys{""};
  const std::vector<unsigned> alphabet{0x00U, 0x80U, 0xFFU};
  for (unsigned first : alphabet) {
    keys.push_back(bytes({first}));
    for (unsigned second : alphabet) {
      keys.push_back(bytes({first, second}));
    }
  }

  CritBitStringSet set;
  std::set<std::string, UnsignedLess> oracle;
  std::mt19937_64 generator(0x7061747269636961ULL);
  std::shuffle(keys.begin(), keys.end(), generator);
  for (const auto& key : keys) {
    REQUIRE(set.insert(key));
    oracle.insert(key);
    REQUIRE(set.valid_structure());
  }
  REQUIRE_EQ(set.values_unsigned_lexicographic(), oracle_values(oracle));

  std::shuffle(keys.begin(), keys.end(), generator);
  for (const auto& key : keys) {
    REQUIRE(set.erase(key));
    oracle.erase(key);
    REQUIRE(set.valid_structure());
    REQUIRE_EQ(set.values_unsigned_lexicographic(), oracle_values(oracle));
  }
  REQUIRE(set.empty());
}

TEST_CASE(crit_bit_randomized_differential_against_ordered_set) {
  CritBitStringSet set;
  std::set<std::string, UnsignedLess> oracle;
  std::mt19937_64 generator(0x63726974626974ULL);
  std::uniform_int_distribution<int> operation_distribution(0, 2);
  std::uniform_int_distribution<int> length_distribution(0, 12);
  std::uniform_int_distribution<int> byte_distribution(0, 255);

  auto random_key = [&]() {
    const std::size_t length = static_cast<std::size_t>(length_distribution(generator));
    std::string key(length, '\0');
    for (char& value : key) value = static_cast<char>(byte_distribution(generator));
    return key;
  };

  for (std::size_t step = 0; step < 600U; ++step) {
    const std::string key = random_key();
    const int operation = operation_distribution(generator);
    if (operation == 0) {
      const bool expected = oracle.insert(key).second;
      REQUIRE_EQ(set.insert(key), expected);
    } else if (operation == 1) {
      const bool expected = oracle.erase(key) != 0U;
      REQUIRE_EQ(set.erase(key), expected);
    } else {
      REQUIRE_EQ(set.contains(key), oracle.find(key) != oracle.end());
    }
    REQUIRE_EQ(set.size(), oracle.size());
    if ((step % 137U) == 0U) {
      REQUIRE(set.valid_structure());
      REQUIRE_EQ(set.values_unsigned_lexicographic(), oracle_values(oracle));
    }
  }
  REQUIRE(set.valid_structure());
  REQUIRE_EQ(set.values_unsigned_lexicographic(), oracle_values(oracle));
}

}  // namespace
