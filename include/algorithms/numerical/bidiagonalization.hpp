#pragma once

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace algorithms::numerical {

struct BidiagonalizationResult {
  std::vector<std::vector<double>> u;
  std::vector<std::vector<double>> b;
  std::vector<std::vector<double>> v;
  std::size_t left_reflectors_applied = 0U;
  std::size_t right_reflectors_applied = 0U;
};

namespace bidiagonal_detail {

inline void require_finite(double value, const char* message) {
  if (!std::isfinite(value)) {
    throw std::overflow_error(message);
  }
}

inline std::size_t validate_matrix(const std::vector<std::vector<double>>& matrix) {
  if (matrix.empty()) {
    return 0U;
  }
  const std::size_t columns = matrix.front().size();
  for (const auto& row : matrix) {
    if (row.size() != columns) {
      throw std::invalid_argument("bidiagonalization matrix must be rectangular");
    }
    for (const double value : row) {
      if (!std::isfinite(value)) {
        throw std::invalid_argument("bidiagonalization matrix must be finite");
      }
    }
  }
  return columns;
}

inline double norm(const std::vector<double>& values) {
  double result = 0.0;
  for (const double value : values) {
    result = std::hypot(result, value);
    require_finite(result, "bidiagonalization norm overflowed");
  }
  return result;
}

inline double dot(const std::vector<double>& left,
                  const std::vector<double>& right) {
  double result = 0.0;
  for (std::size_t index = 0U; index < left.size(); ++index) {
    const double product = left[index] * right[index];
    require_finite(product, "bidiagonalization dot product overflowed");
    result += product;
    require_finite(result, "bidiagonalization dot product overflowed");
  }
  return result;
}

inline bool make_reflector(std::vector<double>& reflector) {
  const double original_norm = norm(reflector);
  if (original_norm == 0.0) {
    return false;
  }
  const double alpha = -std::copysign(original_norm, reflector.front());
  require_finite(alpha, "bidiagonalization reflector overflowed");
  reflector.front() -= alpha;
  require_finite(reflector.front(), "bidiagonalization reflector overflowed");
  const double reflector_norm = norm(reflector);
  if (reflector_norm == 0.0) {
    return false;
  }
  for (double& value : reflector) {
    value /= reflector_norm;
    require_finite(value, "bidiagonalization reflector overflowed");
  }
  return true;
}

inline std::vector<std::vector<double>> identity(std::size_t size) {
  std::vector<std::vector<double>> matrix(
      size, std::vector<double>(size, 0.0));
  for (std::size_t index = 0U; index < size; ++index) {
    matrix[index][index] = 1.0;
  }
  return matrix;
}

inline void right_multiply_embedded_reflector(
    std::vector<std::vector<double>>& matrix, std::size_t offset,
    const std::vector<double>& reflector, const char* overflow_message) {
  for (auto& row : matrix) {
    std::vector<double> slice(reflector.size(), 0.0);
    for (std::size_t index = 0U; index < reflector.size(); ++index) {
      slice[index] = row[offset + index];
    }
    const double factor = 2.0 * dot(slice, reflector);
    require_finite(factor, overflow_message);
    for (std::size_t index = 0U; index < reflector.size(); ++index) {
      const double update = factor * reflector[index];
      require_finite(update, overflow_message);
      row[offset + index] -= update;
      require_finite(row[offset + index], overflow_message);
    }
  }
}

inline void left_multiply_embedded_reflector(
    std::vector<std::vector<double>>& matrix, std::size_t offset,
    const std::vector<double>& reflector, std::size_t first_column,
    const char* overflow_message) {
  if (matrix.empty()) {
    return;
  }
  const std::size_t columns = matrix.front().size();
  for (std::size_t column = first_column; column < columns; ++column) {
    std::vector<double> slice(reflector.size(), 0.0);
    for (std::size_t index = 0U; index < reflector.size(); ++index) {
      slice[index] = matrix[offset + index][column];
    }
    const double factor = 2.0 * dot(reflector, slice);
    require_finite(factor, overflow_message);
    for (std::size_t index = 0U; index < reflector.size(); ++index) {
      const double update = factor * reflector[index];
      require_finite(update, overflow_message);
      matrix[offset + index][column] -= update;
      require_finite(matrix[offset + index][column], overflow_message);
    }
  }
}

}  // namespace bidiagonal_detail

inline BidiagonalizationResult householder_bidiagonalize(
    const std::vector<std::vector<double>>& matrix) {
  const std::size_t rows = matrix.size();
  const std::size_t columns = bidiagonal_detail::validate_matrix(matrix);

  BidiagonalizationResult result;
  result.b = matrix;
  result.u = bidiagonal_detail::identity(rows);
  result.v = bidiagonal_detail::identity(columns);

  const std::size_t steps = rows < columns ? rows : columns;
  for (std::size_t step = 0U; step < steps; ++step) {
    std::vector<double> left(rows - step, 0.0);
    for (std::size_t row = step; row < rows; ++row) {
      left[row - step] = result.b[row][step];
    }
    if (bidiagonal_detail::make_reflector(left)) {
      bidiagonal_detail::left_multiply_embedded_reflector(
          result.b, step, left, step, "bidiagonalization left update overflowed");
      bidiagonal_detail::right_multiply_embedded_reflector(
          result.u, step, left, "bidiagonalization U update overflowed");
      for (std::size_t row = step + 1U; row < rows; ++row) {
        result.b[row][step] = 0.0;
      }
      ++result.left_reflectors_applied;
    }

    if (step + 1U >= columns) {
      continue;
    }
    std::vector<double> right(columns - (step + 1U), 0.0);
    for (std::size_t column = step + 1U; column < columns; ++column) {
      right[column - (step + 1U)] = result.b[step][column];
    }
    if (bidiagonal_detail::make_reflector(right)) {
      bidiagonal_detail::right_multiply_embedded_reflector(
          result.b, step + 1U, right, "bidiagonalization right update overflowed");
      bidiagonal_detail::right_multiply_embedded_reflector(
          result.v, step + 1U, right, "bidiagonalization V update overflowed");
      for (std::size_t column = step + 2U; column < columns; ++column) {
        result.b[step][column] = 0.0;
      }
      ++result.right_reflectors_applied;
    }
  }
  return result;
}

}  // namespace algorithms::numerical
