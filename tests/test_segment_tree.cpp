#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

#include "algorithms/data_structures/fenwick_tree.hpp"
#include "algorithms/data_structures/segment_tree.hpp"
#include "algorithms/data_structures/segment_tree_beats.hpp"

namespace {

using algorithms::data_structures::FenwickTree;
using algorithms::data_structures::SegmentTree;
using algorithms::data_structures::SegmentTreeBeats;

std::int64_t naive_range(const std::vector<std::int64_t>& values,
                         std::size_t begin, std::size_t end) {
  std::int64_t sum = 0;
  for (std::size_t index = begin; index < end; ++index) {
    sum += values[index];
  }
  return sum;
}

void apply_naive_chmin(std::vector<std::int64_t>& values, std::size_t begin,
                       std::size_t end, std::int64_t cap) {
  for (std::size_t index = begin; index < end; ++index) {
    values[index] = std::min(values[index], cap);
  }
}

}  // namespace

TEST_CASE(segment_tree_builds_and_answers_all_ranges) {
  const std::vector<std::int64_t> values{5, -4, 7, 0, 11, -3, 2};
  SegmentTree tree(values);
  REQUIRE_EQ(tree.size(), values.size());

  for (std::size_t begin = 0; begin <= values.size(); ++begin) {
    for (std::size_t end = begin; end <= values.size(); ++end) {
      REQUIRE_EQ(tree.range_sum(begin, end),
                 naive_range(values, begin, end));
    }
  }

  SegmentTree zeros(5);
  REQUIRE_EQ(zeros.size(), std::size_t{5});
  REQUIRE_EQ(zeros.range_sum(0, 5), std::int64_t{0});
  zeros.assign(2, 9);
  zeros.assign(4, -2);
  REQUIRE_EQ(zeros.range_sum(1, 5), std::int64_t{7});
}

TEST_CASE(segment_tree_validates_bounds_empty_and_layout_size) {
  SegmentTree empty(0);
  REQUIRE_EQ(empty.size(), std::size_t{0});
  REQUIRE_EQ(empty.range_sum(0, 0), std::int64_t{0});
  REQUIRE_THROWS_AS(empty.assign(0, 1), std::out_of_range);
  REQUIRE_THROWS_AS(empty.range_sum(0, 1), std::out_of_range);

  SegmentTree tree(3);
  REQUIRE_THROWS_AS(tree.assign(3, 1), std::out_of_range);
  REQUIRE_THROWS_AS(tree.range_sum(2, 1), std::out_of_range);
  REQUIRE_THROWS_AS(tree.range_sum(0, 4), std::out_of_range);
  REQUIRE_THROWS_AS(SegmentTree(std::numeric_limits<std::size_t>::max()),
                    std::length_error);
}

TEST_CASE(segment_tree_rejects_unrepresentable_nodes_and_assigns_transactionally) {
  const std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
  const std::int64_t minimum = std::numeric_limits<std::int64_t>::min();

  REQUIRE_THROWS_AS(
      (SegmentTree(std::vector<std::int64_t>{maximum, 1})),
      std::overflow_error);

  SegmentTree positive(
      std::vector<std::int64_t>{maximum, 0, -maximum, 0});
  REQUIRE_EQ(positive.range_sum(0, 4), std::int64_t{0});
  REQUIRE_THROWS_AS(positive.assign(1, 1), std::overflow_error);
  REQUIRE_EQ(positive.range_sum(0, 2), maximum);
  REQUIRE_EQ(positive.range_sum(1, 2), std::int64_t{0});
  REQUIRE_EQ(positive.range_sum(0, 4), std::int64_t{0});

  SegmentTree negative(
      std::vector<std::int64_t>{minimum, 0, maximum, 0});
  REQUIRE_EQ(negative.range_sum(0, 4), std::int64_t{-1});
  REQUIRE_THROWS_AS(negative.assign(1, -1), std::overflow_error);
  REQUIRE_EQ(negative.range_sum(0, 2), minimum);
  REQUIRE_EQ(negative.range_sum(1, 2), std::int64_t{0});
  REQUIRE_EQ(negative.range_sum(0, 4), std::int64_t{-1});
}

