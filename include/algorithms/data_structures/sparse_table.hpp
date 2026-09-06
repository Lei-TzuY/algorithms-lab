#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

struct SparseTableMinResult {
  std::int64_t value;
  std::size_t index;
};

// Immutable sparse table for range-minimum queries with deterministic argmin.
// Public indices are zero-based and ranges are half-open.
//
// Level k stores the minimum entry for every interval of length 2^k. A query
// over a non-empty range uses two possibly overlapping 2^k blocks. Because
// minimum is idempotent, overlap cannot change the answer.
//
// Construction: O(n log n), range_min: O(1), storage: O(n log n).
class SparseTableMin {
 public:
  explicit SparseTableMin(const std::vector<std::int64_t>& values)
      : size_(values.size()),
        logs_(checked_log_table_size(values.size()), std::size_t{0}) {
    for (std::size_t length = 2; length <= size_; ++length) {
      logs_[length] = logs_[length / 2U] + 1U;
    }

    if (size_ == 0U) {
      return;
    }

    std::vector<Entry> base_level;
    base_level.reserve(size_);
    for (std::size_t index = 0; index < size_; ++index) {
      base_level.push_back(Entry{values[index], index});
    }
    levels_.push_back(std::move(base_level));

    std::size_t half_span = 1U;
    while (half_span <= size_ / 2U) {
      const std::size_t span = half_span * 2U;
      const std::size_t entry_count = size_ - span + 1U;
      const std::vector<Entry>& previous = levels_.back();
      std::vector<Entry> next(entry_count);
      for (std::size_t begin = 0; begin < entry_count; ++begin) {
        next[begin] = better(previous[begin], previous[begin + half_span]);
      }
      levels_.push_back(std::move(next));
      half_span = span;
    }
  }

  [[nodiscard]] std::size_t size() const noexcept { return size_; }

  [[nodiscard]] SparseTableMinResult range_min(std::size_t begin,
                                                std::size_t end) const {
    if (begin >= end || end > size_) {
      throw std::out_of_range("sparse-table range must be non-empty and valid");
    }

    const std::size_t length = end - begin;
    const std::size_t level = logs_[length];
    const std::size_t span = std::size_t{1} << level;
    const Entry answer =
        better(levels_[level][begin], levels_[level][end - span]);
    return SparseTableMinResult{answer.value, answer.index};
  }

 private:
  struct Entry {
    std::int64_t value = 0;
    std::size_t index = 0;
  };

  static std::size_t checked_log_table_size(std::size_t size) {
    if (size == std::numeric_limits<std::size_t>::max()) {
      throw std::length_error("sparse table size is too large");
    }
    return size + 1U;
  }

  static Entry better(const Entry& left, const Entry& right) noexcept {
    if (left.value < right.value) {
      return left;
    }
    if (right.value < left.value) {
      return right;
    }
    return left.index <= right.index ? left : right;
  }

  std::size_t size_;
  std::vector<std::size_t> logs_;
  std::vector<std::vector<Entry>> levels_;
};

}  // namespace algorithms::data_structures
