#pragma once

#include "algorithms/data_structures/disjoint_sparse_table.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using algorithms::data_structures::DisjointSparseTable;

struct CountingConcat {
  std::shared_ptr<std::size_t> calls;
  std::string operator()(const std::string& left, const std::string& right) const {
    ++*calls;
    return left + right;
  }
};

TEST_CASE(disjoint_sparse_table_validates_ranges_and_singletons) {
  const DisjointSparseTable<std::int64_t> empty({});
  REQUIRE_EQ(empty.size(), 0U);
  REQUIRE_THROWS_AS(empty.query(0U, 1U), std::out_of_range);

  const DisjointSparseTable<std::int64_t> one({-7});
  REQUIRE_EQ(one.size(), 1U);
  REQUIRE_EQ(one.query(0U, 1U), -7);
  REQUIRE_THROWS_AS(one.query(0U, 0U), std::out_of_range);
  REQUIRE_THROWS_AS(one.query(1U, 1U), std::out_of_range);
  REQUIRE_THROWS_AS(one.query(0U, 2U), std::out_of_range);
}

TEST_CASE(disjoint_sparse_table_preserves_noncommutative_order) {
  const DisjointSparseTable<std::string> table({"a", "bc", "d", "EF", "g"});
  REQUIRE_EQ(table.query(0U, 5U), std::string("abcdEFg"));
  REQUIRE_EQ(table.query(1U, 4U), std::string("bcdEF"));
  REQUIRE_EQ(table.query(2U, 5U), std::string("dEFg"));
  REQUIRE_EQ(table.query(3U, 4U), std::string("EF"));
}

TEST_CASE(disjoint_sparse_table_non_singleton_query_uses_one_combine) {
  auto calls = std::make_shared<std::size_t>(0U);
  DisjointSparseTable<std::string, CountingConcat> table(
      {"0", "1", "2", "3", "4", "5", "6", "7", "8"}, CountingConcat{calls});

  *calls = 0U;
  REQUIRE_EQ(table.query(1U, 8U), std::string("1234567"));
  REQUIRE_EQ(*calls, 1U);

  *calls = 0U;
  REQUIRE_EQ(table.query(4U, 5U), std::string("4"));
  REQUIRE_EQ(*calls, 0U);
}

TEST_CASE(disjoint_sparse_table_randomized_sum_differential) {
  std::mt19937_64 rng(0xD15A01A7ULL);
  std::uniform_int_distribution<std::size_t> length_distribution(0U, 160U);
  std::uniform_int_distribution<std::int64_t> value_distribution(-1000, 1000);

  for (std::size_t trial = 0; trial < 500U; ++trial) {
    const std::size_t length = length_distribution(rng);
    std::vector<std::int64_t> values(length);
    for (auto& value : values) {
      value = value_distribution(rng);
    }

    const DisjointSparseTable<std::int64_t> table(values);
    REQUIRE_EQ(table.size(), values.size());

    if (values.empty()) {
      continue;
    }

    std::uniform_int_distribution<std::size_t> begin_distribution(0U, values.size() - 1U);
    for (std::size_t query_index = 0; query_index < 180U; ++query_index) {
      const std::size_t begin = begin_distribution(rng);
      std::uniform_int_distribution<std::size_t> end_distribution(begin + 1U,
                                                                  values.size());
      const std::size_t end = end_distribution(rng);
      std::int64_t expected = 0;
      for (std::size_t index = begin; index < end; ++index) {
        expected += values[index];
      }
      REQUIRE_EQ(table.query(begin, end), expected);
    }
  }
}

}  // namespace
