#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::linear_algebra {

using SmithMatrix = std::vector<std::vector<std::int64_t>>;

struct SmithNormalFormResult {
  SmithMatrix diagonal;
  SmithMatrix left_transform;
  SmithMatrix right_transform;
  std::vector<std::int64_t> invariant_factors;
  std::size_t rank = 0;
};

namespace smith_detail {

[[nodiscard]] inline std::uint64_t magnitude(std::int64_t value) noexcept {
  if (value >= 0) return static_cast<std::uint64_t>(value);
  return static_cast<std::uint64_t>(-(value + 1)) + 1U;
}

[[nodiscard]] inline std::int64_t checked_add(std::int64_t lhs,
                                              std::int64_t rhs) {
  constexpr auto kMin = std::numeric_limits<std::int64_t>::min();
  constexpr auto kMax = std::numeric_limits<std::int64_t>::max();
  if ((rhs > 0 && lhs > kMax - rhs) ||
      (rhs < 0 && lhs < kMin - rhs)) {
    throw std::overflow_error("Smith normal form addition overflow");
  }
  return lhs + rhs;
}

[[nodiscard]] inline std::int64_t checked_sub(std::int64_t lhs,
                                              std::int64_t rhs) {
  constexpr auto kMin = std::numeric_limits<std::int64_t>::min();
  constexpr auto kMax = std::numeric_limits<std::int64_t>::max();
  if ((rhs > 0 && lhs < kMin + rhs) ||
      (rhs < 0 && lhs > kMax + rhs)) {
    throw std::overflow_error("Smith normal form subtraction overflow");
  }
  return lhs - rhs;
}

[[nodiscard]] inline std::int64_t checked_mul(std::int64_t lhs,
                                              std::int64_t rhs) {
  constexpr auto kMin = std::numeric_limits<std::int64_t>::min();
  constexpr auto kMax = std::numeric_limits<std::int64_t>::max();
  if (lhs == 0 || rhs == 0) return 0;
  if (lhs > 0) {
    if (rhs > 0) {
      if (lhs > kMax / rhs) {
        throw std::overflow_error("Smith normal form multiplication overflow");
      }
    } else if (rhs < kMin / lhs) {
      throw std::overflow_error("Smith normal form multiplication overflow");
    }
  } else {
    if (rhs > 0) {
      if (lhs < kMin / rhs) {
        throw std::overflow_error("Smith normal form multiplication overflow");
      }
    } else if (lhs < kMax / rhs) {
      throw std::overflow_error("Smith normal form multiplication overflow");
    }
  }
  return lhs * rhs;
}

[[nodiscard]] inline std::int64_t checked_negate(std::int64_t value) {
  if (value == std::numeric_limits<std::int64_t>::min()) {
    throw std::overflow_error("Smith normal form negation overflow");
  }
  return -value;
}

[[nodiscard]] inline std::int64_t quotient(std::int64_t numerator,
                                           std::int64_t divisor) {
  if (divisor == 0) {
    throw std::logic_error("Smith normal form zero divisor");
  }
  if (numerator == std::numeric_limits<std::int64_t>::min() &&
      divisor == -1) {
    throw std::overflow_error("Smith normal form quotient overflow");
  }
  return numerator / divisor;
}

[[nodiscard]] inline bool divides(std::int64_t divisor,
                                  std::int64_t value) {
  if (divisor == 0) return value == 0;
  if (divisor == 1 || divisor == -1) return true;
  if (value == std::numeric_limits<std::int64_t>::min() && divisor == -1) {
    return true;
  }
  return value % divisor == 0;
}

[[nodiscard]] inline SmithMatrix identity(std::size_t size) {
  SmithMatrix result(size, std::vector<std::int64_t>(size, 0));
  for (std::size_t index = 0; index < size; ++index) result[index][index] = 1;
  return result;
}

inline void swap_rows(SmithMatrix& matrix, SmithMatrix& left,
                      std::size_t first, std::size_t second) {
  if (first == second) return;
  std::swap(matrix[first], matrix[second]);
  std::swap(left[first], left[second]);
}

inline void swap_columns(SmithMatrix& matrix, SmithMatrix& right,
                         std::size_t first, std::size_t second) {
  if (first == second) return;
  for (auto& row : matrix) std::swap(row[first], row[second]);
  for (auto& row : right) std::swap(row[first], row[second]);
}

inline void row_subtract_multiple(SmithMatrix& matrix, SmithMatrix& left,
                                  std::size_t target, std::size_t source,
                                  std::int64_t factor) {
  if (factor == 0) return;
  for (std::size_t column = 0; column < matrix[target].size(); ++column) {
    matrix[target][column] = checked_sub(
        matrix[target][column], checked_mul(factor, matrix[source][column]));
  }
  for (std::size_t column = 0; column < left[target].size(); ++column) {
    left[target][column] = checked_sub(
        left[target][column], checked_mul(factor, left[source][column]));
  }
}

inline void column_subtract_multiple(SmithMatrix& matrix,
                                     SmithMatrix& right,
                                     std::size_t target, std::size_t source,
                                     std::int64_t factor) {
  if (factor == 0) return;
  for (auto& row : matrix) {
    row[target] = checked_sub(row[target], checked_mul(factor, row[source]));
  }
  for (auto& row : right) {
    row[target] = checked_sub(row[target], checked_mul(factor, row[source]));
  }
}

inline void row_add(SmithMatrix& matrix, SmithMatrix& left,
                    std::size_t target, std::size_t source) {
  for (std::size_t column = 0; column < matrix[target].size(); ++column) {
    matrix[target][column] =
        checked_add(matrix[target][column], matrix[source][column]);
  }
  for (std::size_t column = 0; column < left[target].size(); ++column) {
    left[target][column] =
        checked_add(left[target][column], left[source][column]);
  }
}

inline void negate_row(SmithMatrix& matrix, SmithMatrix& left,
                       std::size_t row) {
  for (auto& value : matrix[row]) value = checked_negate(value);
  for (auto& value : left[row]) value = checked_negate(value);
}

}  // namespace smith_detail

