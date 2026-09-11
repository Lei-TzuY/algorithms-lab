#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::data_structures {

// Segment Tree Beats for nonnegative signed-64 values.
// Public ranges are zero-based and half-open.
//
// Supported operations:
// - range_chmin(begin, end, cap): a[i] = min(a[i], cap)
// - range_sum(begin, end): exact signed-64 sum
//
// Construction rejects negative values and any initial subtree sum that is not
// representable in int64_t. Caps must also be nonnegative. Under this contract,
// chmin is monotone and stored sums can only decrease, so a successful build
// cannot later overflow a stored sum.
class SegmentTreeBeats {
 public:
  explicit SegmentTreeBeats(const std::vector<std::int64_t>& values)
      : size_(values.size()) {
    for (const std::int64_t value : values) {
      if (value < 0) {
        throw std::invalid_argument(
            "segment-tree-beats values must be nonnegative");
      }
    }
    if (size_ == 0U) {
      tree_.resize(1U);
      return;
    }
    if (size_ > std::numeric_limits<std::size_t>::max() / 4U) {
      throw std::length_error("segment-tree-beats size is too large");
    }
    tree_.resize(size_ * 4U);
    build(1U, 0U, size_, values);
  }

  [[nodiscard]] std::size_t size() const noexcept { return size_; }

  void range_chmin(std::size_t begin, std::size_t end, std::int64_t cap) {
    validate_range(begin, end);
    if (cap < 0) {
      throw std::invalid_argument("segment-tree-beats cap must be nonnegative");
    }
    if (begin == end || size_ == 0U) {
      return;
    }
    range_chmin_impl(1U, 0U, size_, begin, end, cap);
  }

  [[nodiscard]] std::int64_t range_sum(std::size_t begin,
                                       std::size_t end) const {
    validate_range(begin, end);
    if (begin == end || size_ == 0U) {
      return 0;
    }
    return range_sum_impl(1U, 0U, size_, begin, end,
                          std::numeric_limits<std::int64_t>::max());
  }

 private:
  struct Node {
    std::int64_t sum = 0;
    std::int64_t maximum = 0;
    std::int64_t second_maximum = -1;
    std::size_t maximum_count = 0;
  };

  static std::int64_t checked_add_nonnegative(std::int64_t left,
                                               std::int64_t right) {
    if (left < 0 || right < 0 ||
        left > std::numeric_limits<std::int64_t>::max() - right) {
      throw std::overflow_error("segment-tree-beats sum overflow");
    }
    return left + right;
  }

