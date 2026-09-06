#include "test_framework.hpp"

#include <algorithm>
#include <functional>
#include <random>
#include <span>
#include <vector>

#include "algorithms/searching/binary_search.hpp"
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
