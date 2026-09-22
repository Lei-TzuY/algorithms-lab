#pragma once

#include "algorithms/dynamic_programming/ordered_tree_edit_distance.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

using algorithms::dynamic_programming::OrderedRootedTree;
using algorithms::dynamic_programming::OrderedTreeNode;
using algorithms::dynamic_programming::ordered_tree_edit_distance;

namespace ordered_tree_edit_distance_test_detail {

struct RelationTable {
  std::vector<std::vector<bool>> ancestor;
  std::vector<std::vector<bool>> left_of;
};

inline RelationTable relations(const OrderedRootedTree& tree) {
  const std::size_t n = tree.nodes.size();
  RelationTable out{
      std::vector<std::vector<bool>>(n, std::vector<bool>(n, false)),
      std::vector<std::vector<bool>>(n, std::vector<bool>(n, false))};

  if (n == 0U) {
    return out;
  }

  std::vector<std::size_t> preorder(n, 0U);
  std::vector<std::size_t> subtree_end(n, 0U);
  std::size_t timer = 0U;

  const std::function<void(std::size_t)> visit =
      [&](const std::size_t node) {
        preorder[node] = timer++;
        for (const std::size_t child : tree.nodes[node].children) {
          visit(child);
        }
        subtree_end[node] = timer;
      };

  visit(*tree.root);

  for (std::size_t first = 0U; first < n; ++first) {
    for (std::size_t second = 0U; second < n; ++second) {
      if (first == second) {
        continue;
      }
      out.ancestor[first][second] =
          preorder[first] < preorder[second] &&
          preorder[second] < subtree_end[first];
      out.left_of[first][second] =
          subtree_end[first] <= preorder[second];
    }
  }
  return out;
}

// Independent exact oracle via the mapping characterization of ordered-tree
// edit distance. It does not use postorder, keyroots, or forest DP.
inline std::size_t brute_mapping_distance(
    const OrderedRootedTree& source,
    const OrderedRootedTree& target) {
  const std::size_t n = source.nodes.size();
  const std::size_t m = target.nodes.size();
  const RelationTable source_rel = relations(source);
  const RelationTable target_rel = relations(target);

  std::vector<bool> target_used(m, false);
  std::vector<std::pair<std::size_t, std::size_t>> mapping;
  std::size_t best = n + m;

  const auto compatible =
      [&](const std::size_t source_node,
          const std::size_t target_node) {
        for (const auto& [old_source, old_target] : mapping) {
          if (source_rel.ancestor[source_node][old_source] !=
                  target_rel.ancestor[target_node][old_target] ||
              source_rel.ancestor[old_source][source_node] !=
                  target_rel.ancestor[old_target][target_node] ||
              source_rel.left_of[source_node][old_source] !=
                  target_rel.left_of[target_node][old_target] ||
              source_rel.left_of[old_source][source_node] !=
                  target_rel.left_of[old_target][target_node]) {
            return false;
          }
        }
        return true;
      };

  const std::function<void(std::size_t, std::size_t)> enumerate =
      [&](const std::size_t source_node,
          const std::size_t relabels) {
        if (source_node == n) {
          const std::size_t mapped = mapping.size();
          const std::size_t cost =
              (n - mapped) + (m - mapped) + relabels;
          best = std::min(best, cost);
          return;
        }

        // Leave this source node unmapped: one deletion in the mapping model.
        enumerate(source_node + 1U, relabels);

        for (std::size_t target_node = 0U;
             target_node < m; ++target_node) {
          if (target_used[target_node] ||
              !compatible(source_node, target_node)) {
            continue;
          }

          target_used[target_node] = true;
          mapping.emplace_back(source_node, target_node);
          const std::size_t mismatch =
              source.nodes[source_node].label ==
                      target.nodes[target_node].label
                  ? 0U
                  : 1U;
          enumerate(source_node + 1U, relabels + mismatch);
          mapping.pop_back();
          target_used[target_node] = false;
        }
      };

  enumerate(0U, 0U);
  return best;
}

inline OrderedRootedTree random_tree(
    std::mt19937_64& random, const std::size_t size) {
  OrderedRootedTree tree;
  if (size == 0U) {
    return tree;
  }

  tree.nodes.resize(size);
  tree.root = 0U;
  for (std::size_t node = 0U; node < size; ++node) {
    tree.nodes[node].label =
        static_cast<std::int64_t>(random() % 4U);
  }

  for (std::size_t node = 1U; node < size; ++node) {
    const std::size_t parent =
        static_cast<std::size_t>(random() % node);
    tree.nodes[parent].children.push_back(node);
  }
  for (auto& node : tree.nodes) {
    std::shuffle(node.children.begin(), node.children.end(), random);
  }
  return tree;
}

}  // namespace ordered_tree_edit_distance_test_detail