  static std::int64_t checked_product_nonnegative(std::int64_t value,
                                                   std::size_t count) {
    if (value < 0) {
      throw std::logic_error("segment-tree-beats negative product input");
    }
    if (value == 0 || count == 0U) {
      return 0;
    }
    const auto maximum =
        static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max());
    const auto unsigned_value = static_cast<std::uint64_t>(value);
    if (count > maximum / unsigned_value) {
      throw std::overflow_error("segment-tree-beats product overflow");
    }
    return static_cast<std::int64_t>(unsigned_value * count);
  }

  static Node merge_nodes(const Node& left, const Node& right) {
    Node merged;
    merged.sum = checked_add_nonnegative(left.sum, right.sum);
    if (left.maximum == right.maximum) {
      merged.maximum = left.maximum;
      merged.maximum_count = left.maximum_count + right.maximum_count;
      merged.second_maximum =
          std::max(left.second_maximum, right.second_maximum);
      return merged;
    }
    if (left.maximum > right.maximum) {
      merged.maximum = left.maximum;
      merged.maximum_count = left.maximum_count;
      merged.second_maximum = std::max(left.second_maximum, right.maximum);
      return merged;
    }
    merged.maximum = right.maximum;
    merged.maximum_count = right.maximum_count;
    merged.second_maximum = std::max(left.maximum, right.second_maximum);
    return merged;
  }

  void build(std::size_t node, std::size_t left, std::size_t right,
             const std::vector<std::int64_t>& values) {
    if (right - left == 1U) {
      tree_[node] = Node{values[left], values[left], -1, 1U};
      return;
    }
    const std::size_t middle = left + (right - left) / 2U;
    build(node * 2U, left, middle, values);
    build(node * 2U + 1U, middle, right, values);
    tree_[node] = merge_nodes(tree_[node * 2U], tree_[node * 2U + 1U]);
  }

  void apply_node_chmin(std::size_t node, std::int64_t cap) {
    Node& current = tree_[node];
    if (cap >= current.maximum) {
      return;
    }
    if (!(current.second_maximum < cap)) {
      throw std::logic_error("segment-tree-beats invalid shortcut");
    }
    const std::int64_t difference = current.maximum - cap;
    const std::int64_t decrease =
        checked_product_nonnegative(difference, current.maximum_count);
    if (decrease > current.sum) {
      throw std::logic_error("segment-tree-beats sum invariant violated");
    }
    current.sum -= decrease;
    current.maximum = cap;
  }

  void push(std::size_t node) {
    const std::int64_t cap = tree_[node].maximum;
    const std::size_t left_child = node * 2U;
    const std::size_t right_child = left_child + 1U;
    if (tree_[left_child].maximum > cap) {
      apply_node_chmin(left_child, cap);
    }
    if (tree_[right_child].maximum > cap) {
      apply_node_chmin(right_child, cap);
    }
  }

  void range_chmin_impl(std::size_t node, std::size_t left,
                        std::size_t right, std::size_t query_left,
                        std::size_t query_right, std::int64_t cap) {
    Node& current = tree_[node];
    if (query_right <= left || right <= query_left || current.maximum <= cap) {
      return;
    }
    if (query_left <= left && right <= query_right &&
        current.second_maximum < cap) {
      apply_node_chmin(node, cap);
      return;
    }

    push(node);
    const std::size_t middle = left + (right - left) / 2U;
    range_chmin_impl(node * 2U, left, middle, query_left, query_right, cap);
    range_chmin_impl(node * 2U + 1U, middle, right, query_left, query_right,
                     cap);
    current = merge_nodes(tree_[node * 2U], tree_[node * 2U + 1U]);
  }

  [[nodiscard]] static std::int64_t sum_under_cap(const Node& node,
                                                   std::int64_t cap) {
    if (cap >= node.maximum) {
      return node.sum;
    }
    if (!(node.second_maximum < cap)) {
      throw std::logic_error("segment-tree-beats inherited cap invariant");
    }
    const std::int64_t difference = node.maximum - cap;
    const std::int64_t decrease =
        checked_product_nonnegative(difference, node.maximum_count);
    if (decrease > node.sum) {
      throw std::logic_error("segment-tree-beats sum invariant violated");
    }
    return node.sum - decrease;
  }

  [[nodiscard]] std::int64_t range_sum_impl(
      std::size_t node, std::size_t left, std::size_t right,
      std::size_t query_left, std::size_t query_right,
      std::int64_t inherited_cap) const {
    if (query_right <= left || right <= query_left) {
      return 0;
    }
    const Node& current = tree_[node];
    if (query_left <= left && right <= query_right) {
      return sum_under_cap(current, inherited_cap);
    }

    const std::int64_t child_cap = std::min(inherited_cap, current.maximum);
    const std::size_t middle = left + (right - left) / 2U;
    return checked_add_nonnegative(
        range_sum_impl(node * 2U, left, middle, query_left, query_right,
                       child_cap),
        range_sum_impl(node * 2U + 1U, middle, right, query_left, query_right,
                       child_cap));
  }

  void validate_range(std::size_t begin, std::size_t end) const {
    if (begin > end || end > size_) {
      throw std::out_of_range("segment-tree-beats range out of range");
    }
  }

  std::size_t size_ = 0;
  std::vector<Node> tree_;
};

}  // namespace algorithms::data_structures
