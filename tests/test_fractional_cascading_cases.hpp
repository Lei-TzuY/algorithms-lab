#pragma once

#include "algorithms/data_structures/fractional_cascading.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace fractional_cascading_test_detail {

using Index = algorithms::data_structures::FractionalCascadingIndex;
using Value = Index::Value;

std::vector<std::size_t> oracle_lower_bounds(
    const std::vector<std::vector<Value>>& catalogs, Value key) {
  std::vector<std::size_t> result;
  result.reserve(catalogs.size());
  for (const auto& catalog : catalogs) {
    const auto it = std::lower_bound(catalog.begin(), catalog.end(), key);
    result.push_back(static_cast<std::size_t>(it - catalog.begin()));
  }
  return result;
}

}  // namespace fractional_cascading_test_detail

TEST_CASE(fractional_cascading_validation_and_boundaries) {
  using fractional_cascading_test_detail::Index;
  using Value = Index::Value;

  REQUIRE_THROWS_AS(Index({{Value{1}, Value{3}, Value{2}}}), std::invalid_argument);

  const std::vector<std::vector<Value>> catalogs{
      {},
      {-5, -5, 0, 4, 9},
      {-7, -1, -1, 2, 2, 2, 10},
      {std::numeric_limits<Value>::min(), 0, std::numeric_limits<Value>::max()},
  };
  const Index index(catalogs);
  REQUIRE(index.valid_structure());
  REQUIRE_EQ(index.catalog_count(), catalogs.size());
  for (const Value key : {std::numeric_limits<Value>::min(), Value{-8}, Value{-5},
                          Value{-1}, Value{2}, Value{3}, Value{10},
                          std::numeric_limits<Value>::max()}) {
    REQUIRE_EQ(index.lower_bound_indices(key),
               fractional_cascading_test_detail::oracle_lower_bounds(catalogs, key));
  }
}

TEST_CASE(fractional_cascading_empty_and_sparse_catalog_chains) {
  using fractional_cascading_test_detail::Index;
  using Value = Index::Value;

  const Index none({});
  REQUIRE(none.valid_structure());
  REQUIRE(none.lower_bound_indices(0).empty());

  const std::vector<std::vector<Value>> catalogs{{}, {}, {1}, {}, {2, 8}, {}, {9}};
  const Index index(catalogs);
  REQUIRE(index.valid_structure());
  for (Value key = 0; key <= 10; ++key) {
    REQUIRE_EQ(index.lower_bound_indices(key),
               fractional_cascading_test_detail::oracle_lower_bounds(catalogs, key));
  }
}

TEST_CASE(fractional_cascading_bridge_uses_single_predecessor_correction) {
  using fractional_cascading_test_detail::Index;
  using Value = Index::Value;

  const std::vector<std::vector<Value>> catalogs{{100}, {1, 2, 3, 4}, {0, 10, 20, 30, 40}};
  const Index index(catalogs);
  REQUIRE(index.valid_structure());
  for (const Value key : {Value{0}, Value{1}, Value{3}, Value{9}, Value{19}, Value{31}, Value{101}}) {
    REQUIRE_EQ(index.lower_bound_indices(key),
               fractional_cascading_test_detail::oracle_lower_bounds(catalogs, key));
  }
}

TEST_CASE(fractional_cascading_duplicate_heavy_multisets) {
  using fractional_cascading_test_detail::Index;
  using Value = Index::Value;

  const std::vector<std::vector<Value>> catalogs{
      {1, 1, 1, 1, 5, 5},
      {1, 1, 2, 2, 2, 5, 5, 5},
      {0, 1, 1, 1, 1, 1, 6},
      {1, 1, 1},
  };
  const Index index(catalogs);
  REQUIRE(index.valid_structure());
  for (Value key = -1; key <= 7; ++key) {
    REQUIRE_EQ(index.lower_bound_indices(key),
               fractional_cascading_test_detail::oracle_lower_bounds(catalogs, key));
  }
}

TEST_CASE(fractional_cascading_randomized_differential_and_space_bound) {
  using fractional_cascading_test_detail::Index;
  using Value = Index::Value;

  std::mt19937_64 rng(0xFCA5CADEULL);
  std::uniform_int_distribution<int> catalog_count_dist(0, 10);
  std::uniform_int_distribution<int> length_dist(0, 32);
  std::uniform_int_distribution<int> value_dist(-50, 50);
  std::uniform_int_distribution<int> query_dist(-60, 60);

  for (std::size_t trial = 0; trial < 900U; ++trial) {
    const int catalog_count = catalog_count_dist(rng);
    std::vector<std::vector<Value>> catalogs(
        static_cast<std::size_t>(catalog_count));
    std::size_t original_entries = 0U;
    for (auto& catalog : catalogs) {
      const int length = length_dist(rng);
      catalog.reserve(static_cast<std::size_t>(length));
      for (int i = 0; i < length; ++i) {
        catalog.push_back(static_cast<Value>(value_dist(rng)));
      }
      std::sort(catalog.begin(), catalog.end());
      original_entries += catalog.size();
    }

    const Index index(catalogs);
    REQUIRE(index.valid_structure());
    REQUIRE(index.total_augmented_entries() <= original_entries * 2U);
    const auto sizes = index.augmented_sizes();
    REQUIRE_EQ(sizes.size(), catalogs.size());

    for (std::size_t query = 0; query < 80U; ++query) {
      const Value key = static_cast<Value>(query_dist(rng));
      REQUIRE_EQ(index.lower_bound_indices(key),
                 fractional_cascading_test_detail::oracle_lower_bounds(catalogs, key));
    }
  }
}

TEST_CASE(fractional_cascading_input_snapshot_is_immutable) {
  using fractional_cascading_test_detail::Index;
  using Value = Index::Value;

  std::vector<std::vector<Value>> catalogs{{1, 3, 5}, {2, 4, 6}};
  const Index index(catalogs);
  catalogs[0][0] = 100;
  REQUIRE_EQ(index.lower_bound_indices(3), (std::vector<std::size_t>{1U, 1U}));
  REQUIRE(index.valid_structure());
}
