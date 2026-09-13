#pragma once

#include "algorithms/numerical/singular_value_decomposition.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace svd_test_detail {

using Matrix = std::vector<std::vector<double>>;
using algorithms::numerical::SingularValueDecompositionResult;

inline double frobenius_norm(const Matrix& matrix) {
  double result = 0.0;
  for (const auto& row : matrix) {
    for (const double value : row) {
      result = std::hypot(result, value);
    }
  }
  return result;
}

inline Matrix reconstruct(const SingularValueDecompositionResult& svd) {
  const std::size_t rows = svd.u.size();
  const std::size_t columns = svd.v.size();
  const std::size_t rank_bound = svd.singular_values.size();
  Matrix result(rows, std::vector<double>(columns, 0.0));
  for (std::size_t row = 0U; row < rows; ++row) {
    for (std::size_t column = 0U; column < columns; ++column) {
      long double sum = 0.0L;
      for (std::size_t index = 0U; index < rank_bound; ++index) {
        sum += static_cast<long double>(svd.u[row][index]) *
               static_cast<long double>(svd.singular_values[index]) *
               static_cast<long double>(svd.v[column][index]);
      }
      result[row][column] = static_cast<double>(sum);
    }
  }
  return result;
}

inline double reconstruction_error(const Matrix& actual, const Matrix& expected) {
  double result = 0.0;
  for (std::size_t row = 0U; row < actual.size(); ++row) {
    for (std::size_t column = 0U; column < actual[row].size(); ++column) {
      result = std::hypot(result, actual[row][column] - expected[row][column]);
    }
  }
  return result;
}

inline double column_dot(const Matrix& matrix, std::size_t first,
                         std::size_t second) {
  long double sum = 0.0L;
  for (const auto& row : matrix) {
    sum += static_cast<long double>(row[first]) *
           static_cast<long double>(row[second]);
  }
  return static_cast<double>(sum);
}

inline void require_factorization_invariants(const Matrix& matrix,
                                             double relative_tolerance = 2.0e-8) {
  const auto svd = algorithms::numerical::one_sided_jacobi_svd(
      matrix, 1.0e-12, 200U);
  const std::size_t rows = matrix.size();
  const std::size_t columns = rows == 0U ? 0U : matrix.front().size();
  const std::size_t rank_bound = std::min(rows, columns);

  REQUIRE_EQ(svd.singular_values.size(), rank_bound);
  REQUIRE_EQ(svd.u.size(), rows);
  REQUIRE_EQ(svd.v.size(), columns);
  for (const auto& row : svd.u) {
    REQUIRE_EQ(row.size(), rank_bound);
  }
  for (const auto& row : svd.v) {
    REQUIRE_EQ(row.size(), rank_bound);
  }

  for (std::size_t index = 0U; index < rank_bound; ++index) {
    REQUIRE(std::isfinite(svd.singular_values[index]));
    REQUIRE(svd.singular_values[index] >= 0.0);
    if (index > 0U) {
      REQUIRE(svd.singular_values[index - 1U] + 1.0e-15 >=
              svd.singular_values[index]);
    }
  }

  for (std::size_t first = 0U; first < rank_bound; ++first) {
    for (std::size_t second = 0U; second < rank_bound; ++second) {
      const double expected = first == second ? 1.0 : 0.0;
      REQUIRE(std::fabs(column_dot(svd.u, first, second) - expected) < 1.0e-8);
      REQUIRE(std::fabs(column_dot(svd.v, first, second) - expected) < 1.0e-8);
    }
  }

  const double input_norm = frobenius_norm(matrix);
  const double scale = std::max(1.0, input_norm);
  const Matrix reconstructed = reconstruct(svd);
  REQUIRE(reconstruction_error(matrix, reconstructed) <=
          relative_tolerance * scale);

  long double singular_energy = 0.0L;
  for (const double singular_value : svd.singular_values) {
    singular_energy += static_cast<long double>(singular_value) *
                       static_cast<long double>(singular_value);
  }
  const long double input_energy =
      static_cast<long double>(input_norm) * static_cast<long double>(input_norm);
  REQUIRE(std::fabs(static_cast<double>(singular_energy - input_energy)) <=
          5.0e-8 * std::max(1.0, input_norm * input_norm));

  for (std::size_t index = 0U; index < rank_bound; ++index) {
    if (svd.singular_values[index] <= 1.0e-10 * scale) {
      continue;
    }
    for (std::size_t row = 0U; row < rows; ++row) {
      long double lhs = 0.0L;
      for (std::size_t column = 0U; column < columns; ++column) {
        lhs += static_cast<long double>(matrix[row][column]) *
               static_cast<long double>(svd.v[column][index]);
      }
      const long double rhs =
          static_cast<long double>(svd.singular_values[index]) *
          static_cast<long double>(svd.u[row][index]);
      REQUIRE(std::fabs(static_cast<double>(lhs - rhs)) <= 1.0e-7 * scale);
    }
    for (std::size_t column = 0U; column < columns; ++column) {
      long double lhs = 0.0L;
      for (std::size_t row = 0U; row < rows; ++row) {
        lhs += static_cast<long double>(matrix[row][column]) *
               static_cast<long double>(svd.u[row][index]);
      }
      const long double rhs =
          static_cast<long double>(svd.singular_values[index]) *
          static_cast<long double>(svd.v[column][index]);
      REQUIRE(std::fabs(static_cast<double>(lhs - rhs)) <= 1.0e-7 * scale);
    }
  }
}

inline void require_2x2_closed_form(const Matrix& matrix) {
  const auto svd = algorithms::numerical::one_sided_jacobi_svd(
      matrix, 1.0e-12, 200U);
  const double a = matrix[0][0];
  const double b = matrix[0][1];
  const double c = matrix[1][0];
  const double d = matrix[1][1];
  const double trace = a * a + b * b + c * c + d * d;
  const double determinant = a * d - b * c;
  const double discriminant =
      std::max(0.0, trace * trace - 4.0 * determinant * determinant);
  const double root = std::sqrt(discriminant);
  const double expected_largest =
      std::sqrt(std::max(0.0, 0.5 * (trace + root)));
  const double expected_smallest =
      std::sqrt(std::max(0.0, 0.5 * (trace - root)));
  const double scale = std::max(1.0, expected_largest);
  REQUIRE(std::fabs(svd.singular_values[0] - expected_largest) <=
          2.0e-9 * scale);
  REQUIRE(std::fabs(svd.singular_values[1] - expected_smallest) <=
          2.0e-9 * scale);
}

}  // namespace svd_test_detail
