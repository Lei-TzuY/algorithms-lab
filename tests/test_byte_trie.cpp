#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <map>
#include <random>
#include <string>
#include <string_view>

#include "algorithms/data_structures/byte_trie.hpp"

namespace {

using algorithms::data_structures::ByteTrie;

std::size_t oracle_prefix_count(
    const std::map<std::string, std::size_t>& oracle,
    std::string_view prefix) {
  std::size_t count = 0;
  for (const auto& [key, multiplicity] : oracle) {
    if (key.size() >= prefix.size() &&
        std::equal(prefix.begin(), prefix.end(), key.begin())) {
      count += multiplicity;
    }
  }
  return count;
}

std::size_t oracle_total(const std::map<std::string, std::size_t>& oracle) {
  std::size_t total = 0;
  for (const auto& [key, multiplicity] : oracle) {
    (void)key;
    total += multiplicity;
  }
  return total;
}

std::string random_key(std::mt19937_64& rng) {
  std::uniform_int_distribution<int> length_distribution(0, 8);
  std::uniform_int_distribution<int> byte_distribution(0, 255);
  const std::size_t length =
      static_cast<std::size_t>(length_distribution(rng));
  std::string key;
  key.reserve(length);
  for (std::size_t index = 0; index < length; ++index) {
    key.push_back(static_cast<char>(byte_distribution(rng)));
  }
  return key;
}

}  // namespace

TEST_CASE(byte_trie_tracks_duplicates_empty_keys_and_prefixes) {
  ByteTrie trie;
  REQUIRE_EQ(trie.total_count(), std::size_t{0});
  REQUIRE_EQ(trie.count_prefix(""), std::size_t{0});

  trie.insert("");
  trie.insert("");
  trie.insert("a");
  trie.insert("ab");
  trie.insert("ab");
  trie.insert("abc");

  REQUIRE_EQ(trie.total_count(), std::size_t{6});
  REQUIRE_EQ(trie.count(""), std::size_t{2});
  REQUIRE_EQ(trie.count("a"), std::size_t{1});
  REQUIRE_EQ(trie.count("ab"), std::size_t{2});
  REQUIRE_EQ(trie.count("abc"), std::size_t{1});
  REQUIRE_EQ(trie.count("abcd"), std::size_t{0});
  REQUIRE(trie.contains("ab"));
  REQUIRE(!trie.contains("b"));
  REQUIRE_EQ(trie.count_prefix(""), std::size_t{6});
  REQUIRE_EQ(trie.count_prefix("a"), std::size_t{4});
  REQUIRE_EQ(trie.count_prefix("ab"), std::size_t{3});
  REQUIRE_EQ(trie.count_prefix("abc"), std::size_t{1});
  REQUIRE_EQ(trie.count_prefix("z"), std::size_t{0});
}

TEST_CASE(byte_trie_treats_null_and_high_bit_bytes_as_data) {
  ByteTrie trie;
  const std::string embedded_null{"a\0b", 3};
  std::string null_prefix{"a\0", 2};
  std::string high_bit;
  high_bit.push_back(static_cast<char>(0xFF));
  high_bit.push_back(static_cast<char>(0x80));
  std::string high_prefix(1, static_cast<char>(0xFF));

  trie.insert(embedded_null);
  trie.insert(embedded_null);
  trie.insert(high_bit);

  REQUIRE_EQ(trie.count(embedded_null), std::size_t{2});
  REQUIRE_EQ(trie.count_prefix(null_prefix), std::size_t{2});
  REQUIRE_EQ(trie.count(high_bit), std::size_t{1});
  REQUIRE_EQ(trie.count_prefix(high_prefix), std::size_t{1});
  REQUIRE_EQ(trie.total_count(), std::size_t{3});
}

TEST_CASE(byte_trie_matches_map_and_prefix_scan_randomized_model) {
  std::mt19937_64 rng(0x5452494542595445ULL);
  std::uniform_int_distribution<int> operation_distribution(0, 99);

  for (std::size_t trial = 0; trial < 180; ++trial) {
    ByteTrie trie;
    std::map<std::string, std::size_t> oracle;

    for (std::size_t operation = 0; operation < 180; ++operation) {
      const std::string key = random_key(rng);
      if (operation_distribution(rng) < 60) {
        trie.insert(key);
        ++oracle[key];
      } else {
        const auto found = oracle.find(key);
        const std::size_t expected =
            found == oracle.end() ? 0U : found->second;
        REQUIRE_EQ(trie.count(key), expected);
        REQUIRE_EQ(trie.contains(key), expected != 0U);
      }

      const std::string prefix_source = random_key(rng);
      std::uniform_int_distribution<std::size_t> prefix_length_distribution(
          0U, prefix_source.size());
      const std::size_t prefix_length = prefix_length_distribution(rng);
      const std::string_view prefix(prefix_source.data(), prefix_length);
      REQUIRE_EQ(trie.count_prefix(prefix),
                 oracle_prefix_count(oracle, prefix));
      REQUIRE_EQ(trie.total_count(), oracle_total(oracle));
    }

    for (const auto& [key, multiplicity] : oracle) {
      REQUIRE_EQ(trie.count(key), multiplicity);
      for (std::size_t length = 0; length <= key.size(); ++length) {
        const std::string_view prefix(key.data(), length);
        REQUIRE_EQ(trie.count_prefix(prefix),
                   oracle_prefix_count(oracle, prefix));
      }
    }
  }
}