TEST_CASE(segment_tree_range_aggregation_handles_representability_boundaries) {
  const std::int64_t maximum = std::numeric_limits<std::int64_t>::max();

  SegmentTree transient(std::vector<std::int64_t>{
      maximum, 0, 0, 0, 1, 0, -1, 0});
  REQUIRE_EQ(transient.range_sum(0, 7), maximum);

  SegmentTree range_overflow(
      std::vector<std::int64_t>{-maximum, maximum, maximum, -maximum});
  REQUIRE_EQ(range_overflow.range_sum(0, 4), std::int64_t{0});
  REQUIRE_THROWS_AS(range_overflow.range_sum(1, 3), std::overflow_error);
}

TEST_CASE(segment_tree_matches_naive_and_fenwick_randomized_models) {
  std::mt19937_64 rng(0x5345474D454E54ULL);
  std::uniform_int_distribution<int> size_distribution(0, 64);
  std::uniform_int_distribution<int> value_distribution(-100, 100);
  std::uniform_int_distribution<int> operation_distribution(0, 99);

  for (std::size_t trial = 0; trial < 180; ++trial) {
    const std::size_t size =
        static_cast<std::size_t>(size_distribution(rng));
    std::vector<std::int64_t> values;
    values.reserve(size);
    for (std::size_t index = 0; index < size; ++index) {
      values.push_back(static_cast<std::int64_t>(value_distribution(rng)));
    }

    SegmentTree segment(values);
    FenwickTree fenwick(size);
    for (std::size_t index = 0; index < size; ++index) {
      fenwick.add(index, values[index]);
    }

    for (std::size_t operation = 0; operation < 200; ++operation) {
      if (size > 0U && operation_distribution(rng) < 55) {
        std::uniform_int_distribution<std::size_t> index_distribution(
            0U, size - 1U);
        const std::size_t index = index_distribution(rng);
        const std::int64_t replacement =
            static_cast<std::int64_t>(value_distribution(rng));
        const std::int64_t delta = replacement - values[index];
        segment.assign(index, replacement);
        fenwick.add(index, delta);
        values[index] = replacement;
      } else {
        std::uniform_int_distribution<std::size_t> endpoint_distribution(
            0U, size);
        const std::size_t first = endpoint_distribution(rng);
        const std::size_t second = endpoint_distribution(rng);
        const std::size_t begin = std::min(first, second);
        const std::size_t end = std::max(first, second);
        const std::int64_t oracle = naive_range(values, begin, end);
        REQUIRE_EQ(segment.range_sum(begin, end), oracle);
        REQUIRE_EQ(fenwick.range_sum(begin, end), oracle);
      }
    }

    for (std::size_t begin = 0; begin <= size; ++begin) {
      for (std::size_t end = begin; end <= size; ++end) {
        const std::int64_t oracle = naive_range(values, begin, end);
        REQUIRE_EQ(segment.range_sum(begin, end), oracle);
        REQUIRE_EQ(fenwick.range_sum(begin, end), oracle);
      }
    }
  }
}

TEST_CASE(segment_tree_beats_basic_shortcuts_and_partial_ranges) {
  SegmentTreeBeats tree({5, 1, 5, 3, 2});
  REQUIRE_EQ(tree.size(), 5U);
  REQUIRE_EQ(tree.range_sum(0, 5), 16);

  tree.range_chmin(0, 5, 4);
  REQUIRE_EQ(tree.range_sum(0, 5), 14);

  tree.range_chmin(1, 4, 2);
  REQUIRE_EQ(tree.range_sum(0, 5), 11);
  REQUIRE_EQ(tree.range_sum(1, 4), 5);

  tree.range_chmin(0, 5, 2);
  REQUIRE_EQ(tree.range_sum(0, 5), 9);
  tree.range_chmin(0, 5, 2);
  REQUIRE_EQ(tree.range_sum(0, 5), 9);
}

