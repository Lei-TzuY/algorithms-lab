#pragma once

#include "algorithms/numerical/lu_factorization.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

namespace lu_test_detail {

using Matrix = std::vector<std::vector<double>>;

inline bool close(double actual, double expected, double scale = 1.0) {
  const double magnitude = std::max({1.0, std::fabs(actual), std::fabs(expected), scale});
  return std::fabs(actual - expected) <= 2.0e-10 * magnitude;
}

inline Matrix multiply(const Matrix& left, const Matrix& right) {
  const std::size_t rows = left.size();
  const std::size_t inner = rows == 0U ? 0U : left.front().size();
  const std::size_t columns = right.empty() ? 0U : right.front().size();
  Matrix result(rows, std::vector<double>(columns, 0.0));
  for (std::size_t row = 0; row < rows; ++row) {
    for (std::size_t column = 0; column < columns; ++column) {
      long double sum = 0.0L;
      for (std::size_t k = 0; k < inner; ++k) {
        sum += static_cast<long double>(left[row][k]) *
               static_cast<long double>(right[k][column]);
      }
      result[row][column] = static_cast<double>(sum);
    }
  }
  return result;
}

inline Matrix permute_rows(const Matrix& matrix,
                           const std::vector<std::size_t>& permutation) {
  Matrix result(matrix.size());
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    result[row] = matrix[permutation[row]];
  }
  return result;
}

inline std::vector<double> multiply_vector(const Matrix& matrix,
                                           const std::vector<double>& vector) {
  std::vector<double> result(matrix.size(), 0.0);
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    long double sum = 0.0L;
    for (std::size_t column = 0; column < vector.size(); ++column) {
      sum += static_cast<long double>(matrix[row][column]) *
             static_cast<long double>(vector[column]);
    }
    result[row] = static_cast<double>(sum);
  }
  return result;
}

inline void require_matrix_close(const Matrix& actual, const Matrix& expected) {
  REQUIRE_EQ(actual.size(), expected.size());
  for (std::size_t row = 0; row < actual.size(); ++row) {
    REQUIRE_EQ(actual[row].size(), expected[row].size());
    for (std::size_t column = 0; column < actual[row].size(); ++column) {
      REQUIRE(close(actual[row][column], expected[row][column]));
    }
  }
}

inline Matrix build_matrix_from_known_factors(
    const Matrix& lower, const Matrix& upper,
    const std::vector<std::size_t>& permutation) {
  const Matrix permuted = multiply(lower, upper);
  Matrix matrix(permuted.size());
  for (std::size_t row = 0; row < permutation.size(); ++row) {
    matrix[permutation[row]] = permuted[row];
  }
  return matrix;
}

}  // namespace lu_test_detail

TEST_CASE(lu_factorization_known_pivoting_and_solve) {
  using algorithms::numerical::lu_factorize;
  using algorithms::numerical::lu_solve;
  using lu_test_detail::Matrix;

  const Matrix matrix{{0.0, 2.0, 1.0},
                      {1.0, 1.0, 0.0},
                      {2.0, 0.0, 1.0}};
  const auto factor = lu_factorize(matrix);
  REQUIRE_EQ(factor.permutation.front(), std::size_t{2});
  lu_test_detail::require_matrix_close(
      lu_test_detail::permute_rows(matrix, factor.permutation),
      lu_test_detail::multiply(factor.lower, factor.upper));

  const std::vector<double> expected_x{2.0, -1.0, 3.0};
  const auto rhs = lu_test_detail::multiply_vector(matrix, expected_x);
  const auto actual_x = lu_solve(factor, rhs);
  REQUIRE_EQ(actual_x.size(), expected_x.size());
  for (std::size_t index = 0; index < actual_x.size(); ++index) {
    REQUIRE(lu_test_detail::close(actual_x[index], expected_x[index]));
  }
}

