#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::coding {

struct AlphabeticBinaryTreeNode {
  std::optional<std::size_t> left;
  std::optional<std::size_t> right;
  std::optional<std::size_t> leaf_index;
};

struct OptimalAlphabeticBinaryTree {
  std::vector<std::size_t> leaf_depths;
  std::vector<AlphabeticBinaryTreeNode> nodes;
  std::optional<std::size_t> root;
  std::uint64_t weighted_external_path_length = 0;
  std::size_t compatibility_merges = 0;
};

namespace alphabetic_detail {

struct WorkEntry {
  bool infinite = false;
  std::uint64_t weight = 0;
  std::size_t node = 0;
};

inline bool less_equal(const WorkEntry& lhs, const WorkEntry& rhs) {
  if (lhs.infinite) {
    return rhs.infinite;
  }
  return rhs.infinite || lhs.weight <= rhs.weight;
}

inline bool greater_equal_finite(const WorkEntry& lhs, std::uint64_t rhs) {
  return lhs.infinite || lhs.weight >= rhs;
}

inline std::uint64_t checked_add(std::uint64_t lhs, std::uint64_t rhs) {
  if (rhs > std::numeric_limits<std::uint64_t>::max() - lhs) {
    throw std::overflow_error("alphabetic tree weight sum exceeds uint64_t");
  }
  return lhs + rhs;
}

inline std::uint64_t checked_weighted_add(std::uint64_t total,
                                          std::uint64_t weight,
                                          std::size_t depth) {
  if (weight != 0 && depth > std::numeric_limits<std::uint64_t>::max() / weight) {
    throw std::overflow_error("alphabetic tree weighted path length exceeds uint64_t");
  }
  const auto term = weight * static_cast<std::uint64_t>(depth);
  return checked_add(total, term);
}

}  // namespace alphabetic_detail

