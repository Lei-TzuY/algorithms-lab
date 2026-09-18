#pragma once

#include "algorithms/data_structures/finger_tree.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace finger_tree_test_detail {

using algorithms::data_structures::PersistentFingerTree;

inline void require_matches(const PersistentFingerTree& tree,
                            const std::vector<std::int64_t>& expected) {
  REQUIRE(tree.valid_invariants());
  REQUIRE_EQ(tree.size(), expected.size());
  REQUIRE_EQ(tree.empty(), expected.empty());
  REQUIRE(tree.to_vector() == expected);
  if (expected.empty()) {
    REQUIRE_THROWS_AS(tree.front(), std::out_of_range);
    REQUIRE_THROWS_AS(tree.back(), std::out_of_range);
    REQUIRE_THROWS_AS(tree.at(0U), std::out_of_range);
    REQUIRE_THROWS_AS(tree.pop_front(), std::out_of_range);
    REQUIRE_THROWS_AS(tree.pop_back(), std::out_of_range);
    return;
  }
  REQUIRE_EQ(tree.front(), expected.front());
  REQUIRE_EQ(tree.back(), expected.back());
  for (std::size_t index = 0U; index < expected.size(); ++index) {
    REQUIRE_EQ(tree.at(index), expected[index]);
  }
  REQUIRE_THROWS_AS(tree.at(expected.size()), std::out_of_range);
}

}  // namespace finger_tree_test_detail

TEST_CASE(finger_tree_basic_end_operations_and_full_width_values) {
  using namespace finger_tree_test_detail;

  PersistentFingerTree empty;
  require_matches(empty, {});

  const auto first = empty.push_back(std::numeric_limits<std::int64_t>::min());
  const auto second = first.push_back(7);
  const auto third = second.push_front(std::numeric_limits<std::int64_t>::max());
  const auto fourth = third.push_front(-4);

  require_matches(empty, {});
  require_matches(first, {std::numeric_limits<std::int64_t>::min()});
  require_matches(second, {std::numeric_limits<std::int64_t>::min(), 7});
  require_matches(third, {std::numeric_limits<std::int64_t>::max(),
                          std::numeric_limits<std::int64_t>::min(), 7});
  require_matches(fourth, {-4, std::numeric_limits<std::int64_t>::max(),
                           std::numeric_limits<std::int64_t>::min(), 7});
  require_matches(fourth.pop_front(),
                  {std::numeric_limits<std::int64_t>::max(),
                   std::numeric_limits<std::int64_t>::min(), 7});
  require_matches(fourth.pop_back(),
                  {-4, std::numeric_limits<std::int64_t>::max(),
                   std::numeric_limits<std::int64_t>::min()});
}

TEST_CASE(finger_tree_deep_digit_carries_and_borrows_preserve_order) {
  using namespace finger_tree_test_detail;

  PersistentFingerTree tree;
  std::vector<std::int64_t> expected;
  for (std::int64_t value = 0; value < 2048; ++value) {
    tree = tree.push_back(value);
    expected.push_back(value);
    if ((value & 63) == 0) {
      REQUIRE(tree.valid_invariants());
    }
  }
  require_matches(tree, expected);

  PersistentFingerTree front_popped = tree;
  for (std::size_t count = 0U; count < 1024U; ++count) {
    REQUIRE_EQ(front_popped.front(), static_cast<std::int64_t>(count));
    front_popped = front_popped.pop_front();
    if ((count & 63U) == 0U) {
      REQUIRE(front_popped.valid_invariants());
    }
  }
  std::vector<std::int64_t> tail(expected.begin() + 1024, expected.end());
  require_matches(front_popped, tail);

  PersistentFingerTree back_popped = tree;
  for (std::size_t count = 0U; count < 1024U; ++count) {
    const std::int64_t expected_back = 2047 - static_cast<std::int64_t>(count);
    REQUIRE_EQ(back_popped.back(), expected_back);
    back_popped = back_popped.pop_back();
    if ((count & 63U) == 0U) {
      REQUIRE(back_popped.valid_invariants());
    }
  }
  std::vector<std::int64_t> head(expected.begin(), expected.begin() + 1024);
  require_matches(back_popped, head);
}

TEST_CASE(finger_tree_branching_persistence_preserves_old_versions) {
  using namespace finger_tree_test_detail;

  std::mt19937_64 random(0xF17E7AEEULL);
  std::vector<PersistentFingerTree> versions(1U);
  std::vector<std::vector<std::int64_t>> oracles(1U);

  for (std::size_t step = 0U; step < 1500U; ++step) {
    const std::size_t base_index =
        static_cast<std::size_t>(random() % versions.size());
    const PersistentFingerTree base = versions[base_index];
    const std::vector<std::int64_t> before = oracles[base_index];
    std::vector<std::int64_t> after = before;
    PersistentFingerTree next = base;

    const unsigned operation = static_cast<unsigned>(random() % 6U);
    const std::int64_t value = static_cast<std::int64_t>(random());
    if (operation == 0U || before.empty()) {
      next = base.push_front(value);
      after.insert(after.begin(), value);
    } else if (operation == 1U) {
      next = base.push_back(value);
      after.push_back(value);
    } else if (operation == 2U) {
      next = base.pop_front();
      after.erase(after.begin());
    } else if (operation == 3U) {
      next = base.pop_back();
      after.pop_back();
    } else if (operation == 4U) {
      const std::size_t index =
          static_cast<std::size_t>(random() % before.size());
      REQUIRE_EQ(base.at(index), before[index]);
    } else {
      REQUIRE_EQ(base.front(), before.front());
      REQUIRE_EQ(base.back(), before.back());
    }

    // The selected historical version must remain unchanged after deriving next.
    REQUIRE(base.valid_invariants());
    REQUIRE(next.valid_invariants());
    REQUIRE(base.to_vector() == before);
    REQUIRE(next.to_vector() == after);
    REQUIRE_EQ(base.size(), before.size());
    REQUIRE_EQ(next.size(), after.size());
    if ((step % 257U) == 0U) {
      require_matches(base, before);
      require_matches(next, after);
    }
    versions.push_back(std::move(next));
    oracles.push_back(std::move(after));
  }
}

TEST_CASE(finger_tree_mixed_linear_history_matches_vector_oracle) {
  using namespace finger_tree_test_detail;

  std::mt19937_64 random(0xC411D1AULL);
  PersistentFingerTree tree;
  std::vector<std::int64_t> expected;

  for (std::size_t step = 0U; step < 8000U; ++step) {
    const unsigned operation = static_cast<unsigned>(random() % 8U);
    const std::int64_t value = static_cast<std::int64_t>(random());
    if (operation <= 1U || expected.empty()) {
      tree = tree.push_front(value);
      expected.insert(expected.begin(), value);
    } else if (operation <= 3U) {
      tree = tree.push_back(value);
      expected.push_back(value);
    } else if (operation == 4U) {
      tree = tree.pop_front();
      expected.erase(expected.begin());
    } else if (operation == 5U) {
      tree = tree.pop_back();
      expected.pop_back();
    } else {
      const std::size_t index =
          static_cast<std::size_t>(random() % expected.size());
      REQUIRE_EQ(tree.at(index), expected[index]);
    }

    REQUIRE(tree.valid_invariants());
    REQUIRE_EQ(tree.size(), expected.size());
    if ((step % 251U) == 0U) {
      REQUIRE(tree.to_vector() == expected);
    }
  }
  require_matches(tree, expected);
}
