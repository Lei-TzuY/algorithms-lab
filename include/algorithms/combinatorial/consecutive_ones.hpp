#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::combinatorial {

struct ConsecutiveOnesResult {
  std::vector<std::size_t> column_order;
  std::size_t explored_states{};
  std::size_t transition_checks{};

  friend bool operator==(const ConsecutiveOnesResult&,
                         const ConsecutiveOnesResult&) = default;
};

namespace consecutive_ones_detail {

class Solver {
 public:
  Solver(std::size_t column_count, std::vector<std::uint32_t> rows)
      : column_count_(column_count),
        rows_(std::move(rows)),
        full_mask_(column_count == 0U
                       ? std::uint32_t{0}
                       : (std::uint32_t{1} << column_count) - 1U),
        memo_(std::size_t{1} << column_count, std::int8_t{-1}),
        choice_(std::size_t{1} << column_count,
                std::numeric_limits<std::uint8_t>::max()) {}

  [[nodiscard]] std::optional<ConsecutiveOnesResult> run() {
    if (!solve(0U)) {
      return std::nullopt;
    }
    ConsecutiveOnesResult result;
    result.explored_states = explored_states_;
    result.transition_checks = transition_checks_;
    result.column_order.reserve(column_count_);
    std::uint32_t used = 0U;
    while (used != full_mask_) {
      const std::uint8_t next = choice_[used];
      if (next == std::numeric_limits<std::uint8_t>::max()) {
        throw std::logic_error("consecutive-ones reconstruction state missing");
      }
      result.column_order.push_back(static_cast<std::size_t>(next));
      used |= std::uint32_t{1} << next;
    }
    return result;
  }

 private:
  [[nodiscard]] bool transition_allowed(std::uint32_t used,
                                        std::uint32_t next_bit) const noexcept {
    for (const std::uint32_t row : rows_) {
      const std::uint32_t placed = row & used;
      if (placed != 0U && placed != row && (row & next_bit) == 0U) {
        return false;
      }
    }
    return true;
  }

  [[nodiscard]] bool solve(std::uint32_t used) {
    std::int8_t& cached = memo_[used];
    if (cached != -1) {
      return cached == 1;
    }
    if (explored_states_ == std::numeric_limits<std::size_t>::max()) {
      throw std::overflow_error("consecutive-ones state counter overflow");
    }
    ++explored_states_;
    if (used == full_mask_) {
      cached = 1;
      return true;
    }

    for (std::size_t column = 0; column < column_count_; ++column) {
      const std::uint32_t bit = std::uint32_t{1} << column;
      if ((used & bit) != 0U) {
        continue;
      }
      if (transition_checks_ == std::numeric_limits<std::size_t>::max()) {
        throw std::overflow_error("consecutive-ones transition counter overflow");
      }
      ++transition_checks_;
      if (!transition_allowed(used, bit)) {
        continue;
      }
      if (solve(used | bit)) {
        choice_[used] = static_cast<std::uint8_t>(column);
        cached = 1;
        return true;
      }
    }
    cached = 0;
    return false;
  }

  std::size_t column_count_;
  std::vector<std::uint32_t> rows_;
  std::uint32_t full_mask_;
  std::vector<std::int8_t> memo_;
  std::vector<std::uint8_t> choice_;
  std::size_t explored_states_{};
  std::size_t transition_checks_{};
};

}  // namespace consecutive_ones_detail

// Bounded exact recognition of the consecutive-ones property for a binary
// row/column incidence matrix. Returns the lexicographically smallest column
// permutation whose 1s are consecutive in every row, or nullopt if none exists.
//
// This is deliberately an exponential educational baseline, not a PQ-tree or
// PC-tree implementation. The public bound keeps the 2^n state table explicit.
[[nodiscard]] inline std::optional<ConsecutiveOnesResult>
consecutive_ones_ordering(
    std::size_t column_count,
    const std::vector<std::vector<std::size_t>>& one_columns_by_row) {
  constexpr std::size_t kMaxColumns = 20U;
  if (column_count > kMaxColumns) {
    throw std::length_error("consecutive-ones baseline supports at most 20 columns");
  }

  std::vector<std::uint32_t> row_masks;
  row_masks.reserve(one_columns_by_row.size());
  for (const auto& row : one_columns_by_row) {
    std::uint32_t mask = 0U;
    for (const std::size_t column : row) {
      if (column >= column_count) {
        throw std::out_of_range("consecutive-ones column index out of range");
      }
      const std::uint32_t bit = std::uint32_t{1} << column;
      if ((mask & bit) != 0U) {
        throw std::invalid_argument("consecutive-ones row contains duplicate column");
      }
      mask |= bit;
    }
    row_masks.push_back(mask);
  }
  std::sort(row_masks.begin(), row_masks.end());
  row_masks.erase(std::unique(row_masks.begin(), row_masks.end()), row_masks.end());

  return consecutive_ones_detail::Solver(column_count, std::move(row_masks)).run();
}

}  // namespace algorithms::combinatorial
