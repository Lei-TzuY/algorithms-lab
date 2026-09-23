#pragma once

#include "algorithms/dynamic_programming/ordered_tree_edit_distance.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace algorithms::dynamic_programming {

struct OrderedTreeEditWitness {
  std::size_t distance{};
  // Original source-node index -> original target-node index.
  //
  // Unmapped source nodes are deletions, unmapped target nodes are insertions,
  // and mapped nodes with unequal labels are relabel operations.
  std::vector<std::pair<std::size_t, std::size_t>> mapping;

  friend bool operator==(const OrderedTreeEditWitness&,
                         const OrderedTreeEditWitness&) = default;
};

namespace ordered_tree_edit_witness_detail {

struct PreparedTree {
  // Arrays are 1-based in postorder; index 0 is a sentinel.
  std::vector<std::int64_t> labels{0};
  std::vector<std::size_t> leftmost{0};
  std::vector<std::size_t> keyroots;
  std::vector<std::size_t> post_to_node{0};
};

inline PreparedTree prepare_validated(const OrderedRootedTree& tree) {
  // Reuse the already-merged structural contract without duplicating its
  // malformed-tree acceptance logic. Against an empty counterpart the exact
  // distance implementation validates this tree and then returns in linear
  // preprocessing time.
  const OrderedRootedTree empty;
  (void)ordered_tree_edit_distance(tree, empty);

  PreparedTree out;
  const std::size_t n = tree.nodes.size();
  if (n == 0U) {
    return out;
  }

  const std::size_t root = *tree.root;
  std::vector<std::pair<std::size_t, std::size_t>> stack;
  std::vector<std::size_t> postorder;
  stack.reserve(n);
  postorder.reserve(n);
  stack.emplace_back(root, 0U);

  while (!stack.empty()) {
    auto& [node, next_child] = stack.back();
    const auto& children = tree.nodes[node].children;
    if (next_child < children.size()) {
      stack.emplace_back(children[next_child++], 0U);
      continue;
    }
    postorder.push_back(node);
    stack.pop_back();
  }

  std::vector<std::size_t> post_index(n, 0U);
  out.labels.resize(n + 1U);
  out.leftmost.resize(n + 1U);
  out.post_to_node.resize(n + 1U);

  for (std::size_t offset = 0U; offset < n; ++offset) {
    const std::size_t node = postorder[offset];
    const std::size_t index = offset + 1U;
    post_index[node] = index;
    out.labels[index] = tree.nodes[node].label;
    out.post_to_node[index] = node;
  }

  for (std::size_t index = 1U; index <= n; ++index) {
    const std::size_t node = postorder[index - 1U];
    const auto& children = tree.nodes[node].children;
    if (children.empty()) {
      out.leftmost[index] = index;
    } else {
      out.leftmost[index] =
          out.leftmost[post_index[children.front()]];
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
}

inline std::vector<std::vector<std::size_t>> build_tree_distances(
    const PreparedTree& source,
    const PreparedTree& target,
    const std::size_t source_size,
    const std::size_t target_size) {
  std::vector<std::vector<std::size_t>> tree_distance(
      source_size + 1U,
      std::vector<std::size_t>(target_size + 1U, 0U));

  for (const std::size_t source_keyroot : source.keyroots) {
    const std::size_t source_begin =
        source.leftmost[source_keyroot];

    for (const std::size_t target_keyroot : target.keyroots) {
      const std::size_t target_begin =
          target.leftmost[target_keyroot];

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

          if (source.leftmost[source_index] == source_begin &&
              target.leftmost[target_index] == target_begin) {
            const std::size_t relabel_cost =
                source.labels[source_index] ==
                        target.labels[target_index]
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
                source.leftmost[source_index] - source_begin;
            const std::size_t target_prefix =
                target.leftmost[target_index] - target_begin;
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

  return tree_distance;
}

enum class BacktrackAction : unsigned char {
  none,
  erase_source,
  insert_target,
  align_roots,
  use_subtree,
};

struct LocalBacktrack {
  std::size_t source_begin{};
  std::size_t target_begin{};
  std::vector<std::vector<BacktrackAction>> action;
};

inline LocalBacktrack build_local_backtrack(
    const PreparedTree& source,
    const PreparedTree& target,
    const std::vector<std::vector<std::size_t>>& tree_distance,
    const std::size_t source_root,
    const std::size_t target_root) {
  const std::size_t source_begin =
      source.leftmost[source_root];
  const std::size_t target_begin =
      target.leftmost[target_root];
  const std::size_t rows =
      source_root - source_begin + 2U;
  const std::size_t columns =
      target_root - target_begin + 2U;

  std::vector<std::vector<std::size_t>> forest_distance(
      rows, std::vector<std::size_t>(columns, 0U));
  std::vector<std::vector<BacktrackAction>> action(
      rows,
      std::vector<BacktrackAction>(
          columns, BacktrackAction::none));

  for (std::size_t source_index = source_begin;
       source_index <= source_root; ++source_index) {
    const std::size_t row =
        source_index - source_begin + 1U;
    forest_distance[row][0U] =
        forest_distance[row - 1U][0U] + 1U;
    action[row][0U] = BacktrackAction::erase_source;
  }
  for (std::size_t target_index = target_begin;
       target_index <= target_root; ++target_index) {
    const std::size_t column =
        target_index - target_begin + 1U;
    forest_distance[0U][column] =
        forest_distance[0U][column - 1U] + 1U;
    action[0U][column] = BacktrackAction::insert_target;
  }

  for (std::size_t source_index = source_begin;
       source_index <= source_root; ++source_index) {
    const std::size_t row =
        source_index - source_begin + 1U;

    for (std::size_t target_index = target_begin;
         target_index <= target_root; ++target_index) {
      const std::size_t column =
          target_index - target_begin + 1U;
      const std::size_t erase_cost =
          forest_distance[row - 1U][column] + 1U;
      const std::size_t insert_cost =
          forest_distance[row][column - 1U] + 1U;

      std::size_t best{};
      BacktrackAction best_action{};

      if (source.leftmost[source_index] == source_begin &&
          target.leftmost[target_index] == target_begin) {
        const std::size_t relabel_cost =
            source.labels[source_index] ==
                    target.labels[target_index]
                ? 0U
                : 1U;
        best =
            forest_distance[row - 1U][column - 1U] +
            relabel_cost;
        best_action = BacktrackAction::align_roots;
      } else {
        const std::size_t source_prefix =
            source.leftmost[source_index] - source_begin;
        const std::size_t target_prefix =
            target.leftmost[target_index] - target_begin;
        best =
            forest_distance[source_prefix][target_prefix] +
            tree_distance[source_index][target_index];
        best_action = BacktrackAction::use_subtree;
      }

      // Strict comparisons intentionally make the certificate deterministic:
      // prefer a structural match/subtree on ties, then deletion, then
      // insertion.
      if (erase_cost < best) {
        best = erase_cost;
        best_action = BacktrackAction::erase_source;
      }
      if (insert_cost < best) {
        best = insert_cost;
        best_action = BacktrackAction::insert_target;
      }

      forest_distance[row][column] = best;
      action[row][column] = best_action;
    }
  }

  return LocalBacktrack{
      source_begin, target_begin, std::move(action)};
}

inline std::vector<std::pair<std::size_t, std::size_t>>
recover_mapping(
    const PreparedTree& source,
    const PreparedTree& target,
    const std::vector<std::vector<std::size_t>>& tree_distance,
    const std::size_t source_root,
    const std::size_t target_root) {
  std::vector<std::pair<std::size_t, std::size_t>> mapping;
  std::vector<std::pair<std::size_t, std::size_t>> pending;
  pending.emplace_back(source_root, target_root);

  while (!pending.empty()) {
    const auto [current_source_root, current_target_root] =
        pending.back();
    pending.pop_back();

    const LocalBacktrack local =
        build_local_backtrack(
            source, target, tree_distance,
            current_source_root, current_target_root);
    std::size_t row =
        current_source_root - local.source_begin + 1U;
    std::size_t column =
        current_target_root - local.target_begin + 1U;

    while (row != 0U || column != 0U) {
      const BacktrackAction step = local.action[row][column];

      if (step == BacktrackAction::erase_source) {
        --row;
        continue;
      }
      if (step == BacktrackAction::insert_target) {
        --column;
        continue;
      }

      if (row == 0U || column == 0U) {
        throw std::logic_error(
            "ordered tree edit witness backtrack left forest bounds");
      }

      const std::size_t source_index =
          local.source_begin + row - 1U;
      const std::size_t target_index =
          local.target_begin + column - 1U;

      if (step == BacktrackAction::align_roots) {
        mapping.emplace_back(
            source.post_to_node[source_index],
            target.post_to_node[target_index]);
        --row;
        --column;
        continue;
      }

      if (step == BacktrackAction::use_subtree) {
        if (source_index == current_source_root &&
            target_index == current_target_root) {
          throw std::logic_error(
              "ordered tree edit witness recursive subtree made no progress");
        }

        pending.emplace_back(source_index, target_index);
        row =
            source.leftmost[source_index] -
            local.source_begin;
        column =
            target.leftmost[target_index] -
            local.target_begin;
        continue;
      }

      throw std::logic_error(
          "ordered tree edit witness missing backtrack action");
    }
  }

  std::sort(mapping.begin(), mapping.end());
  return mapping;
}

inline void preorder_intervals(
    const OrderedRootedTree& tree,
    std::vector<std::size_t>& preorder,
    std::vector<std::size_t>& subtree_end) {
  const std::size_t n = tree.nodes.size();
  preorder.assign(n, 0U);
  subtree_end.assign(n, 0U);
  if (n == 0U) {
    return;
  }

  std::size_t timer = 0U;
  std::vector<std::pair<std::size_t, std::size_t>> stack;
  stack.reserve(n);
  stack.emplace_back(*tree.root, 0U);
  preorder[*tree.root] = timer++;

  while (!stack.empty()) {
    auto& [node, next_child] = stack.back();
    const auto& children = tree.nodes[node].children;
    if (next_child < children.size()) {
      const std::size_t child = children[next_child++];
      preorder[child] = timer++;
      stack.emplace_back(child, 0U);
      continue;
    }
    subtree_end[node] = timer;
    stack.pop_back();
  }
}

}  // namespace ordered_tree_edit_witness_detail

// Returns a minimum-cost ordered-tree edit certificate.
//
// The mapping is a compact witness for the standard ordered-tree edit model.
// Unmapped source nodes correspond to deletions, unmapped target nodes to
// insertions, and mapped unequal labels to relabels.
[[nodiscard]] inline OrderedTreeEditWitness ordered_tree_edit_witness(
    const OrderedRootedTree& source,
    const OrderedRootedTree& target) {
  using namespace ordered_tree_edit_witness_detail;

  const PreparedTree prepared_source = prepare_validated(source);
  const PreparedTree prepared_target = prepare_validated(target);
  const std::size_t n = source.nodes.size();
  const std::size_t m = target.nodes.size();

  if (n > std::numeric_limits<std::size_t>::max() - m) {
    throw std::length_error(
        "ordered tree edit witness distance exceeds size_t range");
  }

  if (n == 0U || m == 0U) {
    return OrderedTreeEditWitness{n + m, {}};
  }

  const auto tree_distance =
      build_tree_distances(
          prepared_source, prepared_target, n, m);
  std::vector<std::pair<std::size_t, std::size_t>> mapping =
      recover_mapping(
          prepared_source, prepared_target, tree_distance, n, m);

  return OrderedTreeEditWitness{
      tree_distance[n][m], std::move(mapping)};
}

// Expensive exact certificate validator.
//
// It checks one-to-one mapping, ancestry/order preservation, certificate cost,
// and optimality against the merged exact distance implementation.
[[nodiscard]] inline bool valid_ordered_tree_edit_witness(
    const OrderedRootedTree& source,
    const OrderedRootedTree& target,
    const OrderedTreeEditWitness& witness) noexcept {
  using namespace ordered_tree_edit_witness_detail;

  try {
    const std::size_t optimum =
        ordered_tree_edit_distance(source, target);
    if (witness.distance != optimum) {
      return false;
    }

    const std::size_t n = source.nodes.size();
    const std::size_t m = target.nodes.size();
    if (witness.mapping.size() > n ||
        witness.mapping.size() > m) {
      return false;
    }

    std::vector<bool> source_used(n, false);
    std::vector<bool> target_used(m, false);
    std::size_t relabels = 0U;

    for (const auto& [source_node, target_node] :
         witness.mapping) {
      if (source_node >= n || target_node >= m ||
          source_used[source_node] ||
          target_used[target_node]) {
        return false;
      }
      source_used[source_node] = true;
      target_used[target_node] = true;
      if (source.nodes[source_node].label !=
          target.nodes[target_node].label) {
        ++relabels;
      }
    }

    const std::size_t mapped = witness.mapping.size();
    const std::size_t certificate_cost =
        (n - mapped) + (m - mapped) + relabels;
    if (certificate_cost != witness.distance) {
      return false;
    }

    std::vector<std::size_t> source_preorder;
    std::vector<std::size_t> source_end;
    std::vector<std::size_t> target_preorder;
    std::vector<std::size_t> target_end;
    preorder_intervals(
        source, source_preorder, source_end);
    preorder_intervals(
        target, target_preorder, target_end);

    const auto is_ancestor =
        [](const std::vector<std::size_t>& preorder,
           const std::vector<std::size_t>& subtree_end,
           const std::size_t first,
           const std::size_t second) {
          return preorder[first] < preorder[second] &&
                 preorder[second] < subtree_end[first];
        };
    const auto is_left_of =
        [](const std::vector<std::size_t>& preorder,
           const std::vector<std::size_t>& subtree_end,
           const std::size_t first,
           const std::size_t second) {
          return subtree_end[first] <= preorder[second];
        };

    for (std::size_t first = 0U;
         first < witness.mapping.size(); ++first) {
      const auto [source_first, target_first] =
          witness.mapping[first];

      for (std::size_t second = first + 1U;
           second < witness.mapping.size(); ++second) {
        const auto [source_second, target_second] =
            witness.mapping[second];

        if (is_ancestor(
                source_preorder, source_end,
                source_first, source_second) !=
            is_ancestor(
                target_preorder, target_end,
                target_first, target_second)) {
          return false;
        }
        if (is_ancestor(
                source_preorder, source_end,
                source_second, source_first) !=
            is_ancestor(
                target_preorder, target_end,
                target_second, target_first)) {
          return false;
        }
        if (is_left_of(
                source_preorder, source_end,
                source_first, source_second) !=
            is_left_of(
                target_preorder, target_end,
                target_first, target_second)) {
          return false;
        }
        if (is_left_of(
                source_preorder, source_end,
                source_second, source_first) !=
            is_left_of(
                target_preorder, target_end,
                target_second, target_first)) {
          return false;
        }
      }
    }

    return true;
  } catch (...) {
    return false;
  }
}

}  // namespace algorithms::dynamic_programming
