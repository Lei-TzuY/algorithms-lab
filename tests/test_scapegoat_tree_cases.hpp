#pragma once

#include "algorithms/data_structures/scapegoat_tree_set.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <vector>

namespace {

using algorithms::data_structures::ScapegoatTreeSet;

std::vector<std::int64_t> set_values(const std::set<std::int64_t>& values) {
  return std::vector<std::int64_t>(values.begin(), values.end());
}

void require_scapegoat_matches(const ScapegoatTreeSet& tree,
                               const std::set<std::int64_t>& oracle) {
  REQUIRE(tree.valid_structure());
  REQUIRE_EQ(tree.size(), oracle.size());
  REQUIRE_EQ(tree.empty(), oracle.empty());
  REQUIRE_EQ(tree.values_in_order(), set_values(oracle));
  REQUIRE(tree.maximum_size_since_rebuild() >= tree.size());
  if (!tree.empty()) {
    REQUIRE(tree.height() <=
            ScapegoatTreeSet::allowed_depth(tree.maximum_size_since_rebuild()));
  }
}

TEST_CASE(scapegoat_tree_basic_set_semantics_and_boundaries) {
  ScapegoatTreeSet tree;
  std::set<std::int64_t> oracle;
  require_scapegoat_matches(tree, oracle);

  const std::vector<std::int64_t> values{
      5,
      -3,
      9,
      0,
      std::numeric_limits<std::int64_t>::min(),
      std::numeric_limits<std::int64_t>::max(),
  };
  for (const std::int64_t value : values) {
    REQUIRE_EQ(tree.insert(value), oracle.insert(value).second);
    require_scapegoat_matches(tree, oracle);
  }
  REQUIRE(!tree.insert(5));
  REQUIRE(tree.contains(5));
  REQUIRE(!tree.contains(6));
  REQUIRE(!tree.erase(6));
  REQUIRE(tree.erase(std::numeric_limits<std::int64_t>::min()));
  oracle.erase(std::numeric_limits<std::int64_t>::min());
  require_scapegoat_matches(tree, oracle);
}

TEST_CASE(scapegoat_tree_sorted_insertions_trigger_local_rebuilds) {
  ScapegoatTreeSet tree;
  std::set<std::int64_t> oracle;
  for (std::int64_t value = 0; value < 512; ++value) {
    REQUIRE(tree.insert(value));
    oracle.insert(value);
    REQUIRE(tree.valid_structure());
    REQUIRE(tree.height() <=
            ScapegoatTreeSet::allowed_depth(tree.maximum_size_since_rebuild()));
  }
  REQUIRE(tree.insertion_rebuild_count() > 0U);
  REQUIRE_EQ(tree.values_in_order(), set_values(oracle));
}

TEST_CASE(scapegoat_tree_deletion_triggers_global_shrink_rebuild) {
  ScapegoatTreeSet tree;
  std::set<std::int64_t> oracle;
  for (std::int64_t value = 0; value < 192; ++value) {
    REQUIRE(tree.insert(value));
    oracle.insert(value);
  }
  const std::size_t original_q = tree.maximum_size_since_rebuild();
  for (std::int64_t value = 0; value < 80; ++value) {
    REQUIRE(tree.erase(value));
    oracle.erase(value);
  }
  REQUIRE(tree.root_rebuild_count() > 0U);
  REQUIRE(tree.maximum_size_since_rebuild() < original_q);
  require_scapegoat_matches(tree, oracle);

  for (std::int64_t value = 80; value < 192; ++value) {
    REQUIRE(tree.erase(value));
    oracle.erase(value);
  }
  REQUIRE(tree.empty());
  REQUIRE_EQ(tree.maximum_size_since_rebuild(), 0U);
  require_scapegoat_matches(tree, oracle);
}

TEST_CASE(scapegoat_tree_randomized_differential_against_std_set) {
  ScapegoatTreeSet tree;
  std::set<std::int64_t> oracle;
  std::mt19937_64 rng(0x5CA9E60A7ULL);
  std::uniform_int_distribution<std::int64_t> key_distribution(-400, 400);
  std::uniform_int_distribution<int> operation_distribution(0, 99);

  for (std::size_t step = 0U; step < 30000U; ++step) {
    const std::int64_t key = key_distribution(rng);
    const int operation = operation_distribution(rng);
    if (operation < 42) {
      REQUIRE_EQ(tree.insert(key), oracle.insert(key).second);
    } else if (operation < 74) {
      const bool expected = oracle.erase(key) != 0U;
      REQUIRE_EQ(tree.erase(key), expected);
    } else {
      REQUIRE_EQ(tree.contains(key), oracle.contains(key));
    }

    REQUIRE(tree.valid_structure());
    REQUIRE_EQ(tree.size(), oracle.size());
    if (!tree.empty()) {
      REQUIRE(tree.height() <=
              ScapegoatTreeSet::allowed_depth(tree.maximum_size_since_rebuild()));
    }
    if ((step % 127U) == 0U) {
      REQUIRE_EQ(tree.values_in_order(), set_values(oracle));
    }
  }
  REQUIRE_EQ(tree.values_in_order(), set_values(oracle));
  REQUIRE(tree.insertion_rebuild_count() > 0U);
}

}  // namespace