TEST_CASE(segment_tree_beats_cap_equal_second_maximum_descends) {
  SegmentTreeBeats tree({7, 7, 3, 3});
  tree.range_chmin(0, 4, 3);
  REQUIRE_EQ(tree.range_sum(0, 4), 12);
  REQUIRE_EQ(tree.range_sum(0, 1), 3);
  REQUIRE_EQ(tree.range_sum(1, 3), 6);
}

TEST_CASE(segment_tree_beats_empty_extreme_and_validation) {
  SegmentTreeBeats empty({});
  REQUIRE_EQ(empty.size(), 0U);
  REQUIRE_EQ(empty.range_sum(0, 0), 0);
  empty.range_chmin(0, 0, 0);

  SegmentTreeBeats extreme({std::numeric_limits<std::int64_t>::max()});
  REQUIRE_EQ(extreme.range_sum(0, 1),
             std::numeric_limits<std::int64_t>::max());
  extreme.range_chmin(0, 1, 0);
  REQUIRE_EQ(extreme.range_sum(0, 1), 0);

  REQUIRE_THROWS_AS(SegmentTreeBeats(std::vector<std::int64_t>{1, -1}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(
      SegmentTreeBeats(std::vector<std::int64_t>{
          std::numeric_limits<std::int64_t>::max(), 1}),
      std::overflow_error);

  SegmentTreeBeats tree({1, 2, 3});
  REQUIRE_THROWS_AS(tree.range_chmin(0, 3, -1), std::invalid_argument);
  REQUIRE_THROWS_AS(tree.range_chmin(2, 1, 0), std::out_of_range);
  REQUIRE_THROWS_AS(tree.range_chmin(0, 4, 0), std::out_of_range);
  REQUIRE_THROWS_AS(tree.range_sum(2, 1), std::out_of_range);
  REQUIRE_THROWS_AS(tree.range_sum(0, 4), std::out_of_range);
}

TEST_CASE(segment_tree_beats_randomized_differential_against_naive_vector) {
  std::mt19937_64 random(0x5E6B3A75ULL);

  for (std::size_t trial = 0; trial < 300U; ++trial) {
    const std::size_t size = static_cast<std::size_t>(random() % 49U);
    std::vector<std::int64_t> values(size);
    for (std::int64_t& value : values) {
      value = static_cast<std::int64_t>(random() % 1001U);
    }

    SegmentTreeBeats tree(values);
    for (std::size_t step = 0; step < 200U; ++step) {
      std::size_t begin =
          size == 0U ? 0U : static_cast<std::size_t>(random() % (size + 1U));
      std::size_t end =
          size == 0U ? 0U : static_cast<std::size_t>(random() % (size + 1U));
      if (begin > end) {
        std::swap(begin, end);
      }

      if ((random() & 1U) == 0U) {
        const std::int64_t cap =
            static_cast<std::int64_t>(random() % 1001U);
        tree.range_chmin(begin, end, cap);
        apply_naive_chmin(values, begin, end, cap);
      } else {
        REQUIRE_EQ(tree.range_sum(begin, end),
                   naive_range(values, begin, end));
      }

      if (size > 0U && step % 40U == 0U) {
        REQUIRE_EQ(tree.range_sum(0, size), naive_range(values, 0, size));
        for (std::size_t index = 0; index < size; ++index) {
          REQUIRE_EQ(tree.range_sum(index, index + 1U), values[index]);
        }
        for (std::size_t prefix = 0; prefix <= size; ++prefix) {
          REQUIRE_EQ(tree.range_sum(0, prefix),
                     naive_range(values, 0, prefix));
        }
      }
    }
  }
}
