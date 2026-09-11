#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace algorithms::optimization {
namespace detail {

using SmawkMatrix = std::vector<std::vector<std::int64_t>>;

inline void validate_smawk_shape(const SmawkMatrix& matrix) {
  if (matrix.empty()) {
    return;
  }
  const std::size_t columns = matrix.front().size();
  if (columns == 0U) {
    throw std::invalid_argument("SMAWK requires at least one column");
  }
  for (const auto& row : matrix) {
    if (row.size() != columns) {
      throw std::invalid_argument("SMAWK requires a rectangular matrix");
    }
  }
}

inline std::size_t find_smawk_column_position(
    const std::vector<std::size_t>& columns, std::size_t begin,
    std::size_t target) {
  for (std::size_t index = begin; index < columns.size(); ++index) {
    if (columns[index] == target) {
      return index;
    }
  }
  throw std::invalid_argument(
      "matrix violates the totally monotone row-minimum ordering precondition");
}

inline void solve_smawk(const SmawkMatrix& matrix,
                        const std::vector<std::size_t>& rows,
                        const std::vector<std::size_t>& columns,
                        std::vector<std::size_t>& answer) {
  if (rows.empty()) {
    return;
  }

  std::vector<std::size_t> reduced;
  reduced.reserve(std::min(rows.size(), columns.size()));
  for (const std::size_t column : columns) {
    while (!reduced.empty()) {
      const std::size_t row = rows[reduced.size() - 1U];
      if (matrix[row][column] < matrix[row][reduced.back()]) {
        reduced.pop_back();
      } else {
        break;
      }
    }
    if (reduced.size() < rows.size()) {
      reduced.push_back(column);
    }
  }

  std::vector<std::size_t> odd_rows;
  odd_rows.reserve(rows.size() / 2U);
  for (std::size_t index = 1U; index < rows.size(); index += 2U) {
    odd_rows.push_back(rows[index]);
  }
  solve_smawk(matrix, odd_rows, reduced, answer);

  std::size_t lower = 0U;
  for (std::size_t row_index = 0U; row_index < rows.size(); row_index += 2U) {
    if (row_index > 0U) {
      lower = find_smawk_column_position(
          reduced, lower, answer[rows[row_index - 1U]]);
    }

    std::size_t upper = reduced.size() - 1U;
    if (row_index + 1U < rows.size()) {
      upper = find_smawk_column_position(
          reduced, lower, answer[rows[row_index + 1U]]);
    }

    const std::size_t row = rows[row_index];
    std::size_t best_position = lower;
    for (std::size_t position = lower + 1U; position <= upper; ++position) {
      if (matrix[row][reduced[position]] <
          matrix[row][reduced[best_position]]) {
        best_position = position;
      }
    }
    answer[row] = reduced[best_position];
    lower = best_position;
  }
}

}  // namespace detail

// Returns the leftmost minimum column for every row of a totally monotone
// matrix. The matrix must be rectangular. Empty matrices are supported; a
// non-empty matrix must have at least one column.
//
// Total monotonicity is a semantic precondition and is intentionally not
// validated here: a general validator would dominate the linear SMAWK access
// cost that this routine is meant to study.
[[nodiscard]] inline std::vector<std::size_t> smawk_row_minima(
    const std::vector<std::vector<std::int64_t>>& matrix) {
  detail::validate_smawk_shape(matrix);
  if (matrix.empty()) {
    return {};
  }

  std::vector<std::size_t> rows(matrix.size());
  std::iota(rows.begin(), rows.end(), std::size_t{0});
  std::vector<std::size_t> columns(matrix.front().size());
  std::iota(columns.begin(), columns.end(), std::size_t{0});

  std::vector<std::size_t> answer(matrix.size(), 0U);
  detail::solve_smawk(matrix, rows, columns, answer);
  return answer;
}

}  // namespace algorithms::optimization
