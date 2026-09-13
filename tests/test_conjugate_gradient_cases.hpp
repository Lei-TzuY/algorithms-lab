#include "algorithms/numerical/conjugate_gradient.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <random>
#include <cstdint>
#include <stdexcept>
#include <vector>

using algorithms::numerical::ConjugateGradientStatus;
using algorithms::numerical::conjugate_gradient_solve;

namespace cg_recovery_test_detail {

bool cg_close(double left, double right, double tolerance = 1e-8) {
  return std::abs(left - right) <= tolerance * (1.0 + std::abs(right));
}

std::vector<double> cg_matvec(const std::vector<std::vector<double>>& matrix,
                           const std::vector<double>& vector) {
  std::vector<double> result(matrix.size(), 0.0);
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    for (std::size_t column = 0; column < vector.size(); ++column) {
      result[row] += matrix[row][column] * vector[column];
    }
  }
  return result;
}

double cg_residual_norm(const std::vector<std::vector<double>>& matrix,
                     const std::vector<double>& rhs,
                     const std::vector<double>& solution) {
  const auto ax = cg_matvec(matrix, solution);
  double sum = 0.0;
  for (std::size_t index = 0; index < rhs.size(); ++index) {
    const double difference = rhs[index] - ax[index];
    sum += difference * difference;
  }
  return std::sqrt(sum);
}

std::vector<double> cg_gaussian_solve(std::vector<std::vector<double>> matrix,
                                   std::vector<double> rhs) {
  const std::size_t size = matrix.size();
  for (std::size_t pivot = 0; pivot < size; ++pivot) {
    std::size_t best = pivot;
    for (std::size_t row = pivot + 1U; row < size; ++row) {
      if (std::abs(matrix[row][pivot]) > std::abs(matrix[best][pivot])) {
        best = row;
      }
    }
    REQUIRE(std::abs(matrix[best][pivot]) > 1e-12);
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
  for (std::size_t offset = 0; offset < size; ++offset) {
    const std::size_t row = size - 1U - offset;
    double sum = rhs[row];
    for (std::size_t column = row + 1U; column < size; ++column) {
      sum -= matrix[row][column] * solution[column];
    }
    solution[row] = sum / matrix[row][row];
  }
  return solution;
}

std::int64_t cg_sample_signed(std::mt19937_64& random, std::uint64_t span) {
  const std::uint64_t width = 2U * span + 1U;
  const std::uint64_t raw = random() % width;
  return static_cast<std::int64_t>(raw) - static_cast<std::int64_t>(span);
}

std::vector<std::vector<double>> cg_random_spd(std::mt19937_64& random,
                                            std::size_t size) {
  std::vector<std::vector<double>> basis(size, std::vector<double>(size, 0.0));
  for (auto& row : basis) {
    for (double& entry : row) {
      entry = static_cast<double>(cg_sample_signed(random, 3U));
    }
  }
  std::vector<std::vector<double>> matrix(size, std::vector<double>(size, 0.0));
  for (std::size_t row = 0; row < size; ++row) {
    for (std::size_t column = 0; column < size; ++column) {
      double sum = 0.0;
      for (std::size_t inner = 0; inner < size; ++inner) {
        sum += basis[inner][row] * basis[inner][column];
      }
      if (row == column) {
        sum += 2.0;
      }
      matrix[row][column] = sum;
    }
  }
  return matrix;
}


}  // namespace cg_recovery_test_detail

