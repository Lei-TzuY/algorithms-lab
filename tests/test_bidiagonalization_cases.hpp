#pragma once

#include "algorithms/numerical/bidiagonalization.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <random>
#include <vector>

namespace {

using BidiagMatrix = std::vector<std::vector<double>>;

BidiagMatrix bidiag_transpose(const BidiagMatrix& matrix) {
  if (matrix.empty()) {
    return {};
  }
  BidiagMatrix result(matrix.front().size(),
                       std::vector<double>(matrix.size(), 0.0));
  for (std::size_t row = 0U; row < matrix.size(); ++row) {
    for (std::size_t column = 0U; column < matrix[row].size(); ++column) {
      result[column][row] = matrix[row][column];
    }
  }
  return result;
}

BidiagMatrix bidiag_multiply(const BidiagMatrix& left,
                             const BidiagMatrix& right) {
  if (left.empty()) {
    return {};
  }
  const std::size_t inner = left.front().size();
  REQUIRE_EQ(right.size(), inner);
  const std::size_t columns = right.empty() ? 0U : right.front().size();
  BidiagMatrix result(left.size(), std::vector<double>(columns, 0.0));
  for (std::size_t row = 0U; row < left.size(); ++row) {
    for (std::size_t k = 0U; k < inner; ++k) {
      for (std::size_t column = 0U; column < columns; ++column) {
        result[row][column] += left[row][k] * right[k][column];
      }
    }
  }
  return result;
}

BidiagMatrix bidiag_identity(std::size_t size) {
  BidiagMatrix result(size, std::vector<double>(size, 0.0));
  for (std::size_t index = 0U; index < size; ++index) {
    result[index][index] = 1.0;
  }
  return result;
}

void require_bidiag_matrix_close(const BidiagMatrix& actual,
                                  const BidiagMatrix& expected,
                                  double tolerance) {
  REQUIRE_EQ(actual.size(), expected.size());
  for (std::size_t row = 0U; row < actual.size(); ++row) {
    REQUIRE_EQ(actual[row].size(), expected[row].size());
    for (std::size_t column = 0U; column < actual[row].size(); ++column) {
      const double scale = std::max(
          {1.0, std::abs(actual[row][column]), std::abs(expected[row][column])});
      REQUIRE(std::abs(actual[row][column] - expected[row][column]) <=
              tolerance * scale);
    }
  }
}

void verify_bidiagonalization(
    const BidiagMatrix& input,
    const algorithms::numerical::BidiagonalizationResult& result,
    double tolerance) {
  const std::size_t rows = input.size();
  const std::size_t columns = rows == 0U ? 0U : input.front().size();
  REQUIRE_EQ(result.u.size(), rows);
  REQUIRE_EQ(result.b.size(), rows);
  REQUIRE_EQ(result.v.size(), columns);
  for (const auto& row : result.u) {
    REQUIRE_EQ(row.size(), rows);
  }
  for (const auto& row : result.b) {
    REQUIRE_EQ(row.size(), columns);
  }
  for (const auto& row : result.v) {
    REQUIRE_EQ(row.size(), columns);
  }

  require_bidiag_matrix_close(
      bidiag_multiply(bidiag_multiply(result.u, result.b),
                      bidiag_transpose(result.v)),
      input, tolerance);
  require_bidiag_matrix_close(
      bidiag_multiply(bidiag_transpose(result.u), result.u),
      bidiag_identity(rows), tolerance * 5.0);
  require_bidiag_matrix_close(
      bidiag_multiply(bidiag_transpose(result.v), result.v),
      bidiag_identity(columns), tolerance * 5.0);

  for (std::size_t row = 0U; row < rows; ++row) {
    for (std::size_t column = 0U; column < columns; ++column) {
      if (column != row && column != row + 1U) {
        REQUIRE_EQ(result.b[row][column], 0.0);
      }
    }
  }
}

TEST_CASE(bidiagonalization_validation_empty_and_zero_column_contracts) {
  const BidiagMatrix empty;
  const auto empty_result = algorithms::numerical::householder_bidiagonalize(empty);
  REQUIRE(empty_result.u.empty());
  REQUIRE(empty_result.b.empty());
  REQUIRE(empty_result.v.empty());
  REQUIRE_EQ(empty_result.left_reflectors_applied, 0U);
  REQUIRE_EQ(empty_result.right_reflectors_applied, 0U);

  const BidiagMatrix zero_columns(3U);
  const auto zero_result =
      algorithms::numerical::householder_bidiagonalize(zero_columns);
  require_bidiag_matrix_close(zero_result.u, bidiag_identity(3U), 0.0);
  REQUIRE_EQ(zero_result.b.size(), 3U);
  REQUIRE(zero_result.v.empty());

  REQUIRE_THROWS_AS(
      algorithms::numerical::householder_bidiagonalize(
          BidiagMatrix{{1.0, 2.0}, {3.0}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::numerical::householder_bidiagonalize(BidiagMatrix{{
          1.0, std::numeric_limits<double>::infinity()}}),
      std::invalid_argument);
}

TEST_CASE(bidiagonalization_tall_wide_and_rank_deficient_reconstruction) {
  const BidiagMatrix square{{4.0, 1.0, -2.0},
                             {2.0, 3.0, 5.0},
                             {-1.0, 7.0, 6.0}};
  verify_bidiagonalization(
      square, algorithms::numerical::householder_bidiagonalize(square),
      2.0e-11);

  const BidiagMatrix tall{{1.0, 2.0}, {3.0, 4.0}, {5.0, 7.0}, {-2.0, 1.0}};
  verify_bidiagonalization(
      tall, algorithms::numerical::householder_bidiagonalize(tall), 2.0e-11);

  const BidiagMatrix wide{{1.0, 2.0, 3.0, 4.0},
                           {2.0, -1.0, 0.5, 7.0}};
  verify_bidiagonalization(
      wide, algorithms::numerical::householder_bidiagonalize(wide), 2.0e-11);

  const BidiagMatrix deficient{{1.0, 2.0, 3.0},
                                {2.0, 4.0, 6.0},
                                {0.0, 0.0, 0.0},
                                {3.0, 6.0, 9.0}};
  verify_bidiagonalization(
      deficient,
      algorithms::numerical::householder_bidiagonalize(deficient), 1.0e-9);
}

TEST_CASE(bidiagonalization_randomized_factorization_and_orthogonality) {
  std::mt19937_64 random(0xB1D1A60A1ULL);
  std::uniform_int_distribution<std::size_t> dimension_distribution(1U, 10U);
  std::uniform_real_distribution<double> value_distribution(-8.0, 8.0);
  for (std::size_t trial = 0U; trial < 700U; ++trial) {
    const std::size_t rows = dimension_distribution(random);
    const std::size_t columns = dimension_distribution(random);
    BidiagMatrix input(rows, std::vector<double>(columns, 0.0));
    for (auto& row : input) {
      for (double& value : row) {
        value = value_distribution(random);
      }
    }
    verify_bidiagonalization(
        input, algorithms::numerical::householder_bidiagonalize(input),
        3.0e-10);
  }
}

TEST_CASE(bidiagonalization_nonfinite_intermediate_fails_closed) {
  const double huge = std::numeric_limits<double>::max();
  REQUIRE_THROWS_AS(
      algorithms::numerical::householder_bidiagonalize(
          BidiagMatrix{{huge, huge}, {huge, -huge}}),
      std::overflow_error);
}

}  // namespace
