#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace algorithms::coding {

struct WideUnsignedCost {
  std::uint64_t high{0};
  std::uint64_t low{0};

  friend bool operator==(const WideUnsignedCost&, const WideUnsignedCost&) = default;
};

struct AlphabeticCodeResult {
  std::vector<std::size_t> depths;
  std::vector<std::string> codewords;
  WideUnsignedCost weighted_path_length{};
  std::size_t merge_count{0};
};

namespace detail {

inline WideUnsignedCost wide_add(WideUnsignedCost lhs, WideUnsignedCost rhs) {
  WideUnsignedCost result{};
  result.low = lhs.low + rhs.low;
  const std::uint64_t carry = result.low < lhs.low ? 1U : 0U;

  result.high = lhs.high + rhs.high;
  if (result.high < lhs.high) {
    throw std::overflow_error("alphabetic-code exact arithmetic overflow");
  }
  const std::uint64_t before_carry = result.high;
  result.high += carry;
  if (result.high < before_carry) {
    throw std::overflow_error("alphabetic-code exact arithmetic overflow");
  }
  return result;
}

inline bool wide_less(WideUnsignedCost lhs, WideUnsignedCost rhs) noexcept {
  return lhs.high < rhs.high || (lhs.high == rhs.high && lhs.low < rhs.low);
}

inline bool wide_less_equal(WideUnsignedCost lhs, WideUnsignedCost rhs) noexcept {
  return !wide_less(rhs, lhs);
}

inline WideUnsignedCost wide_mul_small(std::uint64_t value, std::size_t factor) {
  WideUnsignedCost result{};
  WideUnsignedCost term{0, value};
  std::size_t remaining = factor;
  while (remaining != 0U) {
    if ((remaining & std::size_t{1}) != 0U) {
      result = wide_add(result, term);
    }
    remaining >>= 1U;
    if (remaining != 0U) {
      term = wide_add(term, term);
    }
  }
  return result;
}

struct GarsiaNode {
  WideUnsignedCost weight{};
  std::size_t leaf_index{std::numeric_limits<std::size_t>::max()};
  std::size_t left{std::numeric_limits<std::size_t>::max()};
  std::size_t right{std::numeric_limits<std::size_t>::max()};
};

struct SequenceEntry {
  bool infinity{false};
  WideUnsignedCost weight{};
  std::size_t node{std::numeric_limits<std::size_t>::max()};
};

struct OrderedNode {
  std::size_t leaf_index{std::numeric_limits<std::size_t>::max()};
  std::size_t left{std::numeric_limits<std::size_t>::max()};
  std::size_t right{std::numeric_limits<std::size_t>::max()};
};

}  // namespace detail

