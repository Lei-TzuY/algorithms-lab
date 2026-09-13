#pragma once

#include "algorithms/numerical/householder_qr.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <random>
#include <vector>

namespace {

using Matrix = std::vector<std::vector<double>>;

Matrix multiply_matrix(const Matrix& left, const Matrix& right) {
  if (left.empty()) {
    return {};
  }
  const std::size_t inner = right.size();
  const std::size_t columns = inner == 0U ? 0U : right.front().size();
  Matrix product(left.size(), std::vector<double>(columns, 0.0));
  for (std::size_t row = 0U; row < left.size(); ++row) {
    for (std::size_t k = 0U; k < inner; ++k) {
      for (std::size_t column = 0U; column < columns; ++column) {
        product[row][column] += left[row][k] * right[k][column];
      }
    }
  }
  return product;
}

Matrix transpose_matrix(const Matrix& matrix) {
  if (matrix.empty()) {
    return {};
  }
  Matrix result(matrix.front().size(), std::vector<double>(matrix.size(), 0.0));
  for (std::size_t row = 0U; row < matrix.size(); ++row) {
    for (std::size_t column = 0U; column < matrix[row].size(); ++column) {
      result[column][row] = matrix[row][column];
    }
  }
  return result;
}

void require_matrix_close(const Matrix& actual, const Matrix& expected,
                          double tolerance) {
  REQUIRE_EQ(actual.size(), expected.size());
  for (std::size_t row = 0U; row < actual.size(); ++row) {
    REQUIRE_EQ(actual[row].size(), expected[row].size());
    for (std::size_t column = 0U; column < actual[row].size(); ++column) {
      const double scale =
          std::max({1.0, std::abs(actual[row][column]),
                    std::abs(expected[row][column])});
      REQUIRE(std::abs(actual[row][column] - expected[row][column]) <=
              tolerance * scale);
    }
  }
}

Matrix identity_matrix(std::size_t size) {
  Matrix result(size, std::vector<double>(size, 0.0));
  for (std::size_t index = 0U; index < size; ++index) {
    result[index][index] = 1.0;
  }
  return result;
}

Matrix modified_gram_schmidt_basis(const Matrix& matrix) {
  const std::size_t rows = matrix.size();
  const std::size_t columns = rows == 0U ? 0U : matrix.front().size();
  Matrix q(rows, std::vector<double>(columns, 0.0));
  for (std::size_t column = 0U; column < columns; ++column) {
    std::vector<double> vector(rows, 0.0);
    for (std::size_t row = 0U; row < rows; ++row) {
      vector[row] = matrix[row][column];
    }
    for (std::size_t previous = 0U; previous < column; ++previous) {
      double projection = 0.0;
      for (std::size_t row = 0U; row < rows; ++row) {
        projection += q[row][previous] * vector[row];
      }
      for (std::size_t row = 0U; row < rows; ++row) {
        vector[row] -= projection * q[row][previous];
      }
    }
    double norm = 0.0;
    for (const double value : vector) {
      norm = std::hypot(norm, value);
    }
    if (norm <= 1.0e-12) {
      return {};
    }
    for (std::size_t row = 0U; row < rows; ++row) {
      q[row][column] = vector[row] / norm;
    }
  }
  return q;
}

Matrix projector_from_columns(const Matrix& q, std::size_t columns) {
  const std::size_t rows = q.size();
  Matrix result(rows, std::vector<double>(rows, 0.0));
  for (std::size_t row = 0U; row < rows; ++row) {
    for (std::size_t other = 0U; other < rows; ++other) {
      double sum = 0.0;
      for (std::size_t column = 0U; column < columns; ++column) {
        sum += q[row][column] * q[other][column];
      }
      result[row][other] = sum;
    }
  }
  return result;
}

void verify_qr(const Matrix& input,
               const algorithms::numerical::HouseholderQrResult& result,
               double tolerance) {
  REQUIRE_EQ(result.q.size(), input.size());
  REQUIRE_EQ(result.r.size(), input.size());
  const std::size_t columns = input.empty() ? 0U : input.front().size();
  for (const auto& row : result.q) {
    REQUIRE_EQ(row.size(), input.size());
  }
  for (const auto& row : result.r) {
    REQUIRE_EQ(row.size(), columns);
  }
  require_matrix_close(multiply_matrix(result.q, result.r), input, tolerance);
  require_matrix_close(multiply_matrix(transpose_matrix(result.q), result.q),
                       identity_matrix(input.size()), tolerance);
  const std::size_t diagonal = std::min(input.size(), columns);
  for (std::size_t column = 0U; column < diagonal; ++column) {
    for (std::size_t row = column + 1U; row < input.size(); ++row) {
      REQUIRE_EQ(result.r[row][column], 0.0);
    }
  }
}

TEST_CASE(householder_qr_validation_and_shape_contracts) {
  const Matrix empty;
  const auto empty_result = algorithms::numerical::householder_qr(empty);
  REQUIRE(empty_result.q.empty());
  REQUIRE(empty_result.r.empty());
  REQUIRE_EQ(empty_result.reflectors_applied, 0U);

  const Matrix zero_columns(3U);
  const auto zero_result = algorithms::numerical::householder_qr(zero_columns);
  require_matrix_close(zero_result.q, identity_matrix(3U), 0.0);
  REQUIRE_EQ(zero_result.r.size(), 3U);
  REQUIRE_EQ(zero_result.reflectors_applied, 0U);

  const Matrix ragged{{1.0, 2.0}, {3.0}};
  REQUIRE_THROWS_AS(algorithms::numerical::householder_qr(ragged),
                    std::invalid_argument);
  const Matrix nonfinite{{1.0, std::numeric_limits<double>::infinity()}};
  REQUIRE_THROWS_AS(algorithms::numerical::householder_qr(nonfinite),
                    std::invalid_argument);
}

TEST_CASE(householder_qr_known_rectangular_and_rank_deficient_cases) {
  const Matrix classic{{12.0, -51.0, 4.0},
                       {6.0, 167.0, -68.0},
                       {-4.0, 24.0, -41.0}};
  const auto classic_result = algorithms::numerical::householder_qr(classic);
  verify_qr(classic, classic_result, 2.0e-12);
  REQUIRE(std::abs(std::abs(classic_result.r[0][0]) - 14.0) <= 1.0e-12);
  REQUIRE(std::abs(std::abs(classic_result.r[1][1]) - 175.0) <= 1.0e-10);
  REQUIRE(std::abs(std::abs(classic_result.r[2][2]) - 35.0) <= 1.0e-10);

  const Matrix tall{{1.0, 2.0}, {3.0, 4.0}, {5.0, 7.0}, {-2.0, 1.0}};
  verify_qr(tall, algorithms::numerical::householder_qr(tall), 5.0e-12);

  const Matrix wide{{1.0, 2.0, 3.0, 4.0}, {2.0, -1.0, 0.5, 7.0}};
  verify_qr(wide, algorithms::numerical::householder_qr(wide), 5.0e-12);

  const Matrix deficient{{1.0, 2.0, 3.0}, {2.0, 4.0, 6.0}, {0.0, 0.0, 0.0}};
  verify_qr(deficient, algorithms::numerical::householder_qr(deficient),
            1.0e-10);
}

TEST_CASE(householder_qr_randomized_reconstruction_orthogonality_and_mgs_space) {
  std::mt19937_64 random(0x485152ULL);
  std::uniform_int_distribution<std::size_t> rows_distribution(1U, 10U);
  std::uniform_real_distribution<double> value_distribution(-4.0, 4.0);

  std::size_t checked_full_rank = 0U;
  for (std::size_t trial = 0U; trial < 700U; ++trial) {
    const std::size_t rows = rows_distribution(random);
    std::uniform_int_distribution<std::size_t> columns_distribution(1U, rows);
    const std::size_t columns = columns_distribution(random);
    Matrix input(rows, std::vector<double>(columns, 0.0));
    for (auto& row : input) {
      for (double& value : row) {
        value = value_distribution(random);
      }
    }

    const auto result = algorithms::numerical::householder_qr(input);
    verify_qr(input, result, 2.0e-10);

    const Matrix mgs = modified_gram_schmidt_basis(input);
    if (!mgs.empty()) {
      const Matrix production_projector =
          projector_from_columns(result.q, columns);
      const Matrix oracle_projector = projector_from_columns(mgs, columns);
      require_matrix_close(production_projector, oracle_projector, 2.0e-9);
      ++checked_full_rank;
    }
  }
  REQUIRE(checked_full_rank >= 650U);
}

TEST_CASE(householder_qr_nonfinite_intermediate_fails_closed) {
  const double huge = std::numeric_limits<double>::max();
  const Matrix matrix{{huge, huge}, {huge, -huge}};
  REQUIRE_THROWS_AS(algorithms::numerical::householder_qr(matrix),
                    std::overflow_error);
}

}  // namespace