TEST_CASE(lu_factorization_validation_singularity_and_overflow) {
  using algorithms::numerical::LuFactorization;
  using algorithms::numerical::lu_factorize;
  using algorithms::numerical::lu_solve;

  const auto empty = lu_factorize({});
  REQUIRE(empty.permutation.empty());
  REQUIRE(lu_solve(empty, {}).empty());

  REQUIRE_THROWS_AS(lu_factorize({{1.0, 2.0}, {3.0}}), std::invalid_argument);
  REQUIRE_THROWS_AS(
      lu_factorize({{1.0, std::numeric_limits<double>::infinity()}, {0.0, 1.0}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(lu_factorize({{1.0, 2.0}, {2.0, 4.0}}), std::domain_error);

  const double max = std::numeric_limits<double>::max();
  REQUIRE_THROWS_AS(lu_factorize({{max, max}, {max, -max}}),
                    std::overflow_error);

  auto factor = lu_factorize({{2.0, 1.0}, {1.0, 2.0}});
  REQUIRE_THROWS_AS(lu_solve(factor, {1.0}), std::invalid_argument);
  REQUIRE_THROWS_AS(
      lu_solve(factor, {1.0, std::numeric_limits<double>::quiet_NaN()}),
      std::invalid_argument);

  LuFactorization malformed = factor;
  malformed.permutation = {0U, 0U};
  REQUIRE_THROWS_AS(lu_solve(malformed, {1.0, 1.0}), std::invalid_argument);
  malformed = factor;
  malformed.lower[0][0] = 2.0;
  REQUIRE_THROWS_AS(lu_solve(malformed, {1.0, 1.0}), std::invalid_argument);
  malformed = factor;
  malformed.upper[1][1] = 0.0;
  REQUIRE_THROWS_AS(lu_solve(malformed, {1.0, 1.0}), std::invalid_argument);
}

TEST_CASE(lu_factorization_randomized_constructed_oracle) {
  using algorithms::numerical::lu_factorize;
  using algorithms::numerical::lu_solve;
  using lu_test_detail::Matrix;

  std::mt19937_64 random(0x4c55504c55464f52ULL);
  for (std::size_t trial = 0; trial < 700U; ++trial) {
    const std::size_t n = 1U + static_cast<std::size_t>(random() % 10U);
    Matrix lower(n, std::vector<double>(n, 0.0));
    Matrix upper(n, std::vector<double>(n, 0.0));
    for (std::size_t row = 0; row < n; ++row) {
      lower[row][row] = 1.0;
      for (std::size_t column = 0; column < row; ++column) {
        lower[row][column] = static_cast<double>(static_cast<int>(random() % 5U) - 2);
      }
      for (std::size_t column = row; column < n; ++column) {
        int value = static_cast<int>(random() % 7U) - 3;
        if (column == row && value == 0) {
          value = (random() & 1U) == 0U ? 2 : -2;
        }
        upper[row][column] = static_cast<double>(value);
      }
    }

    std::vector<std::size_t> construction_permutation(n);
    std::iota(construction_permutation.begin(), construction_permutation.end(),
              std::size_t{0});
    for (std::size_t index = n; index > 1U; --index) {
      const std::size_t other = static_cast<std::size_t>(random() % index);
      std::swap(construction_permutation[index - 1U],
                construction_permutation[other]);
    }

    const Matrix matrix = lu_test_detail::build_matrix_from_known_factors(
        lower, upper, construction_permutation);
    const auto factor = lu_factorize(matrix);
    lu_test_detail::require_matrix_close(
        lu_test_detail::permute_rows(matrix, factor.permutation),
        lu_test_detail::multiply(factor.lower, factor.upper));

    std::vector<double> expected_x(n, 0.0);
    for (double& value : expected_x) {
      value = static_cast<double>(static_cast<int>(random() % 11U) - 5);
    }
    const auto rhs = lu_test_detail::multiply_vector(matrix, expected_x);
    const auto actual_x = lu_solve(factor, rhs);
    REQUIRE_EQ(actual_x.size(), expected_x.size());
    for (std::size_t index = 0; index < n; ++index) {
      REQUIRE(lu_test_detail::close(actual_x[index], expected_x[index], 32.0));
    }
  }
}

TEST_CASE(lu_factorization_repeatability_and_structure) {
  const std::vector<std::vector<double>> matrix{
      {4.0, 3.0, 2.0, 1.0},
      {3.0, 7.0, 1.0, 2.0},
      {2.0, 1.0, 5.0, 3.0},
      {1.0, 2.0, 3.0, 8.0}};
  const auto first = algorithms::numerical::lu_factorize(matrix);
  const auto second = algorithms::numerical::lu_factorize(matrix);
  REQUIRE(first.permutation == second.permutation);
  REQUIRE(first.lower == second.lower);
  REQUIRE(first.upper == second.upper);
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    REQUIRE_EQ(first.lower[row][row], 1.0);
    REQUIRE(first.upper[row][row] != 0.0);
    for (std::size_t column = 0; column < matrix.size(); ++column) {
      if (column > row) {
        REQUIRE_EQ(first.lower[row][column], 0.0);
      }
      if (column < row) {
        REQUIRE_EQ(first.upper[row][column], 0.0);
      }
    }
  }
}