// Bounded-exact Smith normal form over signed 64-bit integers.
//
// Returns unimodular U/V and canonical D with U * A * V = D. Non-zero
// diagonal entries are positive and each divides its successor. The direct
// Euclidean row/column implementation throws std::overflow_error if any
// intermediate entry of D/U/V leaves int64_t; it does not claim arbitrary-
// precision completion for every matrix whose final invariant factors fit.
[[nodiscard]] inline SmithNormalFormResult smith_normal_form(
    const SmithMatrix& input) {
  const std::size_t rows = input.size();
  const std::size_t columns = rows == 0 ? 0 : input.front().size();
  for (const auto& row : input) {
    if (row.size() != columns) {
      throw std::invalid_argument("Smith normal form matrix must be rectangular");
    }
  }

  SmithNormalFormResult result;
  result.diagonal = input;
  result.left_transform = smith_detail::identity(rows);
  result.right_transform = smith_detail::identity(columns);

  const std::size_t diagonal_limit = std::min(rows, columns);
  std::size_t pivot_index = 0;
  while (pivot_index < diagonal_limit) {
    std::size_t pivot_row = rows;
    std::size_t pivot_column = columns;
    std::uint64_t best_magnitude = 0;
    for (std::size_t row = pivot_index; row < rows; ++row) {
      for (std::size_t column = pivot_index; column < columns; ++column) {
        const auto value = result.diagonal[row][column];
        if (value == 0) continue;
        const auto candidate = smith_detail::magnitude(value);
        if (pivot_row == rows || candidate < best_magnitude) {
          pivot_row = row;
          pivot_column = column;
          best_magnitude = candidate;
        }
      }
    }
    if (pivot_row == rows) break;

    smith_detail::swap_rows(result.diagonal, result.left_transform,
                            pivot_index, pivot_row);
    smith_detail::swap_columns(result.diagonal, result.right_transform,
                               pivot_index, pivot_column);

    for (;;) {
      for (std::size_t row = pivot_index + 1; row < rows; ++row) {
        while (result.diagonal[row][pivot_index] != 0) {
          if (result.diagonal[pivot_index][pivot_index] == 0) {
            smith_detail::swap_rows(result.diagonal, result.left_transform,
                                    pivot_index, row);
            continue;
          }
          const auto factor = smith_detail::quotient(
              result.diagonal[row][pivot_index],
              result.diagonal[pivot_index][pivot_index]);
          smith_detail::row_subtract_multiple(
              result.diagonal, result.left_transform, row, pivot_index, factor);
          if (result.diagonal[row][pivot_index] != 0 &&
              smith_detail::magnitude(result.diagonal[row][pivot_index]) <
                  smith_detail::magnitude(
                      result.diagonal[pivot_index][pivot_index])) {
            smith_detail::swap_rows(result.diagonal, result.left_transform,
                                    pivot_index, row);
          }
        }
      }

      for (std::size_t column = pivot_index + 1; column < columns; ++column) {
        while (result.diagonal[pivot_index][column] != 0) {
          if (result.diagonal[pivot_index][pivot_index] == 0) {
            smith_detail::swap_columns(result.diagonal, result.right_transform,
                                       pivot_index, column);
            continue;
          }
          const auto factor = smith_detail::quotient(
              result.diagonal[pivot_index][column],
              result.diagonal[pivot_index][pivot_index]);
          smith_detail::column_subtract_multiple(
              result.diagonal, result.right_transform, column, pivot_index,
              factor);
          if (result.diagonal[pivot_index][column] != 0 &&
              smith_detail::magnitude(result.diagonal[pivot_index][column]) <
                  smith_detail::magnitude(
                      result.diagonal[pivot_index][pivot_index])) {
            smith_detail::swap_columns(result.diagonal, result.right_transform,
                                       pivot_index, column);
          }
        }
      }

      bool axis_is_clear = true;
      for (std::size_t row = pivot_index + 1; row < rows; ++row) {
        if (result.diagonal[row][pivot_index] != 0) {
          axis_is_clear = false;
          break;
        }
      }
      if (axis_is_clear) {
        for (std::size_t column = pivot_index + 1; column < columns; ++column) {
          if (result.diagonal[pivot_index][column] != 0) {
            axis_is_clear = false;
            break;
          }
        }
      }
      if (!axis_is_clear) continue;

      const auto pivot = result.diagonal[pivot_index][pivot_index];
      std::size_t violating_row = rows;
      for (std::size_t row = pivot_index + 1;
           row < rows && violating_row == rows; ++row) {
        for (std::size_t column = pivot_index + 1; column < columns; ++column) {
          if (!smith_detail::divides(pivot, result.diagonal[row][column])) {
            violating_row = row;
            break;
          }
        }
      }
      if (violating_row == rows) break;

      smith_detail::row_add(result.diagonal, result.left_transform, pivot_index,
                            violating_row);
    }

    if (result.diagonal[pivot_index][pivot_index] < 0) {
      smith_detail::negate_row(result.diagonal, result.left_transform,
                               pivot_index);
    }
    result.invariant_factors.push_back(
        result.diagonal[pivot_index][pivot_index]);
    ++pivot_index;
  }

  result.rank = result.invariant_factors.size();
  return result;
}

}  // namespace algorithms::linear_algebra
