#include "algorithms/data_structures/b_tree_set.hpp"
#include "test_framework.hpp"

#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

namespace {

using algorithms::data_structures::BTreeSet;

std::vector<std::int64_t> as_vector(const std::set<std::int64_t>& values) {
  return {values.begin(), values.end()};
}

void require_matches(const BTreeSet& tree,
                     const std::set<std::int64_t>& oracle) {
  REQUIRE(tree.valid_structure());
  REQUIRE_EQ(tree.size(), oracle.size());
  REQUIRE_EQ(tree.empty(), oracle.empty());
  REQUIRE(tree.values_in_order() == as_vector(oracle));
}

}  // namespace

TEST_CASE(b_tree_validates_degree_and_basic_set_semantics) {
  REQUIRE_THROWS_AS(BTreeSet(1U), std::invalid_argument);

  BTreeSet tree(2U);
  REQUIRE_EQ(tree.minimum_degree(), 2U);
  REQUIRE(tree.empty());
  REQUIRE(tree.valid_structure());
  REQUIRE_EQ(tree.height(), 0U);
  REQUIRE(!tree.contains(0));
  REQUIRE(!tree.erase(0));

  const std::vector<std::int64_t> keys{
      0, -9, 17, std::numeric_limits<std::int64_t>::min(),
      std::numeric_limits<std::int64_t>::max(), 4, -3};
  std::set<std::int64_t> oracle;
  for (const std::int64_t key : keys) {
    REQUIRE_EQ(tree.insert(key), oracle.insert(key).second);
    require_matches(tree, oracle);
  }
  REQUIRE(!tree.insert(4));
  REQUIRE(tree.contains(std::numeric_limits<std::int64_t>::min()));
  REQUIRE(tree.contains(std::numeric_limits<std::int64_t>::max()));
  require_matches(tree, oracle);
}

TEST_CASE(b_tree_split_borrow_merge_and_root_shrink_paths) {
  BTreeSet tree(2U);
  std::set<std::int64_t> oracle;
  for (std::int64_t key = 1; key <= 128; ++key) {
    REQUIRE(tree.insert(key));
    oracle.insert(key);
    REQUIRE(tree.valid_structure());
  }
  REQUIRE(tree.height() >= 3U);
  REQUIRE(tree.diagnostics().splits > 0U);

  std::vector<std::int64_t> erase_order;
  for (std::int64_t key = 2; key <= 128; key += 2) {
    erase_order.push_back(key);
  }
  for (std::int64_t key = 127; key >= 1; key -= 2) {
    erase_order.push_back(key);
  }

  for (const std::int64_t key : erase_order) {
    REQUIRE(tree.erase(key));
    oracle.erase(key);
    require_matches(tree, oracle);
  }
  REQUIRE(tree.empty());
  REQUIRE_EQ(tree.height(), 0U);
  const auto& diagnostics = tree.diagnostics();
  REQUIRE(diagnostics.merges > 0U);
  REQUIRE(diagnostics.root_shrinks > 0U);
  REQUIRE(diagnostics.borrows_from_previous > 0U);
  REQUIRE(diagnostics.borrows_from_next > 0U);
}

TEST_CASE(b_tree_multiple_degrees_preserve_ordered_set_semantics) {
  for (const std::size_t degree : {2U, 3U, 5U, 8U}) {
    BTreeSet tree(degree);
    std::set<std::int64_t> oracle;
    for (std::int64_t key = -250; key <= 250; ++key) {
      if ((key % 3) != 0) {
        REQUIRE(tree.insert(key));
        oracle.insert(key);
      }
    }
    require_matches(tree, oracle);

    for (std::int64_t key = 250; key >= -250; --key) {
      if ((key % 5) == 0) {
        REQUIRE_EQ(tree.erase(key), oracle.erase(key) != 0U);
        require_matches(tree, oracle);
      }
    }
  }
}

TEST_CASE(b_tree_randomized_differential_against_std_set) {
  std::mt19937_64 rng(0xB7EE5E7ULL);
  std::uniform_int_distribution<std::int64_t> key_distribution(-2000, 2000);
  std::uniform_int_distribution<int> operation_distribution(0, 99);

  for (const std::size_t degree : {2U, 3U, 6U}) {
    BTreeSet tree(degree);
    std::set<std::int64_t> oracle;

    for (std::size_t step = 0; step < 20000U; ++step) {
      const std::int64_t key = key_distribution(rng);
      const int operation = operation_distribution(rng);
      if (operation < 42) {
        const bool expected = oracle.insert(key).second;
        REQUIRE_EQ(tree.insert(key), expected);
      } else if (operation < 76) {
        const bool expected = oracle.erase(key) != 0U;
        REQUIRE_EQ(tree.erase(key), expected);
      } else {
        REQUIRE_EQ(tree.contains(key), oracle.contains(key));
      }

      REQUIRE(tree.valid_structure());
      REQUIRE_EQ(tree.size(), oracle.size());
      if ((step % 97U) == 0U) {
        REQUIRE(tree.values_in_order() == as_vector(oracle));
      }
    }
    require_matches(tree, oracle);
  }
}
