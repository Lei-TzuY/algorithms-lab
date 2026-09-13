#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numeric>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::numerical::svd_detail {

using Matrix = std::vector<std::vector<double>>;

inline void require_finite(double value, const char* message) {
  if (!std::isfinite(value)) throw std::overflow_error(message);
}

inline std::size_t validate_matrix(const Matrix& matrix) {
  if (matrix.empty()) return 0U;
  const std::size_t columns = matrix.front().size();
  for (const auto& row : matrix) {
    if (row.size() != columns)
      throw std::invalid_argument("SVD matrix must be rectangular");
    for (double value : row)
      if (!std::isfinite(value))
        throw std::invalid_argument("SVD matrix must be finite");
  }
  return columns;
}

inline Matrix identity(std::size_t size) {
  Matrix result(size, std::vector<double>(size, 0.0));
  for (std::size_t i = 0U; i < size; ++i) result[i][i] = 1.0;
  return result;
}

inline Matrix transpose(const Matrix& matrix, std::size_t columns) {
  Matrix result(columns, std::vector<double>(matrix.size(), 0.0));
  for (std::size_t i = 0U; i < matrix.size(); ++i)
    for (std::size_t j = 0U; j < columns; ++j) result[j][i] = matrix[i][j];
  return result;
}

inline double column_dot(const Matrix& matrix, std::size_t p, std::size_t q) {
  double sum = 0.0;
  for (const auto& row : matrix) {
    const double term = row[p] * row[q];
    require_finite(term, "SVD dot product overflowed");
    sum += term;
    require_finite(sum, "SVD dot product overflowed");
  }
  return sum;
}

inline double column_norm(const Matrix& matrix, std::size_t column) {
  double result = 0.0;
  for (const auto& row : matrix) {
    result = std::hypot(result, row[column]);
    require_finite(result, "SVD column norm overflowed");
  }
  return result;
}

inline void rotate_columns(Matrix& matrix, std::size_t p, std::size_t q,
                           double c, double s) {
  for (auto& row : matrix) {
    const double x = row[p], y = row[q];
    const double nx = c * x - s * y, ny = s * x + c * y;
    require_finite(nx, "SVD Jacobi rotation overflowed");
    require_finite(ny, "SVD Jacobi rotation overflowed");
    row[p] = nx;
    row[q] = ny;
  }
}

inline double vector_norm(const std::vector<double>& vector) {
  double result = 0.0;
  for (double value : vector) {
    result = std::hypot(result, value);
    require_finite(result, "SVD orthogonal completion overflowed");
  }
  return result;
}

inline void complete_column(Matrix& matrix, std::size_t column) {
  const std::size_t rows = matrix.size();
  std::vector<double> best(rows, 0.0);
  double best_norm = -1.0;
  for (std::size_t basis = 0U; basis < rows; ++basis) {
    std::vector<double> candidate(rows, 0.0);
    candidate[basis] = 1.0;
    for (std::size_t previous = 0U; previous < column; ++previous) {
      double projection = 0.0;
      for (std::size_t row = 0U; row < rows; ++row)
        projection += matrix[row][previous] * candidate[row];
      require_finite(projection, "SVD orthogonal completion overflowed");
      for (std::size_t row = 0U; row < rows; ++row) {
        candidate[row] -= projection * matrix[row][previous];
        require_finite(candidate[row], "SVD orthogonal completion overflowed");
      }
    }
    const double norm = vector_norm(candidate);
    if (norm > best_norm) {
      best_norm = norm;
      best = std::move(candidate);
    }
  }
  if (!(best_norm > 0.0))
    throw std::runtime_error("SVD could not complete an orthonormal basis");
  for (std::size_t row = 0U; row < rows; ++row) {
    matrix[row][column] = best[row] / best_norm;
    require_finite(matrix[row][column], "SVD orthogonal completion overflowed");
  }
}

struct TallResult {
  Matrix u;
  std::vector<double> values;
  Matrix v;
  std::size_t sweeps;
};

inline TallResult decompose_tall(const Matrix& input, double tolerance,
                                 std::size_t max_sweeps) {
  const std::size_t rows = input.size();
  const std::size_t columns = input.empty() ? 0U : input.front().size();
  Matrix work = input, right = identity(columns);
  bool converged = columns < 2U;
  std::size_t sweeps = 0U;
  for (; sweeps < max_sweeps && !converged; ++sweeps) {
    converged = true;
    for (std::size_t p = 0U; p < columns; ++p) {
      for (std::size_t q = p + 1U; q < columns; ++q) {
        const double alpha = column_dot(work, p, p);
        const double beta = column_dot(work, q, q);
        const double gamma = column_dot(work, p, q);
        if (gamma == 0.0 || alpha == 0.0 || beta == 0.0) continue;
        const double scale = std::sqrt(alpha) * std::sqrt(beta);
        require_finite(scale, "SVD convergence scale overflowed");
        if (std::abs(gamma) <= tolerance * scale) continue;
        converged = false;
        const double zeta = (beta - alpha) / (2.0 * gamma);
        require_finite(zeta, "SVD Jacobi parameter overflowed");
        const double sign = zeta >= 0.0 ? 1.0 : -1.0;
        const double t = sign / (std::abs(zeta) + std::hypot(1.0, zeta));
        const double c = 1.0 / std::hypot(1.0, t), s = c * t;
        require_finite(c, "SVD Jacobi parameter overflowed");
        require_finite(s, "SVD Jacobi parameter overflowed");
        rotate_columns(work, p, q, c, s);
        rotate_columns(right, p, q, c, s);
      }
    }
  }
  if (!converged)
    throw std::runtime_error("SVD Jacobi iteration did not converge");

  std::vector<double> values(columns, 0.0);
  for (std::size_t j = 0U; j < columns; ++j) values[j] = column_norm(work, j);
  std::vector<std::size_t> order(columns);
  std::iota(order.begin(), order.end(), 0U);
  std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
    return values[a] > values[b];
  });

  Matrix sorted_work(rows, std::vector<double>(columns, 0.0));
  Matrix sorted_right(columns, std::vector<double>(columns, 0.0));
  std::vector<double> sorted(columns, 0.0);
  for (std::size_t j = 0U; j < columns; ++j) {
    const std::size_t old = order[j];
    sorted[j] = values[old];
    for (std::size_t i = 0U; i < rows; ++i) sorted_work[i][j] = work[i][old];
    for (std::size_t i = 0U; i < columns; ++i) sorted_right[i][j] = right[i][old];
  }

  Matrix left(rows, std::vector<double>(columns, 0.0));
  const double largest = sorted.empty() ? 0.0 : sorted.front();
  const double zero_threshold =
      tolerance * static_cast<double>(std::max(rows, columns)) * largest;
  require_finite(zero_threshold, "SVD numerical-rank threshold overflowed");
  for (std::size_t j = 0U; j < columns; ++j) {
    if (sorted[j] > zero_threshold && sorted[j] > 0.0) {
      for (std::size_t i = 0U; i < rows; ++i) {
        left[i][j] = sorted_work[i][j] / sorted[j];
        require_finite(left[i][j], "SVD normalization overflowed");
      }
    } else {
      sorted[j] = 0.0;
      complete_column(left, j);
    }
  }
  return {std::move(left), std::move(sorted), std::move(sorted_right), sweeps};
}

}  // namespace algorithms::numerical::svd_detail
