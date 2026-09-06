#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

#include "algorithms/data_structures/fenwick_tree.hpp"

namespace {

using algorithms::data_structures::FenwickTree;

std::int64_t naive_prefix(const std::vector<std::int64_t>& values,
                          std::size_t end) {
  std::int64_t sum = 0;
  for (std::size_t index = 0; index < end; ++index) {
    sum += values[index];
  }
  return sum;
}

std::int64_t naive_range(const std::vector<std::int64_t>& values,
                         std::size_t begin, std::size_t end) {
  std::int64_t sum = 0;
  for (std::size_t index = begin; index < end; ++index) {
    sum += values[index];
  }
  return sum;
}

}  // namespace

TEST_CASE(fenwick_tree_supports_point_updates_prefixes_and_ranges) {
  FenwickTree tree(8);
  std::vector<std::int64_t> values(8, 0);

  const auto apply = [&](std::size_t index, std::int64_t delta) {
    tree.add(index, delta);
    values[index] += delta;
  };

  apply(0, 5);
  apply(3, 7);
  apply(7, 11);
  apply(3, -2);
  apply(1, -4);

  REQUIRE_EQ(tree.size(), std::size_t{8});
  for (std::size_t end = 0; end <= values.size(); ++end) {
    REQUIRE_EQ(tree.prefix_sum(end), naive_prefix(values, end));
  }
  for (std::size_t begin = 0; begin <= values.size(); ++begin) {
    for (std::size_t end = begin; end <= values.size(); ++end) {
      REQUIRE_EQ(tree.range_sum(begin, end),
                 naive_range(values, begin, end));
    }
  }
}

TEST_CASE(fenwick_tree_validates_bounds_and_empty_tree) {
  FenwickTree empty(0);
  REQUIRE_EQ(empty.size(), std::size_t{0});
  REQUIRE_EQ(empty.prefix_sum(0), std::int64_t{0});
  REQUIRE_EQ(empty.range_sum(0, 0), std::int64_t{0});
  REQUIRE_THROWS_AS(empty.add(0, 1), std::out_of_range);
  REQUIRE_THROWS_AS(empty.prefix_sum(1), std::out_of_range);
  REQUIRE_THROWS_AS(FenwickTree(std::numeric_limits<std::size_t>::max()),
                    std::length_error);

  FenwickTree tree(3);
  REQUIRE_THROWS_AS(tree.add(3, 1), std::out_of_range);
  REQUIRE_THROWS_AS(tree.prefix_sum(4), std::out_of_range);
  REQUIRE_THROWS_AS(tree.range_sum(2, 1), std::out_of_range);
  REQUIRE_THROWS_AS(tree.range_sum(0, 4), std::out_of_range);
}

TEST_CASE(fenwick_tree_overflow_updates_are_transactional) {
  FenwickTree positive(2);
  positive.add(0, std::numeric_limits<std::int64_t>::max());
  REQUIRE_EQ(positive.prefix_sum(2),
             std::numeric_limits<std::int64_t>::max());
  REQUIRE_THROWS_AS(positive.add(1, 1), std::overflow_error);
  REQUIRE_EQ(positive.prefix_sum(1),
             std::numeric_limits<std::int64_t>::max());
  REQUIRE_EQ(positive.prefix_sum(2),
             std::numeric_limits<std::int64_t>::max());
  REQUIRE_EQ(positive.range_sum(1, 2), std::int64_t{0});

  FenwickTree negative(2);
  negative.add(0, std::numeric_limits<std::int64_t>::min());
  REQUIRE_EQ(negative.prefix_sum(2),
             std::numeric_limits<std::int64_t>::min());
  REQUIRE_THROWS_AS(negative.add(1, -1), std::overflow_error);
  REQUIRE_EQ(negative.prefix_sum(2),
             std::numeric_limits<std::int64_t>::min());
}

TEST_CASE(fenwick_tree_prefix_aggregation_avoids_false_transient_overflow) {
  FenwickTree tree(7);
  tree.add(0, -1);
  tree.add(4, 1);
  tree.add(6, std::numeric_limits<std::int64_t>::max());

  // The Fenwick decomposition of prefix 7 contains MAX, +1, and -1. A fixed
  // MAX-then-+1 accumulation would overflow even though the exact answer fits.
  REQUIRE_EQ(tree.prefix_sum(7),
             std::numeric_limits<std::int64_t>::max());

  FenwickTree range_overflow(3);
  const std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
  range_overflow.add(0, -maximum);
  range_overflow.add(1, maximum);
  range_overflow.add(2, maximum);
  REQUIRE_EQ(range_overflow.prefix_sum(3), maximum);
  REQUIRE_THROWS_AS(range_overflow.range_sum(1, 3), std::overflow_error);
}

TEST_CASE(fenwick_tree_matches_naive_randomized_model) {
  std::mt19937_64 rng(0x46454E5749434BULL);
  std::uniform_int_distribution<int> size_distribution(0, 64);
  std::uniform_int_distribution<int> delta_distribution(-100, 100);
  std::uniform_int_distribution<int> operation_distribution(0, 99);

  for (std::size_t trial = 0; trial < 220; ++trial) {
    const std::size_t size =
        static_cast<std::size_t>(size_distribution(rng));
    FenwickTree tree(size);
    std::vector<std::int64_t> values(size, 0);

    for (std::size_t operation = 0; operation < 220; ++operation) {
      if (size > 0U && operation_distribution(rng) < 58) {
        std::uniform_int_distribution<std::size_t> index_distribution(
            0U, size - 1U);
        const std::size_t index = index_distribution(rng);
        const std::int64_t delta =
            static_cast<std::int64_t>(delta_distribution(rng));
        tree.add(index, delta);
        values[index] += delta;
      } else {
        std::uniform_int_distribution<std::size_t> endpoint_distribution(
            0U, size);
        const std::size_t first = endpoint_distribution(rng);
        const std::size_t second = endpoint_distribution(rng);
        const std::size_t begin = std::min(first, second);
        const std::size_t end = std::max(first, second);
        REQUIRE_EQ(tree.prefix_sum(end), naive_prefix(values, end));
        REQUIRE_EQ(tree.range_sum(begin, end),
                   naive_range(values, begin, end));
      }
    }

    for (std::size_t end = 0; end <= size; ++end) {
      REQUIRE_EQ(tree.prefix_sum(end), naive_prefix(values, end));
    }
  }
}
