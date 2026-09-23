#pragma once

#include "algorithms/dynamic_programming/ordered_tree_edit_witness.hpp"

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
using algorithms::dynamic_programming::OrderedTreeEditWitness;
using algorithms::dynamic_programming::ordered_tree_edit_distance;
using algorithms::dynamic_programming::ordered_tree_edit_witness;
using algorithms::dynamic_programming::valid_ordered_tree_edit_witness;

namespace ordered_tree_edit_witness_test_detail {

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
          best = std::min(
              best,
              (n - mapped) + (m - mapped) + relabels);
          return;
        }

        enumerate(source_node + 1U, relabels);

        for (std::size_t target_node = 0U;
             target_node < m; ++target_node) {
          if (target_used[target_node] ||
              !compatible(source_node, target_node)) {
            continue;
          }

          target_used[target_node] = true;
          mapping.emplace_back(source_node, target_node);
          enumerate(
              source_node + 1U,
              relabels +
                  (source.nodes[source_node].label ==
                           target.nodes[target_node].label
                       ? 0U
                       : 1U));
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
        static_cast<std::int64_t>(random() % 5U);
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

}  // namespace ordered_tree_edit_witness_test_detail

TEST_CASE(ordered_tree_edit_witness_empty_single_and_relabel) {
  const OrderedRootedTree empty;
  const OrderedRootedTree one_a{{{7, {}}}, 0U};
  const OrderedRootedTree one_b{{{9, {}}}, 0U};

  const auto empty_witness =
      ordered_tree_edit_witness(empty, empty);
  REQUIRE_EQ(empty_witness.distance, 0U);
  REQUIRE(empty_witness.mapping.empty());
  REQUIRE(valid_ordered_tree_edit_witness(
      empty, empty, empty_witness));

  const auto equal_witness =
      ordered_tree_edit_witness(one_a, one_a);
  REQUIRE_EQ(equal_witness.distance, 0U);
  REQUIRE_EQ(equal_witness.mapping.size(), 1U);
  REQUIRE(
      equal_witness.mapping ==
      std::vector<std::pair<std::size_t, std::size_t>>{{0U, 0U}});
  REQUIRE(valid_ordered_tree_edit_witness(
      one_a, one_a, equal_witness));

  const auto relabel_witness =
      ordered_tree_edit_witness(one_a, one_b);
  REQUIRE_EQ(relabel_witness.distance, 1U);
  REQUIRE_EQ(relabel_witness.mapping.size(), 1U);
  REQUIRE(valid_ordered_tree_edit_witness(
      one_a, one_b, relabel_witness));
}

TEST_CASE(ordered_tree_edit_witness_root_delete_promotes_child) {
  const OrderedRootedTree source{
      {
          {10, {1U}},
          {20, {}},
      },
      0U};
  const OrderedRootedTree target{{{20, {}}}, 0U};

  const OrderedTreeEditWitness witness =
      ordered_tree_edit_witness(source, target);
  REQUIRE_EQ(witness.distance, 1U);
  REQUIRE(
      witness.mapping ==
      std::vector<std::pair<std::size_t, std::size_t>>{{1U, 0U}});
  REQUIRE(valid_ordered_tree_edit_witness(
      source, target, witness));
}

TEST_CASE(ordered_tree_edit_witness_is_deterministic_on_ties) {
  const OrderedRootedTree first{
      {
          {0, {1U, 2U}},
          {1, {}},
          {2, {}},
      },
      0U};
  const OrderedRootedTree second{
      {
          {0, {1U, 2U}},
          {2, {}},
          {1, {}},
      },
      0U};

  const OrderedTreeEditWitness expected =
      ordered_tree_edit_witness(first, second);
  REQUIRE_EQ(expected.distance, 2U);
  REQUIRE(valid_ordered_tree_edit_witness(
      first, second, expected));

  for (std::size_t repeat = 0U; repeat < 20U; ++repeat) {
    REQUIRE(
        ordered_tree_edit_witness(first, second) ==
        expected);
  }
}

TEST_CASE(ordered_tree_edit_witness_validator_rejects_bad_certificates) {
  const OrderedRootedTree first{
      {
          {0, {1U, 2U}},
          {0, {}},
          {0, {}},
      },
      0U};
  const OrderedRootedTree second = first;
  const OrderedTreeEditWitness optimum =
      ordered_tree_edit_witness(first, second);
  REQUIRE(valid_ordered_tree_edit_witness(
      first, second, optimum));

  OrderedTreeEditWitness wrong_distance = optimum;
  wrong_distance.distance = 1U;
  REQUIRE(!valid_ordered_tree_edit_witness(
      first, second, wrong_distance));

  const OrderedTreeEditWitness duplicate_source{
      2U, {{1U, 1U}, {1U, 2U}}};
  REQUIRE(!valid_ordered_tree_edit_witness(
      first, second, duplicate_source));

  const OrderedTreeEditWitness duplicate_target{
      2U, {{1U, 1U}, {2U, 1U}}};
  REQUIRE(!valid_ordered_tree_edit_witness(
      first, second, duplicate_target));

  const OrderedTreeEditWitness crossing{
      0U, {{0U, 0U}, {1U, 2U}, {2U, 1U}}};
  REQUIRE(!valid_ordered_tree_edit_witness(
      first, second, crossing));

  const OrderedTreeEditWitness non_optimal{
      6U, {}};
  REQUIRE(!valid_ordered_tree_edit_witness(
      first, second, non_optimal));

  const OrderedRootedTree malformed{
      {
          {0, {1U}},
      },
      0U};
  REQUIRE_THROWS_AS(
      ordered_tree_edit_witness(malformed, second),
      std::invalid_argument);
}

TEST_CASE(ordered_tree_edit_witness_random_tiny_matches_mapping_oracle) {
  using namespace ordered_tree_edit_witness_test_detail;

  std::mt19937_64 random(0xED17C37F1CA7EULL);
  for (std::size_t trial = 0U; trial < 260U; ++trial) {
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
    const OrderedTreeEditWitness witness =
        ordered_tree_edit_witness(first, second);

    REQUIRE_EQ(witness.distance, expected);
    REQUIRE_EQ(
        witness.distance,
        ordered_tree_edit_distance(first, second));
    REQUIRE(valid_ordered_tree_edit_witness(
        first, second, witness));
    REQUIRE(
        ordered_tree_edit_witness(first, second) ==
        witness);
  }
}

TEST_CASE(ordered_tree_edit_witness_larger_random_certificate_replay) {
  using namespace ordered_tree_edit_witness_test_detail;

  std::mt19937_64 random(0xC371F1CA7EULL);
  for (std::size_t trial = 0U; trial < 120U; ++trial) {
    const std::size_t first_size =
        6U + static_cast<std::size_t>(random() % 20U);
    const std::size_t second_size =
        6U + static_cast<std::size_t>(random() % 20U);
    const OrderedRootedTree first =
        random_tree(random, first_size);
    const OrderedRootedTree second =
        random_tree(random, second_size);

    const OrderedTreeEditWitness witness =
        ordered_tree_edit_witness(first, second);
    REQUIRE_EQ(
        witness.distance,
        ordered_tree_edit_distance(first, second));
    REQUIRE(valid_ordered_tree_edit_witness(
        first, second, witness));
  }
}
