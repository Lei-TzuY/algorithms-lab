#pragma once

#include "algorithms/numerical/householder_qr.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <vector>

namespace algorithms::numerical {

struct LeastSquaresResult {
  std::vector<double> solution;
  std::vector<double> residual;
  double residual_norm{};
  double normal_residual_inf{};
  std::size_t numerical_rank{};
};

namespace least_squares_detail {

inline void require_finite(double value, const char* message) {
  if (!std::isfinite(value)) {
    throw std::overflow_error(message);
  }
}

inline double checked_multiply(double left, double right, const char* message) {
  const double product = left * right;
  require_finite(product, message);
  return product;
}

inline double checked_add(double left, double right, const char* message) {
  const double sum = left + right;
  require_finite(sum, message);
  return sum;
}

inline double dot_q_column_rhs(const std::vector<std::vector<double>>& q,
                               std::size_t column,
                               const std::vector<double>& rhs) {
  double result = 0.0;
  for (std::size_t row = 0U; row < rhs.size(); ++row) {
    const double product = checked_multiply(
        q[row][column], rhs[row], "least-squares Q^T b overflowed");
    result = checked_add(result, product, "least-squares Q^T b overflowed");
  }
  return result;
}

}  // namespace least_squares_detail

// Solves min_x ||A x - b||_2 for a finite dense m x n matrix with m >= n and
// numerically full column rank. Rank-deficient systems and underdetermined
// systems are rejected rather than silently switching to a pseudoinverse or a
// minimum-norm contract.
inline LeastSquaresResult least_squares_qr(
    const std::vector<std::vector<double>>& matrix,
    const std::vector<double>& rhs,
    double relative_rank_tolerance = 1.0e-12) {
  if (!std::isfinite(relative_rank_tolerance) ||
      !(relative_rank_tolerance > 0.0)) {
    throw std::invalid_argument(
        "least-squares rank tolerance must be finite and positive");
  }

  const std::size_t rows = matrix.size();
  const std::size_t columns = qr_detail::validate_matrix(matrix);
  if (rhs.size() != rows) {
    throw std::invalid_argument("least-squares rhs size mismatch");
  }
  for (const double value : rhs) {
    if (!std::isfinite(value)) {
      throw std::invalid_argument("least-squares rhs must be finite");
    }
  }
  if (rows < columns) {
    throw std::invalid_argument(
        "least-squares QR requires rows >= columns");
  }

  LeastSquaresResult result;
  result.solution.assign(columns, 0.0);
  result.residual = rhs;
  result.numerical_rank = columns;

  if (columns == 0U) {
    double norm = 0.0;
    for (const double value : rhs) {
      norm = std::hypot(norm, value);
      least_squares_detail::require_finite(
          norm, "least-squares residual norm overflowed");
    }
    result.residual_norm = norm;
    result.normal_residual_inf = 0.0;
    return result;
  }

  const HouseholderQrResult qr = householder_qr(matrix);
  double diagonal_scale = 0.0;
  for (std::size_t index = 0U; index < columns; ++index) {
    diagonal_scale = std::max(diagonal_scale, std::abs(qr.r[index][index]));
  }
  least_squares_detail::require_finite(
      diagonal_scale, "least-squares rank scale overflowed");
  if (diagonal_scale == 0.0) {
    throw std::invalid_argument("least-squares matrix is rank deficient");
  }
  const double threshold = relative_rank_tolerance * diagonal_scale;
  least_squares_detail::require_finite(
      threshold, "least-squares rank threshold overflowed");
  for (std::size_t index = 0U; index < columns; ++index) {
    if (std::abs(qr.r[index][index]) <= threshold) {
      throw std::invalid_argument("least-squares matrix is rank deficient");
    }
  }

  std::vector<double> transformed(columns, 0.0);
  for (std::size_t column = 0U; column < columns; ++column) {
    transformed[column] =
        least_squares_detail::dot_q_column_rhs(qr.q, column, rhs);
  }

  for (std::size_t reverse = columns; reverse > 0U; --reverse) {
    const std::size_t row = reverse - 1U;
    double numerator = transformed[row];
    for (std::size_t column = row + 1U; column < columns; ++column) {
      const double product = least_squares_detail::checked_multiply(
          qr.r[row][column], result.solution[column],
          "least-squares back substitution overflowed");
      numerator = least_squares_detail::checked_add(
          numerator, -product, "least-squares back substitution overflowed");
    }
    result.solution[row] = numerator / qr.r[row][row];
    least_squares_detail::require_finite(
        result.solution[row], "least-squares back substitution overflowed");
  }

  result.residual.assign(rows, 0.0);
  double residual_norm = 0.0;
  for (std::size_t row = 0U; row < rows; ++row) {
    double prediction = 0.0;
    for (std::size_t column = 0U; column < columns; ++column) {
      const double product = least_squares_detail::checked_multiply(
          matrix[row][column], result.solution[column],
          "least-squares residual evaluation overflowed");
      prediction = least_squares_detail::checked_add(
          prediction, product, "least-squares residual evaluation overflowed");
    }
    result.residual[row] = least_squares_detail::checked_add(
        rhs[row], -prediction, "least-squares residual evaluation overflowed");
    residual_norm = std::hypot(residual_norm, result.residual[row]);
    least_squares_detail::require_finite(
        residual_norm, "least-squares residual norm overflowed");
  }
  result.residual_norm = residual_norm;

  double normal_inf = 0.0;
  for (std::size_t column = 0U; column < columns; ++column) {
    double normal_component = 0.0;
    for (std::size_t row = 0U; row < rows; ++row) {
      const double product = least_squares_detail::checked_multiply(
          matrix[row][column], result.residual[row],
          "least-squares normal residual overflowed");
      normal_component = least_squares_detail::checked_add(
          normal_component, product,
          "least-squares normal residual overflowed");
    }
    normal_inf = std::max(normal_inf, std::abs(normal_component));
  }
  result.normal_residual_inf = normal_inf;
  return result;
}

}  // namespace algorithms::numerical
