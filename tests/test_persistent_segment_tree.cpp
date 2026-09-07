#include "algorithms/data_structures/persistent_segment_tree.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::data_structures::PersistentSegmentTree;

std::int64_t oracle_sum(const std::vector<std::int64_t>& values,
                        std::size_t begin, std::size_t end) {
  // Randomized differential values are bounded to +/-10,000 with n=31,
  // so this independent linear oracle is mathematically bounded by 310,000.
  std::int64_t sum = 0;
  for (std::size_t i = begin; i < end; ++i) {
    sum += values[i];
  }
  return sum;
}

TEST_CASE(persistent_segment_tree_empty_and_validation) {
  PersistentSegmentTree tree(0);
  REQUIRE_EQ(tree.size(), 0U);
  REQUIRE_EQ(tree.version_count(), 1U);
  REQUIRE_EQ(tree.node_count(), 0U);
  REQUIRE_EQ(tree.range_sum(0, 0, 0), 0);
  REQUIRE_THROWS_AS(tree.assign(0, 0, 7), std::out_of_range);
  REQUIRE_THROWS_AS(tree.range_sum(1, 0, 0), std::out_of_range);
  REQUIRE_THROWS_AS(tree.range_sum(0, 0, 1), std::out_of_range);
}

TEST_CASE(persistent_segment_tree_rejects_invalid_nonempty_version_and_range) {
  PersistentSegmentTree tree(std::vector<std::int64_t>{1, 2, 3});
  REQUIRE_THROWS_AS(tree.assign(1, 0, 9), std::out_of_range);
  REQUIRE_THROWS_AS(tree.range_sum(1, 0, 1), std::out_of_range);
  REQUIRE_THROWS_AS(tree.range_sum(0, 2, 1), std::out_of_range);
  REQUIRE_THROWS_AS(tree.range_sum(0, 0, 4), std::out_of_range);
}

TEST_CASE(persistent_segment_tree_versions_are_immutable_and_branchable) {
  PersistentSegmentTree tree(std::vector<std::int64_t>{1, 2, 3, 4});
  const auto base_nodes = tree.node_count();
  const auto v1 = tree.assign(0, 1, 20);
  const auto nodes_after_v1 = tree.node_count();
  const auto v2 = tree.assign(0, 2, 30);
  REQUIRE_EQ(v1, 1U);
  REQUIRE_EQ(v2, 2U);
  REQUIRE_EQ(tree.version_count(), 3U);
  REQUIRE(nodes_after_v1 > base_nodes);
  REQUIRE(tree.node_count() > nodes_after_v1);
  REQUIRE_EQ(tree.range_sum(0, 0, 4), 10);
  REQUIRE_EQ(tree.range_sum(v1, 0, 4), 28);
  REQUIRE_EQ(tree.range_sum(v2, 0, 4), 37);
  REQUIRE_EQ(tree.range_sum(v1, 1, 3), 23);
  REQUIRE_EQ(tree.range_sum(v2, 1, 3), 32);
}

TEST_CASE(persistent_segment_tree_update_path_uses_logarithmic_new_nodes) {
  PersistentSegmentTree tree(17);
  const auto before = tree.node_count();
  static_cast<void>(tree.assign(0, 8, 5));
  const auto created = tree.node_count() - before;
  // A root-to-leaf path in a 17-element binary interval decomposition has
  // at most ceil(log2(17)) + 1 = 6 nodes.
  REQUIRE(created <= 6U);
  REQUIRE(created >= 5U);
}

TEST_CASE(persistent_segment_tree_failed_update_is_transactional) {
  PersistentSegmentTree tree(std::vector<std::int64_t>{
      std::numeric_limits<std::int64_t>::max(), 0});
  const auto versions_before = tree.version_count();
  const auto nodes_before = tree.node_count();
  REQUIRE_THROWS_AS(tree.assign(0, 1, 1), std::overflow_error);
  REQUIRE_EQ(tree.version_count(), versions_before);
  REQUIRE_EQ(tree.node_count(), nodes_before);
  REQUIRE_EQ(tree.range_sum(0, 0, 2),
             std::numeric_limits<std::int64_t>::max());
}

TEST_CASE(persistent_segment_tree_query_preserves_representable_cancellation) {
  const auto hi = std::numeric_limits<std::int64_t>::max();
  PersistentSegmentTree tree(std::vector<std::int64_t>{hi, -hi, hi, -hi});
  REQUIRE_EQ(tree.range_sum(0, 0, 4), 0);
  REQUIRE_EQ(tree.range_sum(0, 0, 3), hi);
  REQUIRE_EQ(tree.range_sum(0, 1, 4), -hi);
}

TEST_CASE(persistent_segment_tree_randomized_branching_differential) {
  constexpr std::size_t n = 31;
  std::vector<std::int64_t> initial(n, 0);
  PersistentSegmentTree tree(initial);
  std::vector<std::vector<std::int64_t>> versions{initial};
  std::mt19937_64 rng(0xA14F5EEDULL);
  std::uniform_int_distribution<std::int64_t> value_dist(-10000, 10000);
  std::uniform_int_distribution<std::size_t> index_dist(0, n - 1);

  for (int step = 0; step < 3000; ++step) {
    std::uniform_int_distribution<std::size_t> version_dist(0,
                                                            versions.size() - 1);
    const std::size_t base = version_dist(rng);
    const std::size_t index = index_dist(rng);
    const std::int64_t value = value_dist(rng);
    auto expected = versions[base];
    expected[index] = value;

    const auto nodes_before = tree.node_count();
    const auto created_version = tree.assign(base, index, value);
    versions.push_back(expected);
    REQUIRE_EQ(created_version, versions.size() - 1);
    REQUIRE_EQ(tree.version_count(), versions.size());
    REQUIRE(tree.node_count() > nodes_before);
    REQUIRE(tree.node_count() - nodes_before <= 6U);

    for (int query = 0; query < 5; ++query) {
      std::uniform_int_distribution<std::size_t> query_version_dist(
          0, versions.size() - 1);
      const std::size_t version = query_version_dist(rng);
      const std::size_t a = index_dist(rng);
      const std::size_t b = index_dist(rng);
      const std::size_t begin = std::min(a, b);
      const std::size_t end = std::max(a, b) + 1;
      REQUIRE_EQ(tree.range_sum(version, begin, end),
                 oracle_sum(versions[version], begin, end));
    }
  }

  for (std::size_t version = 0; version < versions.size(); version += 137) {
    REQUIRE_EQ(tree.range_sum(version, 0, n),
               oracle_sum(versions[version], 0, n));
  }
}

}  // namespace
