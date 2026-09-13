#pragma once

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::numerical {

struct HouseholderQrResult {
  std::vector<std::vector<double>> q;
  std::vector<std::vector<double>> r;
  std::size_t reflectors_applied = 0U;
};

namespace qr_detail {

inline void require_finite_qr(double value, const char* message) {
  if (!std::isfinite(value)) {
    throw std::overflow_error(message);
  }
}

inline std::size_t validate_matrix(
    const std::vector<std::vector<double>>& matrix) {
  if (matrix.empty()) {
    return 0U;
  }
  const std::size_t columns = matrix.front().size();
  for (const auto& row : matrix) {
    if (row.size() != columns) {
      throw std::invalid_argument("Householder QR matrix must be rectangular");
    }
    for (const double value : row) {
      if (!std::isfinite(value)) {
        throw std::invalid_argument("Householder QR matrix must be finite");
      }
    }
  }
  return columns;
}

inline double vector_norm(const std::vector<double>& values) {
  double norm = 0.0;
  for (const double value : values) {
    norm = std::hypot(norm, value);
    require_finite_qr(norm, "Householder QR norm overflowed");
  }
  return norm;
}

inline double checked_dot_qr(const std::vector<double>& left,
                             const std::vector<double>& right) {
  double result = 0.0;
  for (std::size_t index = 0U; index < left.size(); ++index) {
    const double product = left[index] * right[index];
    require_finite_qr(product, "Householder QR dot product overflowed");
    result += product;
    require_finite_qr(result, "Householder QR dot product overflowed");
  }
  return result;
}

}  // namespace qr_detail

// Dense first-principles Householder QR. The result uses a full m x m Q and
// m x n upper-trapezoidal R such that A ~= Q*R in floating-point arithmetic.
inline HouseholderQrResult householder_qr(
    const std::vector<std::vector<double>>& matrix) {
  const std::size_t rows = matrix.size();
  const std::size_t columns = qr_detail::validate_matrix(matrix);

  HouseholderQrResult result;
  result.r = matrix;
  result.q.assign(rows, std::vector<double>(rows, 0.0));
  for (std::size_t row = 0U; row < rows; ++row) {
    result.q[row][row] = 1.0;
  }
  if (rows == 0U || columns == 0U) {
    return result;
  }

  const std::size_t steps = rows < columns ? rows : columns;
  for (std::size_t column = 0U; column < steps; ++column) {
    std::vector<double> reflector(rows - column, 0.0);
    for (std::size_t row = column; row < rows; ++row) {
      reflector[row - column] = result.r[row][column];
    }

    const double norm = qr_detail::vector_norm(reflector);
    if (norm == 0.0) {
      continue;
    }
    const double alpha = -std::copysign(norm, reflector.front());
    qr_detail::require_finite_qr(alpha, "Householder QR reflector overflowed");
    reflector.front() -= alpha;
    qr_detail::require_finite_qr(reflector.front(),
                                 "Householder QR reflector overflowed");
    const double reflector_norm = qr_detail::vector_norm(reflector);
    if (reflector_norm == 0.0) {
      continue;
    }
    for (double& value : reflector) {
      value /= reflector_norm;
      qr_detail::require_finite_qr(value,
                                   "Householder QR reflector overflowed");
    }

    for (std::size_t target_column = column; target_column < columns;
         ++target_column) {
      std::vector<double> column_slice(reflector.size(), 0.0);
      for (std::size_t offset = 0U; offset < reflector.size(); ++offset) {
        column_slice[offset] = result.r[column + offset][target_column];
      }
      const double projection = qr_detail::checked_dot_qr(reflector, column_slice);
      const double factor = 2.0 * projection;
      qr_detail::require_finite_qr(factor, "Householder QR update overflowed");
      for (std::size_t offset = 0U; offset < reflector.size(); ++offset) {
        const double update = factor * reflector[offset];
        qr_detail::require_finite_qr(update, "Householder QR update overflowed");
        result.r[column + offset][target_column] -= update;
        qr_detail::require_finite_qr(result.r[column + offset][target_column],
                                     "Householder QR update overflowed");
      }
    }

    for (std::size_t q_row = 0U; q_row < rows; ++q_row) {
      std::vector<double> q_slice(reflector.size(), 0.0);
      for (std::size_t offset = 0U; offset < reflector.size(); ++offset) {
        q_slice[offset] = result.q[q_row][column + offset];
      }
      const double projection = qr_detail::checked_dot_qr(q_slice, reflector);
      const double factor = 2.0 * projection;
      qr_detail::require_finite_qr(factor, "Householder QR Q update overflowed");
      for (std::size_t offset = 0U; offset < reflector.size(); ++offset) {
        const double update = factor * reflector[offset];
        qr_detail::require_finite_qr(update, "Householder QR Q update overflowed");
        result.q[q_row][column + offset] -= update;
        qr_detail::require_finite_qr(result.q[q_row][column + offset],
                                     "Householder QR Q update overflowed");
      }
    }

    // The entries are mathematically zero; clearing the roundoff tail makes the
    // upper-trapezoidal contract explicit without changing the represented
    // factorization beyond ordinary floating-point error.
    result.r[column][column] = alpha;
    for (std::size_t row = column + 1U; row < rows; ++row) {
      result.r[row][column] = 0.0;
    }
    ++result.reflectors_applied;
  }

  return result;
}

}  // namespace algorithms::numerical
