#pragma once

#include "algorithms/coding/garsia_wachs.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace alphabetic_code_test_detail {

std::uint64_t checked_add(std::uint64_t a, std::uint64_t b) {
  if (b > std::numeric_limits<std::uint64_t>::max() - a) {
    throw std::overflow_error("oracle overflow");
  }
  return a + b;
}

std::vector<std::vector<std::size_t>> all_ordered_full_tree_depths(
    std::size_t leaf_count) {
  if (leaf_count == 0) return {{}};
  if (leaf_count == 1) return {{0}};

  std::vector<std::vector<std::size_t>> result;
  for (std::size_t left_count = 1; left_count < leaf_count; ++left_count) {
    const auto left_shapes = all_ordered_full_tree_depths(left_count);
    const auto right_shapes = all_ordered_full_tree_depths(leaf_count - left_count);
    for (const auto& left : left_shapes) {
      for (const auto& right : right_shapes) {
        std::vector<std::size_t> depths;
        depths.reserve(leaf_count);
        for (const auto depth : left) depths.push_back(depth + 1);
        for (const auto depth : right) depths.push_back(depth + 1);
        result.push_back(std::move(depths));
      }
    }
  }
  return result;
}

std::uint64_t exhaustive_optimum(const std::vector<std::uint64_t>& weights) {
  if (weights.size() <= 1) return 0;
  std::uint64_t best = std::numeric_limits<std::uint64_t>::max();
  for (const auto& depths : all_ordered_full_tree_depths(weights.size())) {
    std::uint64_t cost = 0;
    for (std::size_t i = 0; i < weights.size(); ++i) {
      if (weights[i] != 0 &&
          depths[i] > std::numeric_limits<std::uint64_t>::max() / weights[i]) {
        throw std::overflow_error("oracle term overflow");
      }
      cost = checked_add(cost, weights[i] * static_cast<std::uint64_t>(depths[i]));
    }
    best = std::min(best, cost);
  }
  return best;
}

void replay_tree(const algorithms::coding::OptimalAlphabeticBinaryTree& result,
                 const std::vector<std::uint64_t>& weights) {
  const std::size_t n = weights.size();
  if (n == 0) {
    REQUIRE(!result.root.has_value());
    REQUIRE(result.nodes.empty());
    REQUIRE(result.leaf_depths.empty());
    return;
  }
  REQUIRE(result.root.has_value());
  REQUIRE_EQ(result.nodes.size(), n * 2 - 1);
  std::vector<std::size_t> leaves;
  std::vector<std::pair<std::size_t, std::size_t>> stack{{*result.root, 0}};
  std::uint64_t replay_cost = 0;
  while (!stack.empty()) {
    const auto [node, depth] = stack.back(); stack.pop_back();
    REQUIRE(node < result.nodes.size());
    const auto& entry = result.nodes[node];
    if (entry.leaf_index.has_value()) {
      REQUIRE(!entry.left.has_value()); REQUIRE(!entry.right.has_value());
      REQUIRE(*entry.leaf_index < n);
      leaves.push_back(*entry.leaf_index);
      REQUIRE_EQ(result.leaf_depths[*entry.leaf_index], depth);
      if (weights[*entry.leaf_index] != 0) {
        REQUIRE(depth <= std::numeric_limits<std::uint64_t>::max() / weights[*entry.leaf_index]);
      }
      replay_cost += weights[*entry.leaf_index] * static_cast<std::uint64_t>(depth);
    } else {
      REQUIRE(entry.left.has_value()); REQUIRE(entry.right.has_value());
      stack.push_back({*entry.right, depth + 1});
      stack.push_back({*entry.left, depth + 1});
    }
  }
  std::vector<std::size_t> expected(n);
  for (std::size_t i = 0; i < n; ++i) expected[i] = i;
  REQUIRE_EQ(leaves, expected);
  REQUIRE_EQ(replay_cost, result.weighted_external_path_length);
  REQUIRE_EQ(result.compatibility_merges, n - 1);
}

}  // namespace alphabetic_code_test_detail

TEST_CASE(alphabetic_code_empty_singleton_and_known_weights) {
  using algorithms::coding::optimal_alphabetic_binary_tree;
  const std::vector<std::uint64_t> empty;
  auto a = optimal_alphabetic_binary_tree(empty);
  alphabetic_code_test_detail::replay_tree(a, empty);
  REQUIRE_EQ(a.weighted_external_path_length, std::uint64_t{0});

  const std::vector<std::uint64_t> one{std::numeric_limits<std::uint64_t>::max()};
  auto b = optimal_alphabetic_binary_tree(one);
  alphabetic_code_test_detail::replay_tree(b, one);
  REQUIRE_EQ(b.leaf_depths, std::vector<std::size_t>({0}));
  REQUIRE_EQ(b.weighted_external_path_length, std::uint64_t{0});

  const std::vector<std::uint64_t> weights{2, 7, 1, 8, 2, 8};
  auto c = optimal_alphabetic_binary_tree(weights);
  alphabetic_code_test_detail::replay_tree(c, weights);
  REQUIRE_EQ(c.weighted_external_path_length,
             alphabetic_code_test_detail::exhaustive_optimum(weights));
}

TEST_CASE(alphabetic_code_zero_weights_ties_and_determinism) {
  using algorithms::coding::optimal_alphabetic_binary_tree;
  const std::vector<std::vector<std::uint64_t>> cases{
      {0,0}, {0,0,0,0}, {5,5,5,5,5}, {0,10,0,10,0}, {9,1,1,1,9}};
  for (const auto& weights : cases) {
    const auto first = optimal_alphabetic_binary_tree(weights);
    const auto second = optimal_alphabetic_binary_tree(weights);
    REQUIRE_EQ(first.leaf_depths, second.leaf_depths);
    REQUIRE_EQ(first.weighted_external_path_length, second.weighted_external_path_length);
    alphabetic_code_test_detail::replay_tree(first, weights);
    REQUIRE_EQ(first.weighted_external_path_length,
               alphabetic_code_test_detail::exhaustive_optimum(weights));
  }
}

TEST_CASE(alphabetic_code_checked_overflow) {
  using algorithms::coding::optimal_alphabetic_binary_tree;
  const std::vector<std::uint64_t> exact{std::numeric_limits<std::uint64_t>::max(), 0};
  const auto result = optimal_alphabetic_binary_tree(exact);
  REQUIRE_EQ(result.weighted_external_path_length, std::numeric_limits<std::uint64_t>::max());

  const std::vector<std::uint64_t> overflow{std::numeric_limits<std::uint64_t>::max(), 1};
  REQUIRE_THROWS_AS(optimal_alphabetic_binary_tree(overflow), std::overflow_error);
}

TEST_CASE(alphabetic_code_randomized_against_exhaustive_ordered_trees) {
  using algorithms::coding::optimal_alphabetic_binary_tree;
  std::mt19937_64 rng(0x474152534941ULL);
  for (std::size_t trial = 0; trial < 900; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 9);
    std::vector<std::uint64_t> weights(n);
    for (auto& weight : weights) weight = rng() % 31;
    const auto result = optimal_alphabetic_binary_tree(weights);
    alphabetic_code_test_detail::replay_tree(result, weights);
    REQUIRE_EQ(result.weighted_external_path_length,
               alphabetic_code_test_detail::exhaustive_optimum(weights));
  }
}