TEST_CASE(ordered_tree_edit_distance_empty_single_and_relabel) {
  const OrderedRootedTree empty;

  const OrderedRootedTree one_a{
      {{7, {}}}, 0U};
  const OrderedRootedTree one_b{
      {{9, {}}}, 0U};

  REQUIRE_EQ(ordered_tree_edit_distance(empty, empty), 0U);
  REQUIRE_EQ(ordered_tree_edit_distance(empty, one_a), 1U);
  REQUIRE_EQ(ordered_tree_edit_distance(one_a, empty), 1U);
  REQUIRE_EQ(ordered_tree_edit_distance(one_a, one_a), 0U);
  REQUIRE_EQ(ordered_tree_edit_distance(one_a, one_b), 1U);
}

TEST_CASE(ordered_tree_edit_distance_delete_promotes_children) {
  // Deleting the source root promotes its only child, yielding the target.
  const OrderedRootedTree source{
      {
          {10, {1U}},
          {20, {}},
      },
      0U};
  const OrderedRootedTree target{
      {
          {20, {}},
      },
      0U};

  REQUIRE_EQ(ordered_tree_edit_distance(source, target), 1U);
  REQUIRE_EQ(ordered_tree_edit_distance(target, source), 1U);
}

TEST_CASE(ordered_tree_edit_distance_respects_sibling_order) {
  const OrderedRootedTree first{
      {
          {0, {1U, 2U}},
          {1, {}},
          {2, {}},
      },
      0U};
  const OrderedRootedTree swapped{
      {
          {0, {1U, 2U}},
          {2, {}},
          {1, {}},
      },
      0U};

  REQUIRE_EQ(ordered_tree_edit_distance(first, swapped), 2U);
  REQUIRE_EQ(
      ordered_tree_edit_distance(first, swapped),
      ordered_tree_edit_distance(swapped, first));
}

TEST_CASE(ordered_tree_edit_distance_rejects_malformed_structure) {
  const OrderedRootedTree empty_with_root{{}, 0U};
  REQUIRE_THROWS_AS(
      ordered_tree_edit_distance(empty_with_root, OrderedRootedTree{}),
      std::invalid_argument);

  const OrderedRootedTree missing_root{
      {
          {1, {}},
      },
      std::nullopt};
  REQUIRE_THROWS_AS(
      ordered_tree_edit_distance(missing_root, OrderedRootedTree{}),
      std::invalid_argument);

  const OrderedRootedTree out_of_range{
      {
          {1, {1U}},
      },
      0U};
  REQUIRE_THROWS_AS(
      ordered_tree_edit_distance(out_of_range, OrderedRootedTree{}),
      std::invalid_argument);

  const OrderedRootedTree disconnected{
      {
          {1, {}},
          {2, {}},
      },
      0U};
  REQUIRE_THROWS_AS(
      ordered_tree_edit_distance(disconnected, OrderedRootedTree{}),
      std::invalid_argument);

  const OrderedRootedTree cycle{
      {
          {1, {1U}},
          {2, {0U}},
      },
      0U};
  REQUIRE_THROWS_AS(
      ordered_tree_edit_distance(cycle, OrderedRootedTree{}),
      std::invalid_argument);

  const OrderedRootedTree shared_child{
      {
          {1, {1U, 2U}},
          {2, {2U}},
          {3, {}},
      },
      0U};
  REQUIRE_THROWS_AS(
      ordered_tree_edit_distance(shared_child, OrderedRootedTree{}),
      std::invalid_argument);
}

TEST_CASE(ordered_tree_edit_distance_known_mixed_structure) {
  using namespace ordered_tree_edit_distance_test_detail;

  const OrderedRootedTree first{
      {
          {5, {1U, 2U}},
          {7, {3U}},
          {9, {}},
          {8, {}},
      },
      0U};
  const OrderedRootedTree second{
      {
          {5, {1U, 2U}},
          {7, {}},
          {9, {3U}},
          {8, {}},
      },
      0U};

  REQUIRE_EQ(
      ordered_tree_edit_distance(first, second),
      brute_mapping_distance(first, second));
  REQUIRE_EQ(
      ordered_tree_edit_distance(second, first),
      brute_mapping_distance(second, first));
}

TEST_CASE(ordered_tree_edit_distance_random_tiny_mapping_oracle) {
  using namespace ordered_tree_edit_distance_test_detail;

  std::mt19937_64 random(0x7EEED17D157AULL);
  for (std::size_t trial = 0U; trial < 220U; ++trial) {
    const std::size_t first_size =
        static_cast<std::size_t>(random() % 6U);
    const std::size_t second_size =
        static_cast<std::size_t>(random() % 6U);

    const OrderedRootedTree first =
        random_tree(random, first_size);
    const OrderedRootedTree second =
        random_tree(random, second_size);

    const std::size_t expected =
        brute_mapping_distance(first, second);
    const std::size_t actual =
        ordered_tree_edit_distance(first, second);

    REQUIRE_EQ(actual, expected);
    REQUIRE_EQ(
        actual,
        ordered_tree_edit_distance(second, first));

    const std::size_t size_gap =
        first_size > second_size
            ? first_size - second_size
            : second_size - first_size;
    REQUIRE(actual >= size_gap);
    REQUIRE(actual <= first_size + second_size);
  }
}
