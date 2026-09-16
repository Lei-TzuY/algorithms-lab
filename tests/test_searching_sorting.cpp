#include "test_framework.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <random>
#include <span>
#include <stdexcept>
#include <vector>

#include "algorithms/optimization/smawk.hpp"
#include "algorithms/searching/binary_search.hpp"
#include "algorithms/sorting/ford_johnson.hpp"
#include "algorithms/sorting/merge_sort.hpp"
#include "algorithms/sorting/quick_sort.hpp"

using algorithms::searching::binary_search_index;
using algorithms::sorting::merge_sort;
using algorithms::sorting::quick_sort;

TEST_CASE(binary_search_boundaries_and_duplicates) {
  const std::vector<int> empty;
  REQUIRE(!binary_search_index<int>(std::span<const int>{empty}, 4).has_value());

  const std::vector<int> singleton{7};
  REQUIRE_EQ(*binary_search_index<int>(std::span<const int>{singleton}, 7),
             std::size_t{0});
  REQUIRE(!binary_search_index<int>(std::span<const int>{singleton}, 6).has_value());

  const std::vector<int> values{-5, -1, 0, 0, 0, 8, 99};
  REQUIRE_EQ(*binary_search_index<int>(std::span<const int>{values}, -5),
             std::size_t{0});
  REQUIRE_EQ(*binary_search_index<int>(std::span<const int>{values}, 0),
             std::size_t{2});
  REQUIRE_EQ(*binary_search_index<int>(std::span<const int>{values}, 99),
             std::size_t{6});
  REQUIRE(!binary_search_index<int>(std::span<const int>{values}, -6).has_value());
  REQUIRE(!binary_search_index<int>(std::span<const int>{values}, 100).has_value());
  REQUIRE(!binary_search_index<int>(std::span<const int>{values}, 7).has_value());
}

namespace {

using SmawkMatrix = std::vector<std::vector<std::int64_t>>;

void require_both_sorts(std::vector<int> input) {
  auto expected = input;
  std::sort(expected.begin(), expected.end());

  auto merge_values = input;
  merge_sort(merge_values);
  REQUIRE_EQ(merge_values, expected);

  auto quick_values = std::move(input);
  quick_sort(quick_values);
  REQUIRE_EQ(quick_values, expected);
}

std::size_t ford_johnson_classical_bound(std::size_t count) {
  std::size_t total = 0U;
  for (std::size_t k = 1U; k <= count; ++k) {
    const std::size_t target = k - k / 4U;  // ceil(3k/4)
    std::size_t power = 1U;
    std::size_t term = 0U;
    while (power < target) {
      power *= 2U;
      ++term;
    }
    total += term;
  }
  return total;
}

std::vector<std::size_t> naive_smawk_row_minima(const SmawkMatrix& matrix) {
  std::vector<std::size_t> result;
  result.reserve(matrix.size());
  for (const auto& row : matrix) {
    std::size_t best = 0U;
    for (std::size_t column = 1U; column < row.size(); ++column) {
      if (row[column] < row[best]) {
        best = column;
      }
    }
    result.push_back(best);
  }
  return result;
}

SmawkMatrix monge_from_pivots(const std::vector<std::int64_t>& pivots,
                              std::size_t columns,
                              const std::vector<std::int64_t>& row_bias) {
  SmawkMatrix matrix(pivots.size(), std::vector<std::int64_t>(columns));
  for (std::size_t row = 0U; row < pivots.size(); ++row) {
    for (std::size_t column = 0U; column < columns; ++column) {
      const std::int64_t coordinate = static_cast<std::int64_t>(2U * column);
      const std::int64_t delta = coordinate - pivots[row];
      matrix[row][column] = delta * delta + row_bias[row];
    }
  }
  return matrix;
}

}  // namespace

TEST_CASE(sorts_edge_cases_and_adversarial_shapes) {
  require_both_sorts({});
  require_both_sorts({1});
  require_both_sorts({2, 1});
  require_both_sorts({1, 2, 3, 4, 5, 6});
  require_both_sorts({6, 5, 4, 3, 2, 1});
  require_both_sorts({4, 4, 4, 4, 4});
  require_both_sorts({3, -1, 3, 0, -1, 2, 2});
}

TEST_CASE(sorts_randomized_differential_against_std_sort) {
  std::mt19937 rng(0xA17C0DEu);
  std::uniform_int_distribution<int> length_dist(0, 250);
  std::uniform_int_distribution<int> value_dist(-50, 50);

  for (int trial = 0; trial < 500; ++trial) {
    const int length = length_dist(rng);
    std::vector<int> values;
    values.reserve(static_cast<std::size_t>(length));
    for (int i = 0; i < length; ++i) {
      values.push_back(value_dist(rng));
    }
    require_both_sorts(values);
  }
}

