#pragma once

#include "algorithms/data_structures/cartesian_tree_rmq.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <vector>

namespace {

using algorithms::data_structures::CartesianTreeRmq;

std::size_t cartesian_rmq_naive_index(const std::vector<std::int64_t>& values,
                                      std::size_t begin,
                                      std::size_t end) {
  std::size_t best = begin;
  for (std::size_t index = begin + 1U; index < end; ++index) {
    if (values[index] < values[best]) {
      best = index;
    }
  }
  return best;
}

TEST_CASE(cartesian_tree_rmq_contract_and_structure) {
  CartesianTreeRmq empty({});
  REQUIRE(empty.empty());
  REQUIRE(empty.valid_structure());
  REQUIRE(!empty.root_index().has_value());
  REQUIRE_THROWS_AS(empty.range_min_index(0U, 0U), std::invalid_argument);

  CartesianTreeRmq singleton({17});
  REQUIRE_EQ(singleton.size(), 1U);
  REQUIRE_EQ(singleton.root_index(), std::optional<std::size_t>(0U));
  REQUIRE_EQ(singleton.range_min_index(0U, 1U), 0U);
  REQUIRE_EQ(singleton.range_min(0U, 1U), 17);
  REQUIRE(singleton.valid_structure());
  REQUIRE_THROWS_AS(singleton.parent_of(1U), std::out_of_range);
  REQUIRE_THROWS_AS(singleton.range_min_index(1U, 1U), std::invalid_argument);
  REQUIRE_THROWS_AS(singleton.range_min_index(0U, 2U), std::out_of_range);
  REQUIRE_THROWS_AS(singleton.range_min_index(1U, 0U), std::out_of_range);
}

TEST_CASE(cartesian_tree_rmq_leftmost_ties_and_full_signed_domain) {
  const std::vector<std::int64_t> values{
      std::numeric_limits<std::int64_t>::max(), 1, 1, 2, 1,
      std::numeric_limits<std::int64_t>::min(),
      std::numeric_limits<std::int64_t>::min(), 9};
  CartesianTreeRmq index(values);
  REQUIRE(index.valid_structure());
  REQUIRE_EQ(index.range_min_index(0U, values.size()), 5U);
  REQUIRE_EQ(index.range_min_index(1U, 5U), 1U);
  REQUIRE_EQ(index.range_min_index(5U, 7U), 5U);
  REQUIRE_EQ(index.values(), values);

  for (std::size_t begin = 0U; begin < values.size(); ++begin) {
    for (std::size_t end = begin + 1U; end <= values.size(); ++end) {
      REQUIRE_EQ(index.range_min_index(begin, end),
                 cartesian_rmq_naive_index(values, begin, end));
    }
  }
}

TEST_CASE(cartesian_tree_rmq_monotone_chains_are_iterative) {
  constexpr std::size_t size = 4096U;
  std::vector<std::int64_t> increasing(size);
  std::vector<std::int64_t> decreasing(size);
  for (std::size_t index = 0U; index < size; ++index) {
    increasing[index] = static_cast<std::int64_t>(index);
    decreasing[index] = static_cast<std::int64_t>(size - index);
  }

  CartesianTreeRmq inc(increasing);
  CartesianTreeRmq dec(decreasing);
  REQUIRE(inc.valid_structure());
  REQUIRE(dec.valid_structure());
  REQUIRE_EQ(inc.root_index(), std::optional<std::size_t>(0U));
  REQUIRE_EQ(dec.root_index(), std::optional<std::size_t>(size - 1U));
  REQUIRE_EQ(inc.euler_size(), 2U * size - 1U);
  REQUIRE_EQ(dec.euler_size(), 2U * size - 1U);
  REQUIRE_EQ(inc.range_min_index(333U, 3888U), 333U);
  REQUIRE_EQ(dec.range_min_index(333U, 3888U), 3887U);
}

TEST_CASE(cartesian_tree_rmq_randomized_differential) {
  std::mt19937_64 rng(0xCA471E51ULL);
  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const std::size_t size = static_cast<std::size_t>(rng() % 257U);
    std::vector<std::int64_t> values(size);
    for (std::int64_t& value : values) {
      value = static_cast<std::int64_t>(rng() % 41U) - 20;
    }

    CartesianTreeRmq index(values);
    REQUIRE(index.valid_structure());
    if (size == 0U) {
      continue;
    }

    REQUIRE_EQ(index.euler_size(), 2U * size - 1U);
    const std::size_t block_size = index.micro_block_size();
    REQUIRE(block_size >= 1U);
    if (block_size < 64U) {
      const std::size_t full_type_bound =
          std::size_t{1} << (block_size == 1U ? 0U : block_size - 1U);
      REQUIRE(index.micro_type_count() <= full_type_bound + 1U);
    }

    for (std::size_t query = 0U; query < 120U; ++query) {
      std::size_t begin = static_cast<std::size_t>(rng() % size);
      std::size_t end = static_cast<std::size_t>(rng() % size);
      if (begin > end) {
        std::swap(begin, end);
      }
      ++end;
      const std::size_t expected =
          cartesian_rmq_naive_index(values, begin, end);
      REQUIRE_EQ(index.range_min_index(begin, end), expected);
      REQUIRE_EQ(index.range_min(begin, end), values[expected]);
    }
  }
}

}  // namespace
