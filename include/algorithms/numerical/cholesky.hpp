#pragma once

#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::numerical {

struct CholeskyFactorization {
  std::vector<std::vector<double>> lower;
};

namespace cholesky_detail {

inline void validate_square_symmetric_finite(
    const std::vector<std::vector<double>>& matrix) {
  const std::size_t n = matrix.size();
  for (std::size_t row = 0; row < n; ++row) {
    if (matrix[row].size() != n) {
      throw std::invalid_argument("cholesky matrix must be square");
    }
    for (std::size_t column = 0; column < n; ++column) {
      if (!std::isfinite(matrix[row][column])) {
        throw std::invalid_argument("cholesky matrix must be finite");
      }
      if (column < row && matrix[row][column] != matrix[column][row]) {
        throw std::invalid_argument("cholesky matrix must be exactly symmetric");
      }
    }
  }
}

inline double narrow_finite(long double value, const char* message) {
  if (!std::isfinite(value) ||
      value > static_cast<long double>(std::numeric_limits<double>::max()) ||
      value < -static_cast<long double>(std::numeric_limits<double>::max())) {
    throw std::overflow_error(message);
  }
  const double narrowed = static_cast<double>(value);
  if (!std::isfinite(narrowed)) {
    throw std::overflow_error(message);
  }
  return narrowed;
}

inline void validate_factor(const CholeskyFactorization& factor) {
  const std::size_t n = factor.lower.size();
  for (std::size_t row = 0; row < n; ++row) {
    if (factor.lower[row].size() != n) {
      throw std::invalid_argument("cholesky factor must be square");
    }
    for (std::size_t column = 0; column < n; ++column) {
      const double value = factor.lower[row][column];
      if (!std::isfinite(value)) {
        throw std::invalid_argument("cholesky factor must be finite");
      }
      if (column > row && value != 0.0) {
        throw std::invalid_argument("cholesky factor must be lower triangular");
      }
    }
    if (factor.lower[row][row] <= 0.0) {
      throw std::invalid_argument("cholesky factor diagonal must be positive");
    }
  }
}

}  // namespace cholesky_detail

// Compute the direct dense Cholesky factorization A = L L^T.
//
// Input must be a finite exactly-symmetric dense square matrix. The executed
// floating-point recurrence must see a strictly positive diagonal residual at
// every step; otherwise std::domain_error is thrown. This is the operational
// finite-precision contract, not a claim that IEEE-754 execution is an exact
// classifier for mathematical positive definiteness on arbitrarily ill-
// conditioned data.
//
// Dot products are accumulated in long double and narrowed only after each
// finalized factor entry. Non-finite/unrepresentable required intermediates
// fail closed with std::overflow_error.
inline CholeskyFactorization cholesky_factorize(
    const std::vector<std::vector<double>>& matrix) {
  cholesky_detail::validate_square_symmetric_finite(matrix);
  const std::size_t n = matrix.size();
  CholeskyFactorization result{std::vector<std::vector<double>>(
      n, std::vector<double>(n, 0.0))};

  for (std::size_t row = 0; row < n; ++row) {
    for (std::size_t column = 0; column <= row; ++column) {
      long double dot = 0.0L;
      for (std::size_t k = 0; k < column; ++k) {
        const long double product =
            static_cast<long double>(result.lower[row][k]) *
            static_cast<long double>(result.lower[column][k]);
        if (!std::isfinite(product)) {
          throw std::overflow_error("cholesky dot product overflowed");
        }
        dot += product;
        if (!std::isfinite(dot)) {
          throw std::overflow_error("cholesky dot product overflowed");
        }
      }

      const long double residual =
          static_cast<long double>(matrix[row][column]) - dot;
      if (!std::isfinite(residual)) {
        throw std::overflow_error("cholesky residual overflowed");
      }

      if (row == column) {
        if (residual <= 0.0L) {
          throw std::domain_error(
              "cholesky matrix is not positive definite under executed arithmetic");
        }
        const long double diagonal = std::sqrt(residual);
        const double narrowed = cholesky_detail::narrow_finite(
            diagonal, "cholesky diagonal overflowed");
        if (narrowed <= 0.0) {
          throw std::overflow_error("cholesky diagonal underflowed");
        }
        result.lower[row][column] = narrowed;
      } else {
        const long double quotient =
            residual /
            static_cast<long double>(result.lower[column][column]);
        result.lower[row][column] = cholesky_detail::narrow_finite(
            quotient, "cholesky factor entry overflowed");
      }
    }
  }
  return result;
}

// Solve (L L^T) x = rhs using the supplied positive-diagonal lower factor.
// The factor is replay-validated rather than assumed trustworthy.
inline std::vector<double> cholesky_solve(
    const CholeskyFactorization& factor,
    const std::vector<double>& rhs) {
  cholesky_detail::validate_factor(factor);
  const std::size_t n = factor.lower.size();
  if (rhs.size() != n) {
    throw std::invalid_argument("cholesky rhs size mismatch");
  }
  for (const double value : rhs) {
    if (!std::isfinite(value)) {
      throw std::invalid_argument("cholesky rhs must be finite");
    }
  }

  std::vector<double> y(n, 0.0);
  for (std::size_t row = 0; row < n; ++row) {
    long double residual = static_cast<long double>(rhs[row]);
    for (std::size_t column = 0; column < row; ++column) {
      residual -= static_cast<long double>(factor.lower[row][column]) *
                  static_cast<long double>(y[column]);
      if (!std::isfinite(residual)) {
        throw std::overflow_error("cholesky forward solve overflowed");
      }
    }
    y[row] = cholesky_detail::narrow_finite(
        residual / static_cast<long double>(factor.lower[row][row]),
        "cholesky forward solve overflowed");
  }

  std::vector<double> x(n, 0.0);
  for (std::size_t offset = 0; offset < n; ++offset) {
    const std::size_t row = n - 1U - offset;
    long double residual = static_cast<long double>(y[row]);
    for (std::size_t column = row + 1U; column < n; ++column) {
      residual -= static_cast<long double>(factor.lower[column][row]) *
                  static_cast<long double>(x[column]);
      if (!std::isfinite(residual)) {
        throw std::overflow_error("cholesky backward solve overflowed");
      }
    }
    x[row] = cholesky_detail::narrow_finite(
        residual / static_cast<long double>(factor.lower[row][row]),
        "cholesky backward solve overflowed");
  }
  return x;
}

}  // namespace algorithms::numerical
