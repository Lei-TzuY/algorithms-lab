#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace algorithms::optimization {

using AssignmentCost = std::int64_t;

struct AssignmentResult {
  AssignmentCost cost;
  std::vector<std::size_t> column_for_row;
};

// Minimum-cost injective assignment of every row to a distinct column.
//
// `cost_matrix` is row-major with `row_count * column_count` entries.
// Empty row sets return an empty zero-cost assignment. For non-empty rows,
// column_count must be positive and row_count <= column_count.
//
// The implementation is the O(rows^2 * columns) rectangular Hungarian
// shortest-augmenting-path algorithm. All potential/slack/total-cost arithmetic
// is checked; unrepresentable intermediate or result values throw
// std::overflow_error.
[[nodiscard]] AssignmentResult hungarian_min_assignment(
    std::size_t row_count, std::size_t column_count,
    std::span<const AssignmentCost> cost_matrix);

}  // namespace algorithms::optimization