TEST_CASE(ford_johnson_edge_cases_duplicates_and_adversarial_count) {
  using algorithms::sorting::ford_johnson_sort;

  REQUIRE_EQ(ford_johnson_sort({}).comparisons, std::size_t{0});
  const std::array<std::int64_t, 1> singleton{{42}};
  REQUIRE_EQ(ford_johnson_sort(singleton).values,
             (std::vector<std::int64_t>{42}));

  const std::array<std::int64_t, 8> duplicates{{
      std::numeric_limits<std::int64_t>::max(), 0, -7, 0,
      std::numeric_limits<std::int64_t>::min(), -7, 5, 5,
  }};
  auto expected = std::vector<std::int64_t>(duplicates.begin(), duplicates.end());
  std::sort(expected.begin(), expected.end());
  const auto result = ford_johnson_sort(duplicates);
  REQUIRE_EQ(result.values, expected);
  REQUIRE(result.comparisons <= ford_johnson_classical_bound(duplicates.size()));

  const std::array<std::int64_t, 9> adversarial{{0, 1, 2, 5, 3, 4, 6, 8, 7}};
  const auto adversarial_result = ford_johnson_sort(adversarial);
  REQUIRE_EQ(adversarial_result.values,
             (std::vector<std::int64_t>{0, 1, 2, 3, 4, 5, 6, 7, 8}));
  REQUIRE_EQ(adversarial_result.comparisons, std::size_t{19});
}

TEST_CASE(ford_johnson_exhaustive_small_permutation_comparison_bounds) {
  using algorithms::sorting::ford_johnson_sort;
  constexpr std::array<std::size_t, 9> expected_worst{{
      0U, 0U, 1U, 3U, 5U, 7U, 10U, 13U, 16U,
  }};

  for (std::size_t size = 0U; size <= 8U; ++size) {
    std::vector<std::int64_t> permutation(size);
    for (std::size_t index = 0U; index < size; ++index) {
      permutation[index] = static_cast<std::int64_t>(index);
    }
    std::size_t worst = 0U;
    do {
      const auto result = ford_johnson_sort(permutation);
      std::vector<std::int64_t> sorted(size);
      for (std::size_t index = 0U; index < size; ++index) {
        sorted[index] = static_cast<std::int64_t>(index);
      }
      REQUIRE_EQ(result.values, sorted);
      REQUIRE(result.comparisons <= ford_johnson_classical_bound(size));
      worst = std::max(worst, result.comparisons);
    } while (std::next_permutation(permutation.begin(), permutation.end()));
    REQUIRE_EQ(worst, expected_worst[size]);
  }
}

TEST_CASE(ford_johnson_randomized_differential_and_determinism) {
  using algorithms::sorting::ford_johnson_sort;

  std::mt19937_64 random(0x464F52444A4F484EULL);
  for (std::size_t trial = 0U; trial < 1500U; ++trial) {
    const auto size = static_cast<std::size_t>(random() % 129U);
    std::vector<std::int64_t> values(size);
    for (auto& value : values) {
      value = static_cast<std::int64_t>(random() % 101U) - 50;
    }
    auto expected = values;
    std::sort(expected.begin(), expected.end());

    const auto first = ford_johnson_sort(values);
    const auto second = ford_johnson_sort(values);
    REQUIRE_EQ(first, second);
    REQUIRE_EQ(first.values, expected);
    REQUIRE(first.comparisons <= ford_johnson_classical_bound(size));
  }
}

TEST_CASE(smawk_shape_and_leftmost_ties) {
  using algorithms::optimization::smawk_row_minima;
  REQUIRE(smawk_row_minima({}).empty());
  REQUIRE_THROWS_AS(smawk_row_minima(SmawkMatrix{{}}), std::invalid_argument);
  REQUIRE_THROWS_AS(smawk_row_minima(SmawkMatrix{{1, 2}, {3}}),
                    std::invalid_argument);

  const SmawkMatrix tied =
      monge_from_pivots({1, 3, 5, 7}, 5U, {0, -4, 9, 1});
  REQUIRE_EQ(smawk_row_minima(tied),
             std::vector<std::size_t>({0U, 1U, 2U, 3U}));
  REQUIRE_EQ(smawk_row_minima(SmawkMatrix{{9, -4, -4, 7}}),
             std::vector<std::size_t>({1U}));
  REQUIRE_EQ(smawk_row_minima(SmawkMatrix{{5}, {-7}, {8}, {-9}}),
             std::vector<std::size_t>({0U, 0U, 0U, 0U}));
}

TEST_CASE(smawk_randomized_monge_differential) {
  using algorithms::optimization::smawk_row_minima;
  std::mt19937_64 rng(0x5A6D4BULL);
  std::uniform_int_distribution<std::size_t> row_count_dist(1U, 90U);
  std::uniform_int_distribution<std::size_t> column_count_dist(1U, 90U);
  std::uniform_int_distribution<std::int64_t> bias_dist(-1000, 1000);

  for (std::size_t trial = 0U; trial < 1200U; ++trial) {
    const std::size_t rows = row_count_dist(rng);
    const std::size_t columns = column_count_dist(rng);
    const std::int64_t max_pivot =
        static_cast<std::int64_t>(2U * (columns - 1U));
    std::uniform_int_distribution<std::int64_t> pivot_dist(0, max_pivot);

    std::vector<std::int64_t> pivots(rows);
    for (auto& pivot : pivots) {
      pivot = pivot_dist(rng);
    }
    std::sort(pivots.begin(), pivots.end());

    std::vector<std::int64_t> row_bias(rows);
    for (auto& bias : row_bias) {
      bias = bias_dist(rng);
    }

    const SmawkMatrix matrix = monge_from_pivots(pivots, columns, row_bias);
    const auto expected = naive_smawk_row_minima(matrix);
    const auto actual = smawk_row_minima(matrix);
    REQUIRE_EQ(actual, expected);
    REQUIRE(std::is_sorted(actual.begin(), actual.end()));
  }
}
