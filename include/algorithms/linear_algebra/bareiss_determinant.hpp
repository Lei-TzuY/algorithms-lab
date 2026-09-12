#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::linear_algebra {

namespace detail {

[[nodiscard]] inline std::int64_t bareiss_checked_mul(std::int64_t lhs,
                                                       std::int64_t rhs) {
  constexpr auto kMin = std::numeric_limits<std::int64_t>::min();
  constexpr auto kMax = std::numeric_limits<std::int64_t>::max();
  if (lhs == 0 || rhs == 0) return 0;

  if (lhs > 0) {
    if (rhs > 0) {
      if (lhs > kMax / rhs) {
        throw std::overflow_error("Bareiss multiplication overflow");
      }
    } else if (rhs < kMin / lhs) {
      throw std::overflow_error("Bareiss multiplication overflow");
    }
  } else {
    if (rhs > 0) {
      if (lhs < kMin / rhs) {
        throw std::overflow_error("Bareiss multiplication overflow");
      }
    } else if (lhs < kMax / rhs) {
      throw std::overflow_error("Bareiss multiplication overflow");
    }
  }
  return lhs * rhs;
}

[[nodiscard]] inline std::int64_t bareiss_checked_sub(std::int64_t lhs,
                                                       std::int64_t rhs) {
  constexpr auto kMin = std::numeric_limits<std::int64_t>::min();
  constexpr auto kMax = std::numeric_limits<std::int64_t>::max();
  if ((rhs > 0 && lhs < kMin + rhs) ||
      (rhs < 0 && lhs > kMax + rhs)) {
    throw std::overflow_error("Bareiss subtraction overflow");
  }
  return lhs - rhs;
}

[[nodiscard]] inline std::int64_t bareiss_exact_div(std::int64_t numerator,
                                                     std::int64_t divisor) {
  constexpr auto kMin = std::numeric_limits<std::int64_t>::min();
  if (divisor == 0) {
    throw std::logic_error("Bareiss zero exact divisor");
  }
  if (numerator == kMin && divisor == -1) {
    throw std::overflow_error("Bareiss division overflow");
  }
  if (numerator % divisor != 0) {
    throw std::logic_error("Bareiss division was not exact");
  }
  return numerator / divisor;
}

[[nodiscard]] inline std::int64_t bareiss_checked_negate(std::int64_t value) {
  if (value == std::numeric_limits<std::int64_t>::min()) {
    throw std::overflow_error("Bareiss determinant sign overflow");
  }
  return -value;
}

}  // namespace detail

// Fraction-free Bareiss determinant for a square signed-64-bit integer matrix.
// The result is exact when every exact multiplication/subtraction intermediate
// is representable by int64_t. If that bounded arithmetic contract is exceeded,
// the function throws std::overflow_error rather than silently wrapping.
[[nodiscard]] inline std::int64_t bareiss_determinant(
    const std::vector<std::vector<std::int64_t>>& matrix) {
  const std::size_t n = matrix.size();
  for (const auto& row : matrix) {
    if (row.size() != n) {
      throw std::invalid_argument("Bareiss determinant matrix must be square");
    }
  }
  if (n == 0) return 1;
  if (n == 1) return matrix[0][0];

  auto work = matrix;
  std::int64_t previous_pivot = 1;
  bool negate_result = false;

  for (std::size_t column = 0; column + 1 < n; ++column) {
    std::size_t pivot_row = column;
    while (pivot_row < n && work[pivot_row][column] == 0) {
      ++pivot_row;
    }
    if (pivot_row == n) return 0;
    if (pivot_row != column) {
      std::swap(work[pivot_row], work[column]);
      negate_result = !negate_result;
    }

    const std::int64_t pivot = work[column][column];
    for (std::size_t row = column + 1; row < n; ++row) {
      for (std::size_t next_column = column + 1; next_column < n;
           ++next_column) {
        const std::int64_t first = detail::bareiss_checked_mul(
            work[row][next_column], pivot);
        const std::int64_t second = detail::bareiss_checked_mul(
            work[row][column], work[column][next_column]);
        const std::int64_t numerator =
            detail::bareiss_checked_sub(first, second);
        work[row][next_column] =
            column == 0
                ? numerator
                : detail::bareiss_exact_div(numerator, previous_pivot);
      }
      work[row][column] = 0;
    }
    previous_pivot = pivot;
  }

  const std::int64_t determinant = work[n - 1][n - 1];
  return negate_result ? detail::bareiss_checked_negate(determinant)
                       : determinant;
}

}  // namespace algorithms::linear_algebra
