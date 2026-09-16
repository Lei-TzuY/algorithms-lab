#pragma once

#include "algorithms/numerical/gmres.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using algorithms::numerical::GmresStatus;
using algorithms::numerical::gmres_solve;

std::vector<double> gmres_test_matvec(
    const std::vector<std::vector<double>>& matrix,
    const std::vector<double>& vector) {
  std::vector<double> result(matrix.size(), 0.0);
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    for (std::size_t column = 0; column < vector.size(); ++column) {
      result[row] += matrix[row][column] * vector[column];
    }
  }
  return result;
}

double gmres_test_residual_norm(
    const std::vector<std::vector<double>>& matrix,
    const std::vector<double>& rhs,
    const std::vector<double>& solution) {
  const auto product = gmres_test_matvec(matrix, solution);
  double squared = 0.0;
  for (std::size_t index = 0; index < rhs.size(); ++index) {
    const double residual = rhs[index] - product[index];
    squared += residual * residual;
  }
  return std::sqrt(squared);
}

std::vector<double> gmres_test_gaussian_elimination(
    std::vector<std::vector<double>> matrix,
    std::vector<double> rhs) {
  const std::size_t size = matrix.size();
  for (std::size_t pivot = 0; pivot < size; ++pivot) {
    std::size_t best = pivot;
    for (std::size_t row = pivot + 1U; row < size; ++row) {
      if (std::abs(matrix[row][pivot]) > std::abs(matrix[best][pivot])) {
        best = row;
      }
    }
    if (matrix[best][pivot] == 0.0) {
      throw std::runtime_error("oracle matrix unexpectedly singular");
    }
    std::swap(matrix[pivot], matrix[best]);
    std::swap(rhs[pivot], rhs[best]);
    for (std::size_t row = pivot + 1U; row < size; ++row) {
      const double factor = matrix[row][pivot] / matrix[pivot][pivot];
      for (std::size_t column = pivot; column < size; ++column) {
        matrix[row][column] -= factor * matrix[pivot][column];
      }
      rhs[row] -= factor * rhs[pivot];
    }
  }
  std::vector<double> solution(size, 0.0);
  for (std::size_t reverse = size; reverse > 0U; --reverse) {
    const std::size_t row = reverse - 1U;
    double value = rhs[row];
    for (std::size_t column = row + 1U; column < size; ++column) {
      value -= matrix[row][column] * solution[column];
    }
    solution[row] = value / matrix[row][row];
  }
  return solution;
}

TEST_CASE(gmres_validates_contract_and_handles_warm_start) {
  REQUIRE_THROWS_AS(gmres_solve({{1.0, 2.0}}, {1.0}, 1e-12, 2U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(gmres_solve({{1.0}}, {}, 1e-12, 2U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(gmres_solve({{1.0}}, {1.0}, 0.0, 2U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(gmres_solve({{1.0}}, {1.0}, 1e-12, 0U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(
      gmres_solve({{std::numeric_limits<double>::infinity()}}, {1.0},
                  1e-12, 2U),
      std::invalid_argument);

  const auto empty = gmres_solve({}, {}, 1e-12, 0U);
  REQUIRE_EQ(empty.status, GmresStatus::converged);
  REQUIRE(empty.solution.empty());
  REQUIRE_EQ(empty.iterations, 0U);

  const auto warm = gmres_solve({{4.0, 1.0}, {2.0, 3.0}}, {6.0, 8.0},
                                1e-12, 2U, {1.0, 2.0});
  REQUIRE_EQ(warm.status, GmresStatus::converged);
  REQUIRE_EQ(warm.iterations, 0U);
  REQUIRE(warm.residual_norm <= 1e-12);
}

TEST_CASE(gmres_solves_known_nonsymmetric_system_and_honors_budget) {
  const std::vector<std::vector<double>> matrix = {
      {4.0, 1.0, 0.0}, {2.0, 5.0, 1.0}, {0.0, 3.0, 6.0}};
  const std::vector<double> expected = {1.0, -2.0, 3.0};
  const auto rhs = gmres_test_matvec(matrix, expected);
  const auto solved = gmres_solve(matrix, rhs, 1e-11, 3U);
  REQUIRE_EQ(solved.status, GmresStatus::converged);
  REQUIRE(solved.residual_norm <= 1e-11);
  for (std::size_t index = 0; index < expected.size(); ++index) {
    REQUIRE(std::abs(solved.solution[index] - expected[index]) <= 1e-9);
  }

  const auto limited = gmres_solve(matrix, rhs, 1e-14, 1U);
  REQUIRE_EQ(limited.status, GmresStatus::iteration_limit);
  REQUIRE_EQ(limited.iterations, 1U);
  REQUIRE(limited.residual_norm > 1e-14);
  REQUIRE(limited.residual_norm < gmres_test_residual_norm(
                                      matrix, rhs,
                                      std::vector<double>(rhs.size(), 0.0)));
}

TEST_CASE(gmres_reports_breakdown_and_nonfinite_arithmetic) {
  const auto happy = gmres_solve({{1.0, 0.0}, {0.0, 0.0}},
                                 {2.0, 0.0}, 1e-12, 2U);
  REQUIRE_EQ(happy.status, GmresStatus::converged);
  REQUIRE(happy.residual_norm <= 1e-12);
  REQUIRE(std::abs(happy.solution[0] - 2.0) <= 1e-12);

  const auto breakdown = gmres_solve({{0.0, 0.0}, {0.0, 0.0}},
                                     {1.0, -1.0}, 1e-12, 2U);
  REQUIRE_EQ(breakdown.status, GmresStatus::arnoldi_breakdown);
  REQUIRE(breakdown.residual_norm > 1.0);

  REQUIRE_THROWS_AS(
      gmres_solve({{std::numeric_limits<double>::max()}}, {1.0}, 1e-12,
                  1U, {2.0}),
      std::overflow_error);
}

TEST_CASE(gmres_random_nonsymmetric_systems_match_independent_elimination) {
  std::mt19937_64 rng(0x474D524553ULL);
  for (std::size_t trial = 0; trial < 500U; ++trial) {
    const std::size_t size = 1U + static_cast<std::size_t>(rng() % 8U);
    std::vector<std::vector<double>> matrix(
        size, std::vector<double>(size, 0.0));
    for (std::size_t row = 0; row < size; ++row) {
      double off_diagonal_sum = 0.0;
      for (std::size_t column = 0; column < size; ++column) {
        if (row == column) {
          continue;
        }
        const std::int64_t raw = static_cast<std::int64_t>(rng() % 7U) - 3;
        matrix[row][column] = static_cast<double>(raw);
        off_diagonal_sum += std::abs(matrix[row][column]);
      }
      matrix[row][row] = off_diagonal_sum + 2.0 +
                         static_cast<double>(rng() % 4U);
    }
    std::vector<double> rhs(size, 0.0);
    for (double& value : rhs) {
      const std::int64_t raw = static_cast<std::int64_t>(rng() % 21U) - 10;
      value = static_cast<double>(raw);
    }

    const auto oracle = gmres_test_gaussian_elimination(matrix, rhs);
    const auto actual = gmres_solve(matrix, rhs, 1e-9, size);
    REQUIRE_EQ(actual.status, GmresStatus::converged);
    REQUIRE(actual.residual_norm <= 1e-9);
    REQUIRE(gmres_test_residual_norm(matrix, rhs, actual.solution) <= 1e-9);
    for (std::size_t index = 0; index < size; ++index) {
      REQUIRE(std::abs(actual.solution[index] - oracle[index]) <= 1e-7);
    }
  }
}

}  // namespace
