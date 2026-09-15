#pragma once

#include "algorithms/data_structures/bk_tree.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace {
using algorithms::data_structures::BkTreeMatch;
using algorithms::data_structures::BkTreeStringSet;

bool bk_byte_less(std::string_view first, std::string_view second) {
  return std::lexicographical_compare(
      first.begin(), first.end(), second.begin(), second.end(),
      [](char left, char right) {
        return static_cast<unsigned char>(left) <
               static_cast<unsigned char>(right);
      });
}

std::size_t bk_oracle_distance(std::string_view first, std::string_view second) {
  std::vector<std::size_t> previous(second.size() + 1U, 0U);
  std::vector<std::size_t> current(second.size() + 1U, 0U);
  for (std::size_t column = 0U; column <= second.size(); ++column) {
    previous[column] = column;
  }
  for (std::size_t row = 1U; row <= first.size(); ++row) {
    current[0] = row;
    for (std::size_t column = 1U; column <= second.size(); ++column) {
      const std::size_t substitute =
          previous[column - 1U] +
          (first[row - 1U] == second[column - 1U] ? 0U : 1U);
      current[column] =
          std::min(substitute,
                   std::min(previous[column] + 1U, current[column - 1U] + 1U));
    }
    previous.swap(current);
  }
  return previous[second.size()];
}

std::vector<BkTreeMatch> bk_oracle_query(const std::vector<std::string>& values,
                                         std::string_view query,
                                         std::size_t radius) {
  std::vector<BkTreeMatch> matches;
  for (const std::string& value : values) {
    const std::size_t distance = bk_oracle_distance(value, query);
    if (distance <= radius) {
      matches.push_back({value, distance});
    }
  }
  std::sort(matches.begin(), matches.end(), [](const BkTreeMatch& first,
                                                const BkTreeMatch& second) {
    if (first.distance != second.distance) {
      return first.distance < second.distance;
    }
    return bk_byte_less(first.value, second.value);
  });
  return matches;
}

bool bk_insert_oracle(std::vector<std::string>& values,
                      const std::string& value) {
  if (std::find(values.begin(), values.end(), value) != values.end()) {
    return false;
  }
  values.push_back(value);
  return true;
}

std::string bk_random_bytes(std::mt19937_64& random, std::size_t max_length) {
  static constexpr std::array<unsigned char, 8> alphabet{
      0U, static_cast<unsigned char>('a'), static_cast<unsigned char>('b'),
      static_cast<unsigned char>('c'), 0x7FU, 0x80U, 0xFEU, 0xFFU};
  const std::size_t length =
      static_cast<std::size_t>(random() % (max_length + 1U));
  std::string value;
  value.reserve(length);
  for (std::size_t index = 0U; index < length; ++index) {
    value.push_back(static_cast<char>(
        alphabet[static_cast<std::size_t>(random() % alphabet.size())]));
  }
  return value;
}

TEST_CASE(bk_tree_empty_duplicates_bytes_and_determinism) {
  BkTreeStringSet tree;
  REQUIRE(tree.empty());
  REQUIRE_EQ(tree.size(), 0U);
  REQUIRE(tree.valid_structure());
  REQUIRE(tree.query_within("anything", 3U).matches.empty());

  REQUIRE(tree.insert("book"));
  REQUIRE(tree.insert("books"));
  REQUIRE(tree.insert("cake"));
  REQUIRE(tree.insert("boo"));
  REQUIRE(!tree.insert("book"));
  const std::string bytes{"\0\xFF", 2U};
  REQUIRE(tree.insert(bytes));
  REQUIRE(!tree.insert(bytes));
  REQUIRE_EQ(tree.size(), 5U);
  REQUIRE(tree.valid_structure());

  const auto result = tree.query_within("book", 1U);
  REQUIRE_EQ(result.matches,
             (std::vector<BkTreeMatch>{{"book", 0U}, {"boo", 1U},
                                       {"books", 1U}}));
  REQUIRE_EQ(tree.query_within("book", 1U), result);
  REQUIRE_EQ(tree.query_within(bytes, 0U).matches,
             (std::vector<BkTreeMatch>{{bytes, 0U}}));
}

TEST_CASE(bk_tree_triangle_pruning_is_observable_and_exact) {
  BkTreeStringSet tree;
  REQUIRE(tree.insert("aaaaaaaa"));
  REQUIRE(tree.insert("bbbbbbbb"));
  REQUIRE(tree.insert("cccccccc"));
  REQUIRE(tree.insert("dddddddd"));
  REQUIRE(tree.valid_structure());

  const auto exact = tree.query_within("aaaaaaaa", 0U);
  REQUIRE_EQ(exact.matches,
             (std::vector<BkTreeMatch>{{"aaaaaaaa", 0U}}));
  REQUIRE_EQ(exact.visited_nodes, 1U);
  REQUIRE(exact.pruned_children >= 1U);

  const auto broad = tree.query_within("aaaaaaaa", 8U);
  REQUIRE_EQ(broad.matches.size(), 4U);
  REQUIRE_EQ(broad.visited_nodes, tree.size());
}

TEST_CASE(bk_tree_incremental_insert_matches_independent_full_scan) {
  BkTreeStringSet tree;
  std::vector<std::string> oracle;
  const std::vector<std::string> values{
      "", "kitten", "sitting", "bitten", "mittens", "smitten", "kitten"};
  for (const std::string& value : values) {
    REQUIRE_EQ(tree.insert(value), bk_insert_oracle(oracle, value));
    REQUIRE(tree.valid_structure());
    for (const std::string& query : {std::string("kitten"), std::string("sitting"),
                                     std::string("")}) {
      for (std::size_t radius = 0U; radius <= 3U; ++radius) {
        REQUIRE_EQ(tree.query_within(query, radius).matches,
                   bk_oracle_query(oracle, query, radius));
      }
    }
  }
}

TEST_CASE(bk_tree_randomized_differential_against_two_row_oracle) {
  std::mt19937_64 random(0x424B54524545ULL);
  for (std::size_t trial = 0U; trial < 400U; ++trial) {
    BkTreeStringSet tree;
    std::vector<std::string> oracle;
    const std::size_t insertions = static_cast<std::size_t>(random() % 36U);
    for (std::size_t index = 0U; index < insertions; ++index) {
      const std::string value = bk_random_bytes(random, 8U);
      REQUIRE_EQ(tree.insert(value), bk_insert_oracle(oracle, value));
    }
    REQUIRE_EQ(tree.size(), oracle.size());
    REQUIRE(tree.valid_structure());

    for (std::size_t query_index = 0U; query_index < 30U; ++query_index) {
      const std::string query = bk_random_bytes(random, 8U);
      const std::size_t radius = static_cast<std::size_t>(random() % 5U);
      const auto result = tree.query_within(query, radius);
      REQUIRE_EQ(result.matches, bk_oracle_query(oracle, query, radius));
      REQUIRE(result.visited_nodes <= tree.size());
    }
  }
}

}  // namespace
