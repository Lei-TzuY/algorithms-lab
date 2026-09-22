#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::dynamic_programming {

struct OrderedTreeNode {
  std::int64_t label{};
  std::vector<std::size_t> children;

  friend bool operator==(const OrderedTreeNode&,
                         const OrderedTreeNode&) = default;
};

struct OrderedRootedTree {
  std::vector<OrderedTreeNode> nodes;
  std::optional<std::size_t> root;

  friend bool operator==(const OrderedRootedTree&,
                         const OrderedRootedTree&) = default;
};

// Exact unit-cost ordered rooted-tree edit distance.
//
// Allowed operations are the standard Zhang-Shasha ordered-tree operations:
//   * relabel one node;
//   * delete one node, promoting its ordered children into its position;
//   * insert one node (the inverse of delete).
//
// Empty trees are represented by nodes.empty() and root == nullopt.
// Non-empty trees must have exactly one root and every other node exactly one
// parent. Child-vector order is semantically significant.
//
// Throws std::invalid_argument for malformed tree structure and
// std::length_error if the representable distance bound would overflow size_t.
[[nodiscard]] inline std::size_t ordered_tree_edit_distance(
    const OrderedRootedTree& source,
    const OrderedRootedTree& target) {
  struct PreparedTree {
    // All arrays are 1-based in postorder; index 0 is a sentinel.
    std::vector<std::int64_t> labels{0};
    std::vector<std::size_t> leftmost{0};
    std::vector<std::size_t> keyroots;
  };

  const auto prepare = [](const OrderedRootedTree& tree) -> PreparedTree {
    PreparedTree out;
    const std::size_t n = tree.nodes.size();

    if (n == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error(
          "ordered tree is too large for postorder indexing");
    }

    if (n == 0U) {
      if (tree.root.has_value()) {
        throw std::invalid_argument(
            "empty ordered tree must not name a root");
      }
      return out;
    }

    if (!tree.root.has_value() || *tree.root >= n) {
      throw std::invalid_argument(
          "non-empty ordered tree requires an in-range root");
    }

    std::vector<std::size_t> indegree(n, 0U);
    for (std::size_t parent = 0U; parent < n; ++parent) {
      for (const std::size_t child : tree.nodes[parent].children) {
        if (child >= n) {
          throw std::invalid_argument(
              "ordered tree child index out of range");
        }
        if (indegree[child] == std::numeric_limits<std::size_t>::max()) {
          throw std::invalid_argument(
              "ordered tree indegree overflow");
        }
        ++indegree[child];
        if (indegree[child] > 1U) {
          throw std::invalid_argument(
              "ordered tree node has multiple parents");
        }
      }
    }

    const std::size_t root = *tree.root;
    if (indegree[root] != 0U) {
      throw std::invalid_argument(
          "ordered tree root must have indegree zero");
    }
    for (std::size_t node = 0U; node < n; ++node) {
      if (node != root && indegree[node] != 1U) {
        throw std::invalid_argument(
            "ordered tree must be connected with one parent per non-root");
      }
    }

    std::vector<unsigned char> color(n, 0U);
    std::vector<std::pair<std::size_t, std::size_t>> stack;
    std::vector<std::size_t> postorder;
    postorder.reserve(n);
    stack.reserve(n);

    color[root] = 1U;
    stack.emplace_back(root, 0U);

    while (!stack.empty()) {
      auto& [node, next_child] = stack.back();
      const auto& children = tree.nodes[node].children;

      if (next_child < children.size()) {
        const std::size_t child = children[next_child++];
        if (color[child] != 0U) {
          throw std::invalid_argument(
              "ordered tree contains a cycle or repeated traversal");
        }
        color[child] = 1U;
        stack.emplace_back(child, 0U);
        continue;
      }

      color[node] = 2U;
      postorder.push_back(node);
      stack.pop_back();
    }

    if (postorder.size() != n) {
      throw std::invalid_argument(
          "ordered tree contains unreachable nodes");
    }

    std::vector<std::size_t> post_index(n, 0U);
    out.labels.resize(n + 1U);
    out.leftmost.resize(n + 1U);

    for (std::size_t offset = 0U; offset < n; ++offset) {
      const std::size_t node = postorder[offset];
      const std::size_t index = offset + 1U;
      post_index[node] = index;
      out.labels[index] = tree.nodes[node].label;
    }

    for (std::size_t index = 1U; index <= n; ++index) {
      const std::size_t node = postorder[index - 1U];
      const auto& children = tree.nodes[node].children;
      if (children.empty()) {
        out.leftmost[index] = index;
      } else {
        const std::size_t first_child_index =
            post_index[children.front()];
        if (first_child_index == 0U ||
            out.leftmost[first_child_index] == 0U) {
          throw std::logic_error(
              "ordered tree postorder preparation invariant violated");
        }
        out.leftmost[index] = out.leftmost[first_child_index];
      }
    }

    std::vector<std::size_t> last_keyroot_for_leftmost(n + 1U, 0U);
    for (std::size_t index = 1U; index <= n; ++index) {
      last_keyroot_for_leftmost[out.leftmost[index]] = index;
    }
    for (const std::size_t keyroot : last_keyroot_for_leftmost) {
      if (keyroot != 0U) {
        out.keyroots.push_back(keyroot);
      }
    }
    std::sort(out.keyroots.begin(), out.keyroots.end());
    return out;
  };

  const PreparedTree a = prepare(source);
  const PreparedTree b = prepare(target);
  const std::size_t n = source.nodes.size();
  const std::size_t m = target.nodes.size();

  if (n > std::numeric_limits<std::size_t>::max() - m) {
    throw std::length_error(
        "ordered tree edit distance exceeds size_t range");
  }

  if (n == 0U) {
    return m;
  }
  if (m == 0U) {
    return n;
  }

  std::vector<std::vector<std::size_t>> tree_distance(
      n + 1U, std::vector<std::size_t>(m + 1U, 0U));

  for (const std::size_t source_keyroot : a.keyroots) {
    const std::size_t source_begin = a.leftmost[source_keyroot];

    for (const std::size_t target_keyroot : b.keyroots) {
      const std::size_t target_begin = b.leftmost[target_keyroot];

      const std::size_t rows =
          source_keyroot - source_begin + 2U;
      const std::size_t columns =
          target_keyroot - target_begin + 2U;
      std::vector<std::vector<std::size_t>> forest_distance(
          rows, std::vector<std::size_t>(columns, 0U));

      for (std::size_t source_index = source_begin;
           source_index <= source_keyroot; ++source_index) {
        const std::size_t row =
            source_index - source_begin + 1U;
        forest_distance[row][0U] =
            forest_distance[row - 1U][0U] + 1U;
      }
      for (std::size_t target_index = target_begin;
           target_index <= target_keyroot; ++target_index) {
        const std::size_t column =
            target_index - target_begin + 1U;
        forest_distance[0U][column] =
            forest_distance[0U][column - 1U] + 1U;
      }

      for (std::size_t source_index = source_begin;
           source_index <= source_keyroot; ++source_index) {
        const std::size_t row =
            source_index - source_begin + 1U;

        for (std::size_t target_index = target_begin;
             target_index <= target_keyroot; ++target_index) {
          const std::size_t column =
              target_index - target_begin + 1U;

          const std::size_t erase_cost =
              forest_distance[row - 1U][column] + 1U;
          const std::size_t insert_cost =
              forest_distance[row][column - 1U] + 1U;

          if (a.leftmost[source_index] == source_begin &&
              b.leftmost[target_index] == target_begin) {
            const std::size_t relabel_cost =
                a.labels[source_index] == b.labels[target_index]
                    ? 0U
                    : 1U;
            const std::size_t align_cost =
                forest_distance[row - 1U][column - 1U] +
                relabel_cost;

            const std::size_t best =
                std::min({erase_cost, insert_cost, align_cost});
            forest_distance[row][column] = best;
            tree_distance[source_index][target_index] = best;
          } else {
            const std::size_t source_prefix =
                a.leftmost[source_index] - source_begin;
            const std::size_t target_prefix =
                b.leftmost[target_index] - target_begin;

            const std::size_t subtree_cost =
                forest_distance[source_prefix][target_prefix] +
                tree_distance[source_index][target_index];

            forest_distance[row][column] =
                std::min({erase_cost, insert_cost, subtree_cost});
          }
        }
      }
    }
  }

  return tree_distance[n][m];
}

}  // namespace algorithms::dynamic_programming