// Direct educational Garsia-Wachs baseline for optimal alphabetic binary
// prefix coding. The O(n log n) data-structural acceleration is intentionally
// not used here; sequence search/insertion is explicit so the merge invariant
// stays inspectable. At most 4096 leaves are accepted to bound the quadratic
// work and the potentially quadratic total codeword output size.
inline AlphabeticCodeResult optimal_alphabetic_prefix_code(
    std::span<const std::uint64_t> weights) {
  constexpr std::size_t kMaxLeaves = 4096U;
  constexpr std::size_t kNoNode = std::numeric_limits<std::size_t>::max();

  if (weights.size() > kMaxLeaves) {
    throw std::length_error("alphabetic-code leaf limit exceeded");
  }

  AlphabeticCodeResult result{};
  const std::size_t n = weights.size();
  if (n == 0U) {
    return result;
  }
  if (n == 1U) {
    result.depths = {0U};
    result.codewords = {std::string{}};
    return result;
  }

  std::vector<detail::GarsiaNode> nodes;
  nodes.reserve(2U * n - 1U);
  std::vector<detail::SequenceEntry> sequence;
  sequence.reserve(n + 2U);
  sequence.push_back(detail::SequenceEntry{true, {}, kNoNode});

  for (std::size_t index = 0; index < n; ++index) {
    const std::size_t node_id = nodes.size();
    nodes.push_back(detail::GarsiaNode{{0, weights[index]}, index, kNoNode, kNoNode});
    sequence.push_back(detail::SequenceEntry{false, {0, weights[index]}, node_id});
  }
  sequence.push_back(detail::SequenceEntry{true, {}, kNoNode});

  std::size_t forest_size = n;
  while (forest_size > 1U) {
    std::size_t position = 1U;
    for (; position + 2U < sequence.size(); ++position) {
      const auto& first = sequence[position];
      const auto& third = sequence[position + 2U];
      if (first.infinity) {
        throw std::logic_error("alphabetic-code internal sentinel corruption");
      }
      if (third.infinity || detail::wide_less_equal(first.weight, third.weight)) {
        break;
      }
    }
    if (position + 2U >= sequence.size() || sequence[position + 1U].infinity) {
      throw std::logic_error("alphabetic-code compatible pair not found");
    }

    const auto first = sequence[position];
    const auto second = sequence[position + 1U];
    const WideUnsignedCost merged_weight =
        detail::wide_add(first.weight, second.weight);
    const std::size_t merged_node = nodes.size();
    nodes.push_back(detail::GarsiaNode{merged_weight, kNoNode, first.node, second.node});

    sequence.erase(sequence.begin() + static_cast<std::ptrdiff_t>(position),
                   sequence.begin() + static_cast<std::ptrdiff_t>(position + 2U));

    std::size_t insertion_left = position - 1U;
    while (!sequence[insertion_left].infinity &&
           detail::wide_less(sequence[insertion_left].weight, merged_weight)) {
      --insertion_left;
    }
    sequence.insert(sequence.begin() + static_cast<std::ptrdiff_t>(insertion_left + 1U),
                    detail::SequenceEntry{false, merged_weight, merged_node});
    --forest_size;
    ++result.merge_count;
  }

  std::size_t root = kNoNode;
  for (const auto& entry : sequence) {
    if (!entry.infinity) {
      if (root != kNoNode) {
        throw std::logic_error("alphabetic-code final forest is not singleton");
      }
      root = entry.node;
    }
  }
  if (root == kNoNode) {
    throw std::logic_error("alphabetic-code final tree missing");
  }

  result.depths.assign(n, 0U);
  std::vector<std::pair<std::size_t, std::size_t>> traversal;
  traversal.push_back({root, 0U});
  while (!traversal.empty()) {
    const auto [node_id, depth] = traversal.back();
    traversal.pop_back();
    const auto& node = nodes[node_id];
    if (node.leaf_index != kNoNode) {
      result.depths[node.leaf_index] = depth;
      continue;
    }
    if (node.left == kNoNode || node.right == kNoNode) {
      throw std::logic_error("alphabetic-code malformed merge tree");
    }
    traversal.push_back({node.right, depth + 1U});
    traversal.push_back({node.left, depth + 1U});
  }

  std::vector<detail::OrderedNode> ordered_nodes;
  ordered_nodes.reserve(2U * n - 1U);
  std::vector<std::pair<std::size_t, std::size_t>> depth_stack;
  depth_stack.reserve(n);
  for (std::size_t leaf = 0; leaf < n; ++leaf) {
    const std::size_t leaf_node = ordered_nodes.size();
    ordered_nodes.push_back(detail::OrderedNode{leaf, kNoNode, kNoNode});
    depth_stack.push_back({result.depths[leaf], leaf_node});

    while (depth_stack.size() >= 2U &&
           depth_stack[depth_stack.size() - 1U].first ==
               depth_stack[depth_stack.size() - 2U].first) {
      const auto right_entry = depth_stack.back();
      depth_stack.pop_back();
      const auto left_entry = depth_stack.back();
      depth_stack.pop_back();
      if (left_entry.first == 0U) {
        throw std::logic_error("alphabetic-code invalid leaf-depth sequence");
      }
      const std::size_t parent = ordered_nodes.size();
      ordered_nodes.push_back(
          detail::OrderedNode{kNoNode, left_entry.second, right_entry.second});
      depth_stack.push_back({left_entry.first - 1U, parent});
    }
  }
  if (depth_stack.size() != 1U || depth_stack.front().first != 0U) {
    throw std::logic_error("alphabetic-code depths cannot form ordered full tree");
  }

  result.codewords.resize(n);
  std::vector<std::pair<std::size_t, std::string>> ordered_traversal;
  ordered_traversal.push_back({depth_stack.front().second, std::string{}});
  while (!ordered_traversal.empty()) {
    auto current = std::move(ordered_traversal.back());
    ordered_traversal.pop_back();
    const auto& node = ordered_nodes[current.first];
    if (node.leaf_index != kNoNode) {
      if (current.second.size() != result.depths[node.leaf_index]) {
        throw std::logic_error("alphabetic-code reconstructed depth mismatch");
      }
      result.codewords[node.leaf_index] = std::move(current.second);
      continue;
    }
    if (node.left == kNoNode || node.right == kNoNode) {
      throw std::logic_error("alphabetic-code malformed ordered tree");
    }
    std::string right_code = current.second;
    right_code.push_back('1');
    current.second.push_back('0');
    ordered_traversal.push_back({node.right, std::move(right_code)});
    ordered_traversal.push_back({node.left, std::move(current.second)});
  }

  WideUnsignedCost total{};
  for (std::size_t index = 0; index < n; ++index) {
    total = detail::wide_add(total,
                             detail::wide_mul_small(weights[index], result.depths[index]));
  }
  result.weighted_path_length = total;
  return result;
}

}  // namespace algorithms::coding