// Direct first-principles Garsia-Wachs construction for an optimal alphabetic
// full binary tree. This implementation deliberately keeps the active sequence
// in a vector, so insertion/search work is O(n^2) overall rather than importing
// the balanced-tree machinery needed for the classical O(n log n) bound.
inline OptimalAlphabeticBinaryTree optimal_alphabetic_binary_tree(
    std::span<const std::uint64_t> weights) {
  OptimalAlphabeticBinaryTree result;
  const std::size_t leaf_count = weights.size();
  result.leaf_depths.assign(leaf_count, 0);

  if (leaf_count == 0) {
    return result;
  }

  if (leaf_count > std::numeric_limits<std::size_t>::max() / 2 + std::size_t{1}) {
    throw std::length_error("alphabetic tree node count is not representable");
  }

  const auto iterator_limit =
      static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
  if (iterator_limit < 2 || leaf_count > iterator_limit - 2) {
    throw std::length_error("alphabetic tree working sequence is too large");
  }

  result.nodes.reserve(leaf_count * 2 - 1);
  for (std::size_t leaf = 0; leaf < leaf_count; ++leaf) {
    result.nodes.push_back(AlphabeticBinaryTreeNode{
        std::nullopt, std::nullopt, std::optional<std::size_t>{leaf}});
  }

  if (leaf_count == 1) {
    result.root = 0;
    return result;
  }

  using alphabetic_detail::WorkEntry;
  std::vector<WorkEntry> active;
  active.reserve(leaf_count + 2);
  active.push_back(WorkEntry{true, 0, 0});
  for (std::size_t leaf = 0; leaf < leaf_count; ++leaf) {
    active.push_back(WorkEntry{false, weights[leaf], leaf});
  }
  active.push_back(WorkEntry{true, 0, 0});

  // Phase 1: construct an optimal-depth tree whose leaf order may differ from
  // the caller's alphabetic order.
  while (active.size() > 3) {
    std::size_t first = 1;
    while (first + 2 < active.size() &&
           !alphabetic_detail::less_equal(active[first], active[first + 2])) {
      ++first;
    }
    if (first + 2 >= active.size() || active[first].infinite ||
        active[first + 1].infinite) {
      throw std::logic_error("Garsia-Wachs compatibility invariant failed");
    }

    const std::uint64_t merged_weight = alphabetic_detail::checked_add(
        active[first].weight, active[first + 1].weight);
    const std::size_t parent = result.nodes.size();
    result.nodes.push_back(AlphabeticBinaryTreeNode{
        std::optional<std::size_t>{active[first].node},
        std::optional<std::size_t>{active[first + 1].node}, std::nullopt});
    ++result.compatibility_merges;

    active.erase(active.begin() + static_cast<std::ptrdiff_t>(first),
                 active.begin() + static_cast<std::ptrdiff_t>(first + 2));

    std::size_t prior = first - 1;
    while (!alphabetic_detail::greater_equal_finite(active[prior], merged_weight)) {
      if (prior == 0) {
        throw std::logic_error("Garsia-Wachs left sentinel invariant failed");
      }
      --prior;
    }
    active.insert(active.begin() + static_cast<std::ptrdiff_t>(prior + 1),
                  WorkEntry{false, merged_weight, parent});
  }

  if (active.size() != 3 || active[1].infinite) {
    throw std::logic_error("Garsia-Wachs phase-1 root invariant failed");
  }

  // Phase 2: recover the optimal depth assigned to each original leaf.
  std::vector<std::pair<std::size_t, std::size_t>> pending;
  pending.push_back({active[1].node, 0});
  while (!pending.empty()) {
    const auto [node, depth] = pending.back();
    pending.pop_back();
    if (node < leaf_count) {
      result.leaf_depths[node] = depth;
      continue;
    }
    const auto& entry = result.nodes[node];
    if (!entry.left.has_value() || !entry.right.has_value() ||
        entry.leaf_index.has_value()) {
      throw std::logic_error("Garsia-Wachs phase-1 tree invariant failed");
    }
    if (depth == std::numeric_limits<std::size_t>::max()) {
      throw std::overflow_error("alphabetic tree depth is not representable");
    }
    pending.push_back({*entry.right, depth + 1});
    pending.push_back({*entry.left, depth + 1});
  }

  // Discard the phase-1 internal topology. Phase 3 reconstructs the unique
  // ordered full tree represented by the optimal leaf-depth sequence.
  result.nodes.resize(leaf_count);
  struct DepthNode {
    std::size_t node;
    std::size_t depth;
  };
  std::vector<DepthNode> stack;
  stack.reserve(leaf_count);
  for (std::size_t leaf = 0; leaf < leaf_count; ++leaf) {
    stack.push_back(DepthNode{leaf, result.leaf_depths[leaf]});
    while (stack.size() >= 2 &&
           stack[stack.size() - 1].depth == stack[stack.size() - 2].depth) {
      const DepthNode right = stack.back();
      stack.pop_back();
      const DepthNode left = stack.back();
      stack.pop_back();
      if (left.depth == 0) {
        throw std::logic_error("alphabetic depth sequence is not a full tree");
      }
      const std::size_t parent = result.nodes.size();
      result.nodes.push_back(AlphabeticBinaryTreeNode{
          std::optional<std::size_t>{left.node},
          std::optional<std::size_t>{right.node}, std::nullopt});
      stack.push_back(DepthNode{parent, left.depth - 1});
    }
  }

  if (stack.size() != 1 || stack.front().depth != 0 ||
      result.nodes.size() != leaf_count * 2 - 1) {
    throw std::logic_error("Garsia-Wachs depth reconstruction invariant failed");
  }
  result.root = stack.front().node;

  for (std::size_t leaf = 0; leaf < leaf_count; ++leaf) {
    result.weighted_external_path_length = alphabetic_detail::checked_weighted_add(
        result.weighted_external_path_length, weights[leaf], result.leaf_depths[leaf]);
  }
  return result;
}

}  // namespace algorithms::coding
