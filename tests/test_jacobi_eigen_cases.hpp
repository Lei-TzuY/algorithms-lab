#pragma once

#include "algorithms/numerical/jacobi_eigen.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace jacobi_eigen_test_detail {

inline bool close_enough(double left, double right, double tolerance) {
  return std::abs(left - right) <=
         tolerance * std::max({1.0, std::abs(left), std::abs(right)});
}

inline void verify_decomposition(
    const std::vector<std::vector<double>>& matrix,
    const algorithms::numerical::JacobiEigenResult& result,
    double tolerance) {
  const std::size_t size = matrix.size();
  REQUIRE_EQ(result.eigenvalues.size(), size);
  REQUIRE_EQ(result.eigenvectors.size(), size);
  for (const auto& row : result.eigenvectors) {
    REQUIRE_EQ(row.size(), size);
  }

  for (std::size_t left = 0U; left < size; ++left) {
    for (std::size_t right = 0U; right < size; ++right) {
      double inner = 0.0;
      for (std::size_t row = 0U; row < size; ++row) {
        inner += result.eigenvectors[row][left] *
                 result.eigenvectors[row][right];
      }
      REQUIRE(close_enough(inner, left == right ? 1.0 : 0.0, tolerance));
    }

    for (std::size_t row = 0U; row < size; ++row) {
      double transformed = 0.0;
      for (std::size_t column = 0U; column < size; ++column) {
        transformed += matrix[row][column] *
                       result.eigenvectors[column][left];
      }
      const double expected =
          result.eigenvectors[row][left] * result.eigenvalues[left];
      REQUIRE(close_enough(transformed, expected, tolerance));
    }
  }
}

inline std::vector<std::vector<double>> matrix_with_known_spectrum(
    const std::vector<double>& spectrum, std::mt19937_64& random) {
  const std::size_t size = spectrum.size();
  std::vector<std::vector<double>> orthogonal(
      size, std::vector<double>(size, 0.0));
  for (std::size_t index = 0U; index < size; ++index) {
    orthogonal[index][index] = 1.0;
  }

  constexpr double cosines[] = {3.0 / 5.0, 5.0 / 13.0, 8.0 / 17.0};
  constexpr double sines[] = {4.0 / 5.0, 12.0 / 13.0, 15.0 / 17.0};
  if (size > 1U) {
    for (std::size_t step = 0U; step < 4U * size; ++step) {
      std::size_t first = static_cast<std::size_t>(random() % size);
      std::size_t second = static_cast<std::size_t>(random() % size);
      if (first == second) {
        second = (second + 1U) % size;
      }
      const std::size_t choice = static_cast<std::size_t>(random() % 3U);
      const double cosine = cosines[choice];
      const double sign = (random() & 1U) == 0U ? 1.0 : -1.0;
      const double sine = sign * sines[choice];
      for (std::size_t row = 0U; row < size; ++row) {
        const double left = orthogonal[row][first];
        const double right = orthogonal[row][second];
        orthogonal[row][first] = cosine * left - sine * right;
        orthogonal[row][second] = sine * left + cosine * right;
      }
    }
  }

  std::vector<std::vector<double>> matrix(
      size, std::vector<double>(size, 0.0));
  for (std::size_t row = 0U; row < size; ++row) {
    for (std::size_t column = row; column < size; ++column) {
      double value = 0.0;
      for (std::size_t index = 0U; index < size; ++index) {
        value += orthogonal[row][index] * spectrum[index] *
                 orthogonal[column][index];
      }
      matrix[row][column] = value;
      matrix[column][row] = value;
    }
  }
  return matrix;
}

}  // namespace jacobi_eigen_test_detail

