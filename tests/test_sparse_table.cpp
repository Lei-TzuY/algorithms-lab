#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

#include "algorithms/data_structures/sparse_table.hpp"

namespace {

using algorithms::data_structures::SparseTableMin;
using algorithms::data_structures::SparseTableMinResult;

SparseTableMinResult naive_min(const std::vector<std::int64_t>& values,
                               std::size_t begin, std::size_t end) {
  SparseTableMinResult result{values[begin], begin};
  for (std::size_t index = begin + 1U; index < end; ++index) {
    if (values[index] < result.value) {
      result = SparseTableMinResult{values[index], index};
    }
  }
  return result;
}

void require_same(const SparseTableMinResult& actual,
                  const SparseTableMinResult& expected) {
  REQUIRE_EQ(actual.value, expected.value);
  REQUIRE_EQ(actual.index, expected.index);
}

}  // namespace

TEST_CASE(sparse_table_answers_all_ranges_with_leftmost_argmin) {
  const std::vector<std::int64_t> values{4, 2, 2, 9, -1, -1, 7};
  SparseTableMin table(values);
  REQUIRE_EQ(table.size(), values.size());

  for (std::size_t begin = 0; begin < values.size(); ++begin) {
    for (std::size_t end = begin + 1U; end <= values.size(); ++end) {
      require_same(table.range_min(begin, end), naive_min(values, begin, end));
    }
  }

  const SparseTableMinResult tied = table.range_min(1, 3);
  REQUIRE_EQ(tied.value, std::int64_t{2});
  REQUIRE_EQ(tied.index, std::size_t{1});
}

TEST_CASE(sparse_table_validates_empty_and_invalid_ranges) {
  SparseTableMin empty(std::vector<std::int64_t>{});
  REQUIRE_EQ(empty.size(), std::size_t{0});
  REQUIRE_THROWS_AS(empty.range_min(0, 0), std::out_of_range);

  SparseTableMin table(std::vector<std::int64_t>{3, 1, 4});
  REQUIRE_THROWS_AS(table.range_min(0, 0), std::out_of_range);
  REQUIRE_THROWS_AS(table.range_min(2, 1), std::out_of_range);
  REQUIRE_THROWS_AS(table.range_min(0, 4), std::out_of_range);
}

TEST_CASE(sparse_table_preserves_snapshot_and_signed_extremes) {
  const std::int64_t minimum = std::numeric_limits<std::int64_t>::min();
  const std::int64_t maximum = std::numeric_limits<std::int64_t>::max();
  std::vector<std::int64_t> values{maximum, 0, minimum, minimum, 5};
  SparseTableMin table(values);

  values.assign(values.size(), maximum);
  const SparseTableMinResult result = table.range_min(0, 5);
  REQUIRE_EQ(result.value, minimum);
  REQUIRE_EQ(result.index, std::size_t{2});
  require_same(table.range_min(3, 5), SparseTableMinResult{minimum, 3});
}

TEST_CASE(sparse_table_matches_exhaustive_naive_randomized_ranges) {
  std::mt19937_64 rng(0x535041525345ULL);
  std::uniform_int_distribution<int> size_distribution(0, 96);
  std::uniform_int_distribution<int> value_distribution(-1000, 1000);

  for (std::size_t trial = 0; trial < 260; ++trial) {
    const std::size_t size =
        static_cast<std::size_t>(size_distribution(rng));
    std::vector<std::int64_t> values;
    values.reserve(size);
    for (std::size_t index = 0; index < size; ++index) {
      values.push_back(static_cast<std::int64_t>(value_distribution(rng)));
    }

    SparseTableMin table(values);
    REQUIRE_EQ(table.size(), size);
    if (size == 0U) {
      REQUIRE_THROWS_AS(table.range_min(0, 0), std::out_of_range);
      continue;
    }

    for (std::size_t begin = 0; begin < size; ++begin) {
      for (std::size_t end = begin + 1U; end <= size; ++end) {
        require_same(table.range_min(begin, end),
                     naive_min(values, begin, end));
      }
    }
  }
}
