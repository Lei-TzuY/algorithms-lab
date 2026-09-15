#pragma once

#include <bit>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::data_structures {

// Immutable O(1)-query range index for an associative binary operation.
// Public ranges are zero-based and half-open [begin, end).
//
// Unlike an overlapping sparse table, this structure does not require the
// operation to be idempotent or commutative. For each level, every power-of-two
// block stores left-half suffix aggregates and right-half prefix aggregates.
// A non-singleton query chooses the level of the highest differing bit between
// begin and end - 1; those endpoints lie on opposite sides of exactly one
// midpoint at that level, so one ordered combine yields the whole range.
//
// Construction: O(n log n), query: O(1), storage: O(n log n).
// Associativity is a caller precondition and is not checked dynamically.
template <typename T, typename BinaryOperation = std::plus<T>>
class DisjointSparseTable {
 public:
  explicit DisjointSparseTable(std::vector<T> values,
                               BinaryOperation operation = BinaryOperation{})
      : values_(std::move(values)), operation_(std::move(operation)) {
    if (values_.size() <= 1U) {
      return;
    }

    const std::size_t level_count =
        static_cast<std::size_t>(std::bit_width(values_.size() - 1U));
    levels_.reserve(level_count);

    for (std::size_t level = 0; level < level_count; ++level) {
      levels_.push_back(values_);
      const std::size_t half_span = std::size_t{1} << level;

      std::size_t block_begin = 0U;
      while (block_begin < values_.size()) {
        const std::size_t remaining_from_begin = values_.size() - block_begin;
        const std::size_t midpoint =
            half_span >= remaining_from_begin ? values_.size()
                                              : block_begin + half_span;
        const std::size_t remaining_from_midpoint = values_.size() - midpoint;
        const std::size_t block_end =
            half_span >= remaining_from_midpoint ? values_.size()
                                                 : midpoint + half_span;

        if (midpoint > block_begin) {
          levels_[level][midpoint - 1U] = values_[midpoint - 1U];
          for (std::size_t index = midpoint - 1U; index > block_begin; --index) {
            levels_[level][index - 1U] =
                operation_(values_[index - 1U], levels_[level][index]);
          }
        }

        if (midpoint < block_end) {
          levels_[level][midpoint] = values_[midpoint];
          for (std::size_t index = midpoint + 1U; index < block_end; ++index) {
            levels_[level][index] =
                operation_(levels_[level][index - 1U], values_[index]);
          }
        }

        if (block_end == values_.size()) {
          break;
        }
        block_begin = block_end;
      }
    }
  }

  [[nodiscard]] std::size_t size() const noexcept { return values_.size(); }

  [[nodiscard]] T query(std::size_t begin, std::size_t end) const {
    if (begin >= end || end > values_.size()) {
      throw std::out_of_range(
          "disjoint sparse-table range must be non-empty and valid");
    }
    if (end - begin == 1U) {
      return values_[begin];
    }

    const std::size_t differing = begin ^ (end - 1U);
    const std::size_t level =
        static_cast<std::size_t>(std::bit_width(differing)) - 1U;
    return operation_(levels_[level][begin], levels_[level][end - 1U]);
  }

 private:
  std::vector<T> values_;
  BinaryOperation operation_;
  std::vector<std::vector<T>> levels_;
};

}  // namespace algorithms::data_structures
