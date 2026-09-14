#pragma once

#include "algorithms/numerical/least_squares.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <random>
#include <vector>

namespace {

using LeastSquaresMatrix = std::vector<std::vector<double>>;

void require_least_squares_close(double actual, double expected,
                                 double tolerance) {
  const double scale = std::max({1.0, std::abs(actual), std::abs(expected)});
  REQUIRE(std::abs(actual - expected) <= tolerance * scale);
}

TEST_CASE(least_squares_qr_validation_and_degenerate_cases) {
  const LeastSquaresMatrix empty;
  const std::vector<double> empty_rhs;
  const auto empty_result =
      algorithms::numerical::least_squares_qr(empty, empty_rhs);
  REQUIRE(empty_result.solution.empty());
  REQUIRE(empty_result.residual.empty());
  REQUIRE_EQ(empty_result.numerical_rank, 0U);

  const LeastSquaresMatrix zero_columns(3U);
  const std::vector<double> zero_column_rhs{3.0, -4.0, 12.0};
  const auto zero_column_result =
      algorithms::numerical::least_squares_qr(zero_columns, zero_column_rhs);
  REQUIRE(zero_column_result.solution.empty());
  REQUIRE_EQ(zero_column_result.residual, zero_column_rhs);
  require_least_squares_close(zero_column_result.residual_norm, 13.0, 1.0e-14);
  REQUIRE_EQ(zero_column_result.normal_residual_inf, 0.0);

  const LeastSquaresMatrix underdetermined{{1.0, 2.0, 3.0},
                                           {4.0, 5.0, 6.0}};
  REQUIRE_THROWS_AS(algorithms::numerical::least_squares_qr(
                        underdetermined, std::vector<double>{1.0, 2.0}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(algorithms::numerical::least_squares_qr(
                        LeastSquaresMatrix{{1.0}, {2.0}},
                        std::vector<double>{1.0}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(algorithms::numerical::least_squares_qr(
                        LeastSquaresMatrix{{1.0}},
                        std::vector<double>{
                            std::numeric_limits<double>::infinity()}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(algorithms::numerical::least_squares_qr(
                        LeastSquaresMatrix{{1.0}}, std::vector<double>{1.0},
                        0.0),
                    std::invalid_argument);

  const LeastSquaresMatrix rank_deficient{{1.0, 2.0},
                                          {2.0, 4.0},
                                          {3.0, 6.0}};
  REQUIRE_THROWS_AS(algorithms::numerical::least_squares_qr(
                        rank_deficient, std::vector<double>{1.0, 2.0, 3.0}),
                    std::invalid_argument);
}

TEST_CASE(least_squares_qr_known_line_fit_has_orthogonal_residual) {
  const LeastSquaresMatrix matrix{{1.0, 0.0}, {1.0, 1.0}, {1.0, 2.0}};
  const std::vector<double> rhs{1.0, 2.0, 2.0};
  const auto result = algorithms::numerical::least_squares_qr(matrix, rhs);

  REQUIRE_EQ(result.numerical_rank, 2U);
  require_least_squares_close(result.solution[0], 7.0 / 6.0, 1.0e-12);
  require_least_squares_close(result.solution[1], 0.5, 1.0e-12);
  require_least_squares_close(result.residual[0], -1.0 / 6.0, 1.0e-12);
  require_least_squares_close(result.residual[1], 1.0 / 3.0, 1.0e-12);
  require_least_squares_close(result.residual[2], -1.0 / 6.0, 1.0e-12);
  require_least_squares_close(result.residual_norm, std::sqrt(1.0 / 6.0),
                              1.0e-12);
  REQUIRE(result.normal_residual_inf <= 1.0e-12);
}

TEST_CASE(least_squares_qr_random_known_projection_matches_independent_oracle) {
  std::mt19937_64 random(0x4c535152ULL);
  std::uniform_int_distribution<int> dimension_distribution(1, 7);
  std::uniform_int_distribution<int> coefficient_distribution(-3, 3);
  std::uniform_int_distribution<int> solution_distribution(-4, 4);
  std::uniform_int_distribution<int> residual_distribution(-3, 3);

  for (std::size_t trial = 0U; trial < 1000U; ++trial) {
    const std::size_t columns =
        static_cast<std::size_t>(dimension_distribution(random));
    const std::size_t rows = columns + 1U;
    LeastSquaresMatrix matrix(rows, std::vector<double>(columns, 0.0));
    for (std::size_t index = 0U; index < columns; ++index) {
      matrix[index][index] = 1.0;
    }

    std::vector<double> final_row(columns, 0.0);
    std::vector<double> expected_solution(columns, 0.0);
    for (std::size_t column = 0U; column < columns; ++column) {
      final_row[column] =
          static_cast<double>(coefficient_distribution(random));
      expected_solution[column] =
          static_cast<double>(solution_distribution(random));
      matrix[columns][column] = final_row[column];
    }

    double residual_scale = static_cast<double>(residual_distribution(random));
    if (residual_scale == 0.0) {
      residual_scale = 1.0;
    }
    std::vector<double> expected_residual(rows, 0.0);
    for (std::size_t column = 0U; column < columns; ++column) {
      expected_residual[column] = -final_row[column] * residual_scale;
    }
    expected_residual[columns] = residual_scale;

    std::vector<double> rhs(rows, 0.0);
    for (std::size_t row = 0U; row < rows; ++row) {
      double value = expected_residual[row];
      for (std::size_t column = 0U; column < columns; ++column) {
        value += matrix[row][column] * expected_solution[column];
      }
      rhs[row] = value;
    }

    const auto result = algorithms::numerical::least_squares_qr(matrix, rhs);
    REQUIRE_EQ(result.solution.size(), columns);
    REQUIRE_EQ(result.numerical_rank, columns);
    for (std::size_t column = 0U; column < columns; ++column) {
      require_least_squares_close(result.solution[column],
                                  expected_solution[column], 2.0e-10);
    }
    for (std::size_t row = 0U; row < rows; ++row) {
      require_least_squares_close(result.residual[row], expected_residual[row],
                                  3.0e-10);
    }
    REQUIRE(result.normal_residual_inf <= 5.0e-9);
  }
}

TEST_CASE(least_squares_qr_nonfinite_intermediate_fails_closed) {
  const double huge = std::numeric_limits<double>::max();
  const LeastSquaresMatrix matrix{{1.0, 1.0}, {1.0, -1.0}};
  REQUIRE_THROWS_AS(algorithms::numerical::least_squares_qr(
                        matrix, std::vector<double>{huge, huge}),
                    std::overflow_error);
}

}  // namespace
