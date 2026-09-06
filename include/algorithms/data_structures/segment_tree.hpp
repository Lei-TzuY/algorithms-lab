#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::data_structures {

// Bottom-up segment tree for signed 64-bit range sums with point assignment.
// Public indices are zero-based and ranges are half-open.
//
// Invariant: every internal node stores the exact sum of its two child
// intervals, and every stored node sum is representable in int64_t. Leaves
// beyond size() up to the power-of-two base are zero padding.
//
// assign/range_sum: O(log n), construction: O(n), storage: O(n).
class SegmentTree {
 public:
  explicit SegmentTree(std::size_t element_count)
      : size_(element_count),
        base_(compute_base(element_count)),
        tree_(storage_size(base_), 0) {}

  explicit SegmentTree(const std::vector<std::int64_t>& values)
      : size_(values.size()),
        base_(compute_base(values.size())),
        tree_(storage_size(base_), 0) {
    for (std::size_t index = 0; index < values.size(); ++index) {
      tree_[base_ + index] = values[index];
    }
    for (std::size_t node = base_; node > 1U;) {
      --node;
      tree_[node] = checked_add(tree_[node * 2U], tree_[node * 2U + 1U]);
    }
  }

  [[nodiscard]] std::size_t size() const noexcept { return size_; }

  // Replaces one logical value. The full ancestor path is precomputed with
  // checked arithmetic before any stored node is changed, so arithmetic
  // failure leaves the entire tree unchanged.
  void assign(std::size_t index, std::int64_t value) {
    validate_index(index);

    constexpr std::size_t max_depth =
        std::numeric_limits<std::size_t>::digits;
    std::array<std::size_t, max_depth> ancestor_nodes{};
    std::array<std::int64_t, max_depth> ancestor_values{};
    std::size_t ancestor_count = 0;

    std::size_t node = base_ + index;
    std::int64_t current_value = value;
    while (node > 1U) {
      const bool is_left_child = (node % 2U) == 0U;
      const std::size_t sibling = is_left_child ? node + 1U : node - 1U;
      const std::int64_t parent_value =
          is_left_child ? checked_add(current_value, tree_[sibling])
                        : checked_add(tree_[sibling], current_value);

      node /= 2U;
      ancestor_nodes[ancestor_count] = node;
      ancestor_values[ancestor_count] = parent_value;
      ++ancestor_count;
      current_value = parent_value;
    }

    tree_[base_ + index] = value;
    for (std::size_t offset = 0; offset < ancestor_count; ++offset) {
      tree_[ancestor_nodes[offset]] = ancestor_values[offset];
    }
  }

  // Returns the exact sum over [begin, end). An exact mathematical answer
  // outside int64_t is reported as overflow.
  [[nodiscard]] std::int64_t range_sum(std::size_t begin,
                                       std::size_t end) const {
    if (begin > end || end > size_) {
      throw std::out_of_range("segment-tree range out of range");
    }

    constexpr std::size_t max_terms =
        2U * std::numeric_limits<std::size_t>::digits;
    std::array<std::int64_t, max_terms> positives{};
    std::array<std::int64_t, max_terms> negatives{};
    std::size_t positive_count = 0;
    std::size_t negative_count = 0;

    const auto collect = [&](std::int64_t term) {
      if (term < 0) {
        negatives[negative_count++] = term;
      } else {
        positives[positive_count++] = term;
      }
    };

    std::size_t left = base_ + begin;
    std::size_t right = base_ + end;
    while (left < right) {
      if ((left % 2U) != 0U) {
        collect(tree_[left]);
        ++left;
      }
      if ((right % 2U) != 0U) {
        --right;
        collect(tree_[right]);
      }
      left /= 2U;
      right /= 2U;
    }

    std::int64_t sum = 0;
    while (positive_count > 0 || negative_count > 0) {
      if (sum >= 0 && negative_count > 0) {
        sum = checked_add(sum, negatives[--negative_count]);
      } else if (sum < 0 && positive_count > 0) {
        sum = checked_add(sum, positives[--positive_count]);
      } else if (positive_count > 0) {
        sum = checked_add(sum, positives[--positive_count]);
      } else {
        sum = checked_add(sum, negatives[--negative_count]);
      }
    }
    return sum;
  }

 private:
  static std::size_t compute_base(std::size_t element_count) {
    const std::size_t maximum = std::numeric_limits<std::size_t>::max();
    std::size_t base = 1U;
    while (base < element_count) {
      if (base > maximum / 2U) {
        throw std::length_error("segment tree size is too large");
      }
      base *= 2U;
    }
    if (base > maximum / 2U) {
      throw std::length_error("segment tree size is too large");
    }
    return base;
  }

  static std::size_t storage_size(std::size_t base) {
    return base * 2U;
  }

  static std::int64_t checked_add(std::int64_t left, std::int64_t right) {
    if ((right > 0 &&
         left > std::numeric_limits<std::int64_t>::max() - right) ||
        (right < 0 &&
         left < std::numeric_limits<std::int64_t>::min() - right)) {
      throw std::overflow_error("segment-tree sum overflow");
    }
    return left + right;
  }

  void validate_index(std::size_t index) const {
    if (index >= size_) {
      throw std::out_of_range("segment-tree index out of range");
    }
  }

  std::size_t size_;
  std::size_t base_;
  std::vector<std::int64_t> tree_;
};

}  // namespace algorithms::data_structures
