#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::data_structures {

// Fenwick tree (binary indexed tree) for signed 64-bit point deltas and range
// sums over zero-based half-open intervals.
//
// Internal invariant: one-based bucket i stores the exact sum of the logical
// array interval [i - lowbit(i), i). Every internal bucket must remain
// representable in int64_t. add() preflights every affected bucket before
// mutating any state, so an overflow leaves the tree unchanged.
//
// add: O(log n), prefix_sum/range_sum: O(log n), storage: O(n).
class FenwickTree {
 public:
  explicit FenwickTree(std::size_t element_count)
      : size_(element_count), tree_(storage_size(element_count), 0) {}

  [[nodiscard]] std::size_t size() const noexcept { return size_; }

  void add(std::size_t index, std::int64_t delta) {
    validate_index(index);

    // First pass provides a strong exception guarantee for arithmetic errors.
    for (std::size_t one_based = index + 1U; one_based <= size_;) {
      (void)checked_add(tree_[one_based], delta);
      const std::size_t step = lowbit(one_based);
      if (step > size_ - one_based) {
        break;
      }
      one_based += step;
    }

    for (std::size_t one_based = index + 1U; one_based <= size_;) {
      tree_[one_based] = checked_add(tree_[one_based], delta);
      const std::size_t step = lowbit(one_based);
      if (step > size_ - one_based) {
        break;
      }
      one_based += step;
    }
  }

  // Returns the exact sum over [0, end). end may equal size().
  [[nodiscard]] std::int64_t prefix_sum(std::size_t end) const {
    if (end > size_) {
      throw std::out_of_range("Fenwick prefix end out of range");
    }

    // A prefix decomposes into at most one bucket per size_t bit. Separate
    // signs and alternate them so a representable final sum is not rejected
    // merely because the conventional fixed aggregation order transiently
    // exceeds int64_t.
    constexpr std::size_t max_terms =
        std::numeric_limits<std::size_t>::digits;
    std::array<std::int64_t, max_terms> positives{};
    std::array<std::int64_t, max_terms> negatives{};
    std::size_t positive_count = 0;
    std::size_t negative_count = 0;

    for (std::size_t one_based = end; one_based > 0;
         one_based -= lowbit(one_based)) {
      const std::int64_t value = tree_[one_based];
      if (value < 0) {
        negatives[negative_count++] = value;
      } else {
        positives[positive_count++] = value;
      }
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

  // Returns the exact sum over [begin, end). Both endpoints are zero-based;
  // end may equal size().
  [[nodiscard]] std::int64_t range_sum(std::size_t begin,
                                       std::size_t end) const {
    if (begin > end || end > size_) {
      throw std::out_of_range("Fenwick range out of range");
    }
    return checked_subtract(prefix_sum(end), prefix_sum(begin));
  }

 private:
  static std::size_t storage_size(std::size_t element_count) {
    if (element_count == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("Fenwick tree size is too large");
    }
    return element_count + 1U;
  }

  static std::size_t lowbit(std::size_t value) noexcept {
    return value & (~value + std::size_t{1});
  }

  static std::int64_t checked_add(std::int64_t left, std::int64_t right) {
    if ((right > 0 &&
         left > std::numeric_limits<std::int64_t>::max() - right) ||
        (right < 0 &&
         left < std::numeric_limits<std::int64_t>::min() - right)) {
      throw std::overflow_error("Fenwick sum overflow");
    }
    return left + right;
  }

  static std::int64_t checked_subtract(std::int64_t left,
                                       std::int64_t right) {
    if ((right > 0 &&
         left < std::numeric_limits<std::int64_t>::min() + right) ||
        (right < 0 &&
         left > std::numeric_limits<std::int64_t>::max() + right)) {
      throw std::overflow_error("Fenwick range sum overflow");
    }
    return left - right;
  }

  void validate_index(std::size_t index) const {
    if (index >= size_) {
      throw std::out_of_range("Fenwick index out of range");
    }
  }

  std::size_t size_;
  std::vector<std::int64_t> tree_;
};

}  // namespace algorithms::data_structures
