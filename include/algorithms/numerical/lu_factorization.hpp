#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <numeric>
#include <stdexcept>
#include <vector>

namespace algorithms::numerical {

struct LuFactorization {
  std::vector<std::size_t> permutation;
  std::vector<std::vector<double>> lower;
  std::vector<std::vector<double>> upper;
};

namespace lu_detail {

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

inline void validate_square_finite(
    const std::vector<std::vector<double>>& matrix) {
  const std::size_t n = matrix.size();
  for (const auto& row : matrix) {
    if (row.size() != n) {
      throw std::invalid_argument("LU matrix must be square");
    }
    for (const double value : row) {
      if (!std::isfinite(value)) {
        throw std::invalid_argument("LU matrix must be finite");
      }
    }
  }
}

inline void validate_factor(const LuFactorization& factor) {
  const std::size_t n = factor.lower.size();
  if (factor.upper.size() != n || factor.permutation.size() != n) {
    throw std::invalid_argument("LU factor dimensions do not match");
  }

  std::vector<bool> seen(n, false);
  for (const std::size_t value : factor.permutation) {
    if (value >= n || seen[value]) {
      throw std::invalid_argument("LU permutation is invalid");
    }
    seen[value] = true;
  }

  for (std::size_t row = 0; row < n; ++row) {
    if (factor.lower[row].size() != n || factor.upper[row].size() != n) {
      throw std::invalid_argument("LU factors must be square");
    }
    for (std::size_t column = 0; column < n; ++column) {
      const double lower = factor.lower[row][column];
      const double upper = factor.upper[row][column];
      if (!std::isfinite(lower) || !std::isfinite(upper)) {
        throw std::invalid_argument("LU factors must be finite");
      }
      if (column > row && lower != 0.0) {
        throw std::invalid_argument("LU lower factor must be lower triangular");
      }
      if (column < row && upper != 0.0) {
        throw std::invalid_argument("LU upper factor must be upper triangular");
      }
    }
    if (factor.lower[row][row] != 1.0) {
      throw std::invalid_argument("LU lower factor must have unit diagonal");
    }
    if (factor.upper[row][row] == 0.0) {
      throw std::invalid_argument("LU upper factor diagonal must be nonzero");
    }
  }
}

}  // namespace lu_detail

// Dense PLU factorization with deterministic partial pivoting.
//
// The result satisfies P*A = L*U up to executed floating-point arithmetic,
// where permutation[row] is the source row of A selected for row `row` of
// P*A. L is unit-lower-triangular and U is upper-triangular.
//
// At each column the pivot is the remaining row with maximum absolute pivot
// magnitude; ties keep the smallest row index. An exactly-zero executed pivot
// is reported as std::domain_error. This is an operational floating-point
// singularity contract, not a numerical-rank or condition-number estimate.
// Required non-finite/unrepresentable arithmetic fails closed with
// std::overflow_error.
inline LuFactorization lu_factorize(
    const std::vector<std::vector<double>>& matrix) {
  lu_detail::validate_square_finite(matrix);
  const std::size_t n = matrix.size();

  LuFactorization result;
  result.permutation.resize(n);
  std::iota(result.permutation.begin(), result.permutation.end(),
            std::size_t{0});
  result.lower.assign(n, std::vector<double>(n, 0.0));
  result.upper = matrix;
  for (std::size_t index = 0; index < n; ++index) {
    result.lower[index][index] = 1.0;
  }

  for (std::size_t column = 0; column < n; ++column) {
    std::size_t pivot_row = column;
    double pivot_magnitude = std::fabs(result.upper[column][column]);
    for (std::size_t row = column + 1U; row < n; ++row) {
      const double magnitude = std::fabs(result.upper[row][column]);
      if (magnitude > pivot_magnitude) {
        pivot_magnitude = magnitude;
        pivot_row = row;
      }
    }
    if (pivot_magnitude == 0.0) {
      throw std::domain_error(
          "LU matrix is singular under executed arithmetic");
    }

    if (pivot_row != column) {
      std::swap(result.upper[pivot_row], result.upper[column]);
      std::swap(result.permutation[pivot_row], result.permutation[column]);
      for (std::size_t previous = 0; previous < column; ++previous) {
        std::swap(result.lower[pivot_row][previous],
                  result.lower[column][previous]);
      }
    }

    for (std::size_t row = column + 1U; row < n; ++row) {
      const long double quotient =
          static_cast<long double>(result.upper[row][column]) /
          static_cast<long double>(result.upper[column][column]);
      const double multiplier = lu_detail::narrow_finite(
          quotient, "LU multiplier overflowed");
      result.lower[row][column] = multiplier;
      result.upper[row][column] = 0.0;

      for (std::size_t next = column + 1U; next < n; ++next) {
        const long double update =
            static_cast<long double>(result.upper[row][next]) -
            static_cast<long double>(multiplier) *
                static_cast<long double>(result.upper[column][next]);
        result.upper[row][next] =
            lu_detail::narrow_finite(update, "LU elimination overflowed");
      }
    }
  }

  return result;
}

// Solve A*x=rhs using a replay-validated PLU factorization of A.
inline std::vector<double> lu_solve(const LuFactorization& factor,
                                    const std::vector<double>& rhs) {
  lu_detail::validate_factor(factor);
  const std::size_t n = factor.lower.size();
  if (rhs.size() != n) {
    throw std::invalid_argument("LU rhs size mismatch");
  }
  for (const double value : rhs) {
    if (!std::isfinite(value)) {
      throw std::invalid_argument("LU rhs must be finite");
    }
  }

  std::vector<double> y(n, 0.0);
  for (std::size_t row = 0; row < n; ++row) {
    long double residual =
        static_cast<long double>(rhs[factor.permutation[row]]);
    for (std::size_t column = 0; column < row; ++column) {
      residual -= static_cast<long double>(factor.lower[row][column]) *
                  static_cast<long double>(y[column]);
      if (!std::isfinite(residual)) {
        throw std::overflow_error("LU forward solve overflowed");
      }
    }
    y[row] = lu_detail::narrow_finite(residual, "LU forward solve overflowed");
  }

  std::vector<double> x(n, 0.0);
  for (std::size_t offset = 0; offset < n; ++offset) {
    const std::size_t row = n - 1U - offset;
    long double residual = static_cast<long double>(y[row]);
    for (std::size_t column = row + 1U; column < n; ++column) {
      residual -= static_cast<long double>(factor.upper[row][column]) *
                  static_cast<long double>(x[column]);
      if (!std::isfinite(residual)) {
        throw std::overflow_error("LU backward solve overflowed");
      }
    }
    const long double quotient =
        residual / static_cast<long double>(factor.upper[row][row]);
    x[row] = lu_detail::narrow_finite(quotient, "LU backward solve overflowed");
  }
  return x;
}

}  // namespace algorithms::numerical