TEST_CASE(jacobi_eigen_validation_and_known_spectrum) {
  using algorithms::numerical::symmetric_jacobi_eigendecomposition;
  using jacobi_eigen_test_detail::close_enough;
  using jacobi_eigen_test_detail::verify_decomposition;

  const auto empty = symmetric_jacobi_eigendecomposition({});
  REQUIRE(empty.converged);
  REQUIRE(empty.eigenvalues.empty());

  const auto singleton = symmetric_jacobi_eigendecomposition({{7.0}});
  REQUIRE(singleton.converged);
  REQUIRE_EQ(singleton.eigenvalues, std::vector<double>{7.0});

  const std::vector<std::vector<double>> matrix{{2.0, 1.0}, {1.0, 2.0}};
  const auto known = symmetric_jacobi_eigendecomposition(matrix);
  REQUIRE(known.converged);
  REQUIRE(close_enough(known.eigenvalues[0], 1.0, 1e-12));
  REQUIRE(close_enough(known.eigenvalues[1], 3.0, 1e-12));
  verify_decomposition(matrix, known, 1e-10);

  const std::vector<std::vector<double>> repeated_matrix{
      {5.0, 0.0, 0.0}, {0.0, 5.0, 0.0}, {0.0, 0.0, -2.0}};
  const auto first = symmetric_jacobi_eigendecomposition(repeated_matrix);
  const auto second = symmetric_jacobi_eigendecomposition(repeated_matrix);
  REQUIRE_EQ(first.eigenvalues, second.eigenvalues);
  REQUIRE_EQ(first.eigenvectors, second.eigenvectors);
}

TEST_CASE(jacobi_eigen_rejects_malformed_inputs) {
  using algorithms::numerical::symmetric_jacobi_eigendecomposition;

  REQUIRE_THROWS_AS(
      symmetric_jacobi_eigendecomposition({{1.0, 2.0}, {2.0}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      symmetric_jacobi_eigendecomposition({{1.0, 2.0}, {3.0, 4.0}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      symmetric_jacobi_eigendecomposition(
          {{std::numeric_limits<double>::infinity()}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(symmetric_jacobi_eigendecomposition({{1.0}}, -1.0),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(symmetric_jacobi_eigendecomposition({{1.0}}, 1.1),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(symmetric_jacobi_eigendecomposition({{1.0}}, 1e-12, 0U),
                    std::invalid_argument);
}

TEST_CASE(jacobi_eigen_reports_budget_and_arithmetic_boundaries) {
  using algorithms::numerical::symmetric_jacobi_eigendecomposition;

  const std::vector<std::vector<double>> dense{
      {4.0, 1.0, 2.0, 0.0}, {1.0, 3.0, 1.0, 1.0},
      {2.0, 1.0, 2.0, 1.0}, {0.0, 1.0, 1.0, 1.0}};
  const auto budgeted = symmetric_jacobi_eigendecomposition(dense, 0.0, 1U);
  REQUIRE(!budgeted.converged);
  REQUIRE_EQ(budgeted.sweeps_executed, 1U);
  REQUIRE(budgeted.maximum_off_diagonal > 0.0);

  const double maximum = std::numeric_limits<double>::max();
  REQUIRE_THROWS_AS(
      symmetric_jacobi_eigendecomposition(
          {{maximum, maximum}, {maximum, -maximum}}),
      std::overflow_error);

  const double tiny = std::numeric_limits<double>::denorm_min();
  const auto extreme = symmetric_jacobi_eigendecomposition(
      {{1e308, tiny}, {tiny, -1e308}}, 0.0, 2U);
  REQUIRE(!extreme.converged);
}

TEST_CASE(jacobi_eigen_randomized_known_spectrum_and_certificates) {
  using algorithms::numerical::symmetric_jacobi_eigendecomposition;
  using jacobi_eigen_test_detail::close_enough;
  using jacobi_eigen_test_detail::matrix_with_known_spectrum;
  using jacobi_eigen_test_detail::verify_decomposition;

  std::mt19937_64 random(0xE16E5EEDULL);
  for (std::size_t trial = 0U; trial < 600U; ++trial) {
    const std::size_t size = 1U + static_cast<std::size_t>(random() % 10U);
    std::vector<double> spectrum(size, 0.0);
    for (double& value : spectrum) {
      const std::int64_t sample = static_cast<std::int64_t>(random() % 31U) - 15;
      value = static_cast<double>(sample);
    }
    std::sort(spectrum.begin(), spectrum.end());

    const auto matrix = matrix_with_known_spectrum(spectrum, random);
    const auto result =
        symmetric_jacobi_eigendecomposition(matrix, 1e-12, 80U);
    REQUIRE(result.converged);
    REQUIRE_EQ(result.eigenvalues.size(), spectrum.size());
    for (std::size_t index = 0U; index < size; ++index) {
      REQUIRE(close_enough(result.eigenvalues[index], spectrum[index], 2e-8));
    }
    verify_decomposition(matrix, result, 5e-8);
  }
}
