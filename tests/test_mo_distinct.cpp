#include "algorithms/offline/mo_distinct.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <set>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
using algorithms::offline::RangeQuery;
using algorithms::offline::mo_distinct_counts;

std::vector<std::size_t> naive_counts(
    std::span<const std::int64_t> values,
    std::span<const RangeQuery> queries) {
  std::vector<std::size_t> result;
  result.reserve(queries.size());
  for (const RangeQuery query : queries) {
    std::set<std::int64_t> distinct;
    for (std::size_t index = query.begin; index < query.end; ++index) {
      distinct.insert(values[index]);
    }
    result.push_back(distinct.size());
  }
  return result;
}

TEST_CASE(mo_distinct_empty_and_validation) {
  const std::vector<std::int64_t> empty;
  const std::vector<RangeQuery> no_queries;
  REQUIRE(mo_distinct_counts(empty, no_queries).empty());

  const std::vector<RangeQuery> empty_query{{0U, 0U}};
  REQUIRE_EQ(mo_distinct_counts(empty, empty_query),
             std::vector<std::size_t>({0U}));

  const std::vector<std::int64_t> values{1, 2, 3};
  const std::vector<RangeQuery> reversed{{2U, 1U}};
  const std::vector<RangeQuery> past_end{{0U, 4U}};
  REQUIRE_THROWS_AS(mo_distinct_counts(values, reversed), std::invalid_argument);
  REQUIRE_THROWS_AS(mo_distinct_counts(values, past_end), std::out_of_range);
}

TEST_CASE(mo_distinct_deterministic_shapes_and_extremes) {
  const std::vector<std::int64_t> values{
      std::numeric_limits<std::int64_t>::min(), 7, 7, 3,
      std::numeric_limits<std::int64_t>::max(), 3, 7};
  const std::vector<RangeQuery> queries{{0U, 7U}, {1U, 3U}, {3U, 6U},
                                        {4U, 4U}, {6U, 7U}, {0U, 1U}};
  const std::vector<std::size_t> expected{4U, 1U, 2U, 0U, 1U, 1U};
  REQUIRE_EQ(mo_distinct_counts(values, queries), expected);
  REQUIRE_EQ(mo_distinct_counts(values, queries), expected);
}

TEST_CASE(mo_distinct_query_order_is_external_only) {
  const std::vector<std::int64_t> values{5, 1, 5, 2, 3, 2, 1, 4};
  std::vector<RangeQuery> queries{{0U, 8U}, {2U, 6U}, {1U, 7U}, {3U, 3U},
                                  {6U, 8U}, {0U, 4U}};
  const auto expected = naive_counts(values, queries);
  REQUIRE_EQ(mo_distinct_counts(values, queries), expected);

  std::reverse(queries.begin(), queries.end());
  REQUIRE_EQ(mo_distinct_counts(values, queries), naive_counts(values, queries));
}

TEST_CASE(mo_distinct_randomized_differential) {
  std::mt19937_64 random(0x4D4F5F4449535449ULL);
  std::uniform_int_distribution<int> length_distribution(0, 96);
  std::uniform_int_distribution<int> query_count_distribution(0, 128);
  std::uniform_int_distribution<int> value_distribution(-12, 12);

  for (std::size_t trial = 0; trial < 1200U; ++trial) {
    const std::size_t length = static_cast<std::size_t>(length_distribution(random));
    std::vector<std::int64_t> values(length);
    for (std::size_t index = 0; index < length; ++index) {
      const std::uint64_t selector = random() % 97U;
      if (selector == 0U) {
        values[index] = std::numeric_limits<std::int64_t>::min();
      } else if (selector == 1U) {
        values[index] = std::numeric_limits<std::int64_t>::max();
      } else {
        values[index] = static_cast<std::int64_t>(value_distribution(random));
      }
    }

    const std::size_t query_count =
        static_cast<std::size_t>(query_count_distribution(random));
    std::vector<RangeQuery> queries;
    queries.reserve(query_count);
    std::uniform_int_distribution<std::size_t> endpoint_distribution(0U, length);
    for (std::size_t query_index = 0; query_index < query_count; ++query_index) {
      std::size_t first = endpoint_distribution(random);
      std::size_t second = endpoint_distribution(random);
      if (first > second) {
        std::swap(first, second);
      }
      queries.push_back(RangeQuery{first, second});
    }

    REQUIRE_EQ(mo_distinct_counts(values, queries), naive_counts(values, queries));

    std::shuffle(queries.begin(), queries.end(), random);
    REQUIRE_EQ(mo_distinct_counts(values, queries), naive_counts(values, queries));
  }
}

}  // namespace
