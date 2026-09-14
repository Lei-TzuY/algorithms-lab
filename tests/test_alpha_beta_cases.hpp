#pragma once

#include "algorithms/games/alpha_beta.hpp"
#include "test_framework.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <utility>
#include <vector>

namespace {

using algorithms::games::GameTreeNode;

struct AlphaBetaOracleResult {
  std::int64_t value{};
  std::vector<std::size_t> principal_variation;
  std::size_t visited_nodes{};
  std::size_t evaluated_leaves{};
};

AlphaBetaOracleResult exhaustive_minimax(
    const std::vector<GameTreeNode>& nodes, std::size_t vertex,
    bool maximizing) {
  AlphaBetaOracleResult result;
  result.visited_nodes = 1U;
  if (nodes[vertex].children.empty()) {
    result.value = *nodes[vertex].utility;
    result.principal_variation = {vertex};
    result.evaluated_leaves = 1U;
    return result;
  }

  std::optional<std::int64_t> best;
  std::vector<std::size_t> best_suffix;
  for (const std::size_t child : nodes[vertex].children) {
    auto child_result = exhaustive_minimax(nodes, child, !maximizing);
    result.visited_nodes += child_result.visited_nodes;
    result.evaluated_leaves += child_result.evaluated_leaves;
    if (!best.has_value() ||
        (maximizing ? child_result.value > *best
                    : child_result.value < *best)) {
      best = child_result.value;
      best_suffix = std::move(child_result.principal_variation);
    }
  }

  result.value = *best;
  result.principal_variation.push_back(vertex);
  result.principal_variation.insert(result.principal_variation.end(),
                                    best_suffix.begin(), best_suffix.end());
  return result;
}

std::size_t alpha_beta_bounded(std::mt19937_64& rng, std::size_t bound) {
  return static_cast<std::size_t>(rng() % static_cast<std::uint64_t>(bound));
}

std::vector<GameTreeNode> random_alpha_beta_tree(std::mt19937_64& rng,
                                                 std::size_t node_count) {
  std::vector<GameTreeNode> nodes(node_count);
  for (std::size_t vertex = 1U; vertex < node_count; ++vertex) {
    const std::size_t parent = alpha_beta_bounded(rng, vertex);
    nodes[parent].children.push_back(vertex);
  }
  for (auto& node : nodes) {
    if (node.children.empty()) {
      const std::uint64_t sample = rng() % 2001ULL;
      node.utility = static_cast<std::int64_t>(sample) - 1000;
    }
  }
  return nodes;
}

void require_alpha_beta_pv_replays(const std::vector<GameTreeNode>& nodes,
                                   const std::vector<std::size_t>& pv,
                                   std::int64_t value) {
  REQUIRE(!pv.empty());
  for (std::size_t index = 1U; index < pv.size(); ++index) {
    bool found = false;
    for (const std::size_t child : nodes[pv[index - 1U]].children) {
      if (child == pv[index]) {
        found = true;
        break;
      }
    }
    REQUIRE(found);
  }
  const std::size_t leaf = pv.back();
  REQUIRE(nodes[leaf].children.empty());
  REQUIRE(nodes[leaf].utility.has_value());
  REQUIRE_EQ(*nodes[leaf].utility, value);
}

TEST_CASE(alpha_beta_known_tree_prunes_and_returns_principal_variation) {
  std::vector<GameTreeNode> nodes(7U);
  nodes[0].children = {1U, 2U};
  nodes[1].children = {3U, 4U};
  nodes[2].children = {5U, 6U};
  nodes[3].utility = 3;
  nodes[4].utility = 5;
  nodes[5].utility = 2;
  nodes[6].utility = 9;

  const auto result = algorithms::games::alpha_beta_minimax(nodes);
  const auto oracle = exhaustive_minimax(nodes, 0U, true);
  REQUIRE_EQ(result.value, 3);
  REQUIRE_EQ(result.principal_variation,
             (std::vector<std::size_t>{0U, 1U, 3U}));
  REQUIRE(result.cutoffs >= 1U);
  REQUIRE(result.visited_nodes < oracle.visited_nodes);
  REQUIRE(result.evaluated_leaves < oracle.evaluated_leaves);
  REQUIRE_EQ(result.value, oracle.value);
  REQUIRE_EQ(result.principal_variation, oracle.principal_variation);
  require_alpha_beta_pv_replays(nodes, result.principal_variation,
                                result.value);
}

TEST_CASE(alpha_beta_supports_full_int64_utilities_and_minimizing_root) {
  std::vector<GameTreeNode> nodes(3U);
  nodes[0].children = {1U, 2U};
  nodes[1].utility = std::numeric_limits<std::int64_t>::min();
  nodes[2].utility = std::numeric_limits<std::int64_t>::max();

  const auto maximum = algorithms::games::alpha_beta_minimax(nodes);
  REQUIRE_EQ(maximum.value, std::numeric_limits<std::int64_t>::max());
  const auto minimum = algorithms::games::alpha_beta_minimax(nodes, 0U, false);
  REQUIRE_EQ(minimum.value, std::numeric_limits<std::int64_t>::min());

  std::vector<GameTreeNode> tie(3U);
  tie[0].children = {1U, 2U};
  tie[1].utility = 7;
  tie[2].utility = 7;
  const auto tied = algorithms::games::alpha_beta_minimax(tie);
  REQUIRE_EQ(tied.value, 7);
  REQUIRE_EQ(tied.principal_variation,
             (std::vector<std::size_t>{0U, 1U}));

  std::vector<GameTreeNode> nonzero_root(3U);
  nonzero_root[2].children = {0U, 1U};
  nonzero_root[0].utility = -4;
  nonzero_root[1].utility = 6;
  const auto rooted =
      algorithms::games::alpha_beta_minimax(nonzero_root, 2U, true);
  REQUIRE_EQ(rooted.value, 6);
  REQUIRE_EQ(rooted.principal_variation,
             (std::vector<std::size_t>{2U, 1U}));
}

TEST_CASE(alpha_beta_rejects_non_tree_and_invalid_payloads) {
  REQUIRE_THROWS_AS(algorithms::games::alpha_beta_minimax({}),
                    std::invalid_argument);

  std::vector<GameTreeNode> singleton(1U);
  singleton[0].utility = 0;
  REQUIRE_THROWS_AS(algorithms::games::alpha_beta_minimax(singleton, 1U),
                    std::out_of_range);

  std::vector<GameTreeNode> bad_child(1U);
  bad_child[0].children = {1U};
  REQUIRE_THROWS_AS(algorithms::games::alpha_beta_minimax(bad_child),
                    std::out_of_range);

  std::vector<GameTreeNode> internal_payload(2U);
  internal_payload[0].children = {1U};
  internal_payload[0].utility = 1;
  internal_payload[1].utility = 0;
  REQUIRE_THROWS_AS(algorithms::games::alpha_beta_minimax(internal_payload),
                    std::invalid_argument);

  std::vector<GameTreeNode> leaf_without_utility(2U);
  leaf_without_utility[0].children = {1U};
  REQUIRE_THROWS_AS(
      algorithms::games::alpha_beta_minimax(leaf_without_utility),
      std::invalid_argument);

  std::vector<GameTreeNode> disconnected(3U);
  disconnected[0].children = {1U};
  disconnected[1].utility = 0;
  disconnected[2].utility = 0;
  REQUIRE_THROWS_AS(algorithms::games::alpha_beta_minimax(disconnected),
                    std::invalid_argument);

  std::vector<GameTreeNode> duplicate_child(2U);
  duplicate_child[0].children = {1U, 1U};
  duplicate_child[1].utility = 0;
  REQUIRE_THROWS_AS(algorithms::games::alpha_beta_minimax(duplicate_child),
                    std::invalid_argument);
}

TEST_CASE(alpha_beta_randomized_differential_against_full_minimax) {
  std::mt19937_64 rng(0xA17AB37AULL);
  for (std::size_t trial = 0U; trial < 1500U; ++trial) {
    const std::size_t node_count = 1U + alpha_beta_bounded(rng, 80U);
    const auto nodes = random_alpha_beta_tree(rng, node_count);
    const bool maximizing_root = (rng() & 1ULL) != 0ULL;

    const auto result =
        algorithms::games::alpha_beta_minimax(nodes, 0U, maximizing_root);
    const auto oracle = exhaustive_minimax(nodes, 0U, maximizing_root);
    REQUIRE_EQ(result.value, oracle.value);
    REQUIRE_EQ(result.principal_variation, oracle.principal_variation);
    REQUIRE(result.visited_nodes <= oracle.visited_nodes);
    REQUIRE(result.evaluated_leaves <= oracle.evaluated_leaves);
    require_alpha_beta_pv_replays(nodes, result.principal_variation,
                                  result.value);
  }
}

}  // namespace
