#pragma once

#include "algorithms/data_structures/balanced_parentheses_tree.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <vector>

namespace balanced_parentheses_tree_tests {

using algorithms::data_structures::BalancedParenthesesTreeIndex;

struct OracleTree {
  std::vector<std::size_t> parent;
  std::vector<std::vector<std::size_t>> children;
  std::vector<std::uint8_t> bits;
  std::vector<std::size_t> open;
  std::vector<std::size_t> close;
  std::vector<std::size_t> subtree;
  std::vector<std::size_t> depth;
  std::vector<std::size_t> preorder;
};

inline void emit(const std::vector<std::vector<std::size_t>>& children,
                 std::size_t vertex, std::vector<std::uint8_t>& bits,
                 std::vector<std::size_t>& open,
                 std::vector<std::size_t>& close,
                 std::vector<std::size_t>& subtree,
                 std::vector<std::size_t>& depth,
                 std::vector<std::size_t>& preorder,
                 std::size_t current_depth, std::size_t& next_preorder) {
  preorder[vertex] = next_preorder++;
  open[vertex] = bits.size();
  depth[vertex] = current_depth;
  bits.push_back(1U);
  std::size_t size = 1U;
  for (const std::size_t child : children[vertex]) {
    emit(children, child, bits, open, close, subtree, depth, preorder,
         current_depth + 1U, next_preorder);
    size += subtree[child];
  }
  close[vertex] = bits.size();
  bits.push_back(0U);
  subtree[vertex] = size;
}

inline OracleTree make_tree(std::mt19937_64& random, std::size_t size) {
  OracleTree tree;
  tree.parent.assign(size, BalancedParenthesesTreeIndex::npos);
  tree.children.resize(size);
  tree.open.resize(size);
  tree.close.resize(size);
  tree.subtree.resize(size);
  tree.depth.resize(size);
  tree.preorder.resize(size);
  if (size == 0U) return tree;

  for (std::size_t vertex = 1U; vertex < size; ++vertex) {
    std::uniform_int_distribution<std::size_t> parent_distribution(0U,
                                                                   vertex - 1U);
    const std::size_t parent = parent_distribution(random);
    tree.parent[vertex] = parent;
    tree.children[parent].push_back(vertex);
  }
  std::size_t next_preorder = 0U;
  emit(tree.children, 0U, tree.bits, tree.open, tree.close, tree.subtree,
       tree.depth, tree.preorder, 0U, next_preorder);
  return tree;
}

inline void verify_tree(const OracleTree& tree) {
  BalancedParenthesesTreeIndex index(tree.bits);
  REQUIRE_EQ(index.node_count(), tree.parent.size());
  REQUIRE(index.valid_structure());
  if (tree.parent.empty()) {
    REQUIRE(index.empty());
    return;
  }

  for (std::size_t vertex = 0U; vertex < tree.parent.size(); ++vertex) {
    const std::size_t node = tree.preorder[vertex];
    REQUIRE_EQ(index.open_position(node), tree.open[vertex]);
    REQUIRE_EQ(index.close_position(node), tree.close[vertex]);
    REQUIRE_EQ(index.find_close(tree.open[vertex]), tree.close[vertex]);
    REQUIRE_EQ(index.node_at_open_position(tree.open[vertex]),
               std::optional<std::size_t>{node});
    REQUIRE(!index.node_at_open_position(tree.close[vertex]).has_value());
    REQUIRE_EQ(index.depth(node), tree.depth[vertex]);
    REQUIRE_EQ(index.subtree_size(node), tree.subtree[vertex]);

    if (tree.parent[vertex] == BalancedParenthesesTreeIndex::npos) {
      REQUIRE(!index.parent(node).has_value());
      REQUIRE(!index.enclose(tree.open[vertex]).has_value());
    } else {
      const std::size_t parent = tree.preorder[tree.parent[vertex]];
      REQUIRE_EQ(index.parent(node), std::optional<std::size_t>{parent});
      REQUIRE_EQ(index.enclose(tree.open[vertex]),
                 std::optional<std::size_t>{tree.open[tree.parent[vertex]]});
    }

    if (tree.children[vertex].empty()) {
      REQUIRE(!index.first_child(node).has_value());
    } else {
      REQUIRE_EQ(index.first_child(node),
                 std::optional<std::size_t>{
                     tree.preorder[tree.children[vertex].front()]});
    }

    const std::size_t parent_vertex = tree.parent[vertex];
    if (parent_vertex == BalancedParenthesesTreeIndex::npos) {
      REQUIRE(!index.next_sibling(node).has_value());
    } else {
      const auto it = std::find(tree.children[parent_vertex].begin(),
                                tree.children[parent_vertex].end(), vertex);
      const auto next = it + 1;
      if (next == tree.children[parent_vertex].end()) {
        REQUIRE(!index.next_sibling(node).has_value());
      } else {
        REQUIRE_EQ(index.next_sibling(node),
                   std::optional<std::size_t>{tree.preorder[*next]});
      }
    }
  }

  for (std::size_t ancestor = 0U; ancestor < tree.parent.size(); ++ancestor) {
    for (std::size_t node = 0U; node < tree.parent.size(); ++node) {
      const bool expected = tree.open[ancestor] <= tree.open[node] &&
                            tree.open[node] < tree.close[ancestor];
      REQUIRE_EQ(index.is_ancestor(tree.preorder[ancestor], tree.preorder[node]),
                 expected);
    }
  }
}

TEST_CASE(balanced_parentheses_tree_rejects_invalid_encodings) {
  using Index = BalancedParenthesesTreeIndex;
  REQUIRE_THROWS_AS(Index(std::vector<std::uint8_t>{0U, 1U}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(Index(std::vector<std::uint8_t>{1U, 0U, 1U, 0U}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(Index(std::vector<std::uint8_t>{1U, 1U, 0U}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(Index(std::vector<std::uint8_t>{1U, 2U, 0U, 0U}),
                    std::invalid_argument);
}

TEST_CASE(balanced_parentheses_tree_known_navigation_and_bounds) {
  std::vector<std::uint8_t> bits{1U, 1U, 0U, 1U, 1U, 0U, 0U, 0U};
  BalancedParenthesesTreeIndex index(bits);
  REQUIRE_EQ(index.node_count(), 4U);
  REQUIRE_EQ(index.first_child(0U), std::optional<std::size_t>{1U});
  REQUIRE_EQ(index.next_sibling(1U), std::optional<std::size_t>{2U});
  REQUIRE_EQ(index.parent(3U), std::optional<std::size_t>{2U});
  REQUIRE_EQ(index.depth(3U), 2U);
  REQUIRE_EQ(index.subtree_size(0U), 4U);
  REQUIRE_EQ(index.find_close(3U), 6U);
  REQUIRE_EQ(index.enclose(4U), std::optional<std::size_t>{3U});
  REQUIRE_THROWS_AS(index.open_position(4U), std::out_of_range);
  REQUIRE_THROWS_AS(index.find_close(2U), std::invalid_argument);
  REQUIRE_THROWS_AS(index.find_close(bits.size()), std::out_of_range);
}

TEST_CASE(balanced_parentheses_tree_crosses_packed_word_boundaries) {
  std::vector<std::uint8_t> chain;
  chain.insert(chain.end(), 130U, 1U);
  chain.insert(chain.end(), 130U, 0U);
  BalancedParenthesesTreeIndex chain_index(chain);
  REQUIRE_EQ(chain_index.node_count(), 130U);
  REQUIRE_EQ(chain_index.depth(129U), 129U);
  REQUIRE_EQ(chain_index.subtree_size(0U), 130U);
  REQUIRE(chain_index.valid_structure());

  std::vector<std::uint8_t> star;
  star.push_back(1U);
  for (std::size_t i = 0U; i < 130U; ++i) {
    star.push_back(1U);
    star.push_back(0U);
  }
  star.push_back(0U);
  BalancedParenthesesTreeIndex star_index(star);
  REQUIRE_EQ(star_index.node_count(), 131U);
  REQUIRE_EQ(star_index.subtree_size(0U), 131U);
  REQUIRE_EQ(star_index.depth(130U), 1U);
  REQUIRE(star_index.valid_structure());
}

TEST_CASE(balanced_parentheses_tree_randomized_differential) {
  std::mt19937_64 random(0xBADA55ULL);
  for (std::size_t trial = 0U; trial < 400U; ++trial) {
    std::uniform_int_distribution<std::size_t> size_distribution(0U, 100U);
    verify_tree(make_tree(random, size_distribution(random)));
  }
}

}  // namespace balanced_parentheses_tree_tests