TEST_CASE(conjugate_gradient_validation_and_empty_semantics) {
  const auto empty = conjugate_gradient_solve({}, {}, 1e-12, 0U);
  REQUIRE(empty.status == ConjugateGradientStatus::converged);
  REQUIRE(empty.solution.empty());
  REQUIRE(empty.iterations == 0U);

  REQUIRE_THROWS_AS(conjugate_gradient_solve({{1.0, 0.0}}, {}, 1e-12, 4U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(conjugate_gradient_solve({{1.0}, {2.0}}, {1.0, 2.0}, 1e-12, 4U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(conjugate_gradient_solve({{1.0, 2.0}, {3.0, 4.0}}, {1.0, 2.0}, 1e-12, 4U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(conjugate_gradient_solve({{1.0}}, {1.0}, 0.0, 4U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(conjugate_gradient_solve({{1.0}}, {1.0}, 1e-12, 0U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(conjugate_gradient_solve({{1.0}}, {1.0}, 1e-12, 4U, {1.0, 2.0}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(conjugate_gradient_solve({{std::numeric_limits<double>::infinity()}}, {1.0}, 1e-12, 4U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(conjugate_gradient_solve({{1e308}}, {1.0}, 1e-12, 4U, {1e308}),
                    std::overflow_error);
}

TEST_CASE(conjugate_gradient_known_spd_and_initial_guess) {
  const std::vector<std::vector<double>> matrix{{4.0, 1.0}, {1.0, 3.0}};
  const std::vector<double> rhs{1.0, 2.0};
  const auto result = conjugate_gradient_solve(matrix, rhs, 1e-13, 8U);
  REQUIRE(result.status == ConjugateGradientStatus::converged);
  REQUIRE(cg_recovery_test_detail::cg_close(result.solution[0], 1.0 / 11.0, 1e-10));
  REQUIRE(cg_recovery_test_detail::cg_close(result.solution[1], 7.0 / 11.0, 1e-10));
  REQUIRE(result.residual_norm <= 1e-12);
  REQUIRE(result.iterations <= 2U);

  const std::vector<double> exact{1.0 / 11.0, 7.0 / 11.0};
  const auto warm = conjugate_gradient_solve(matrix, rhs, 1e-12, 8U, exact);
  REQUIRE(warm.status == ConjugateGradientStatus::converged);
  REQUIRE(warm.iterations == 0U);
}

TEST_CASE(conjugate_gradient_iteration_limit_and_curvature_boundary) {
  const std::vector<std::vector<double>> matrix{{4.0, 1.0, 0.0},
                                                {1.0, 3.0, 1.0},
                                                {0.0, 1.0, 2.0}};
  const std::vector<double> rhs{1.0, 2.0, 3.0};
  const auto limited = conjugate_gradient_solve(matrix, rhs, 1e-15, 1U);
  REQUIRE(limited.status == ConjugateGradientStatus::iteration_limit);
  REQUIRE(limited.iterations == 1U);
  REQUIRE(limited.residual_norm > 1e-15);

  const auto breakdown = conjugate_gradient_solve({{1.0, 0.0}, {0.0, -1.0}},
                                                   {0.0, 1.0}, 1e-12, 4U);
  REQUIRE(breakdown.status == ConjugateGradientStatus::non_positive_curvature);
  REQUIRE(breakdown.iterations == 0U);
}

TEST_CASE(conjugate_gradient_random_spd_differential) {
  std::mt19937_64 random(0xC0A7E6A7ULL);
  for (std::size_t trial = 0; trial < 500U; ++trial) {
    const std::size_t size = 1U + static_cast<std::size_t>(random() % 12U);
    const auto matrix = cg_recovery_test_detail::cg_random_spd(random, size);
    std::vector<double> rhs(size, 0.0);
    for (double& entry : rhs) {
      entry = static_cast<double>(cg_recovery_test_detail::cg_sample_signed(random, 5U));
    }
    const auto oracle = cg_recovery_test_detail::cg_gaussian_solve(matrix, rhs);
    const auto result = conjugate_gradient_solve(matrix, rhs, 1e-10, size * 6U);
    REQUIRE(result.status == ConjugateGradientStatus::converged);
    REQUIRE(result.solution.size() == size);
    REQUIRE(result.iterations <= size * 6U);
    REQUIRE(cg_recovery_test_detail::cg_residual_norm(matrix, rhs, result.solution) <= 2e-9);
    for (std::size_t index = 0; index < size; ++index) {
      REQUIRE(cg_recovery_test_detail::cg_close(result.solution[index], oracle[index], 2e-8));
    }
  }
}
