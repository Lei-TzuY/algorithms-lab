#pragma once

#include "algorithms/numerical/cholesky.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <stdexcept>
#include <vector>

namespace {

using Matrix = std::vector<std::vector<double>>;

bool approx(double left, double right, double tolerance = 1e-9) {
  const double scale = 1.0 + std::max(std::abs(left), std::abs(right));
  return std::abs(left - right) <= tolerance * scale;
}

Matrix reconstruct(const algorithms::numerical::CholeskyFactorization& factor) {
  const std::size_t n = factor.lower.size();
  Matrix result(n, std::vector<double>(n, 0.0));
  for (std::size_t row = 0; row < n; ++row) {
    for (std::size_t column = 0; column < n; ++column) {
      long double sum = 0.0L;
      const std::size_t limit = std::min(row, column);
      for (std::size_t k = 0; k <= limit; ++k) {
        sum += static_cast<long double>(factor.lower[row][k]) *
               static_cast<long double>(factor.lower[column][k]);
      }
      result[row][column] = static_cast<double>(sum);
    }
  }
  return result;
}

std::vector<double> gaussian_solve(Matrix matrix, std::vector<double> rhs) {
  const std::size_t n = matrix.size();
  for (std::size_t pivot = 0; pivot < n; ++pivot) {
    std::size_t best = pivot;
    for (std::size_t row = pivot + 1U; row < n; ++row) {
      if (std::abs(matrix[row][pivot]) > std::abs(matrix[best][pivot])) {
        best = row;
      }
    }
    REQUIRE(std::abs(matrix[best][pivot]) > 1e-14);
    if (best != pivot) {
      std::swap(matrix[best], matrix[pivot]);
      std::swap(rhs[best], rhs[pivot]);
    }
    for (std::size_t row = pivot + 1U; row < n; ++row) {
      const double factor = matrix[row][pivot] / matrix[pivot][pivot];
      matrix[row][pivot] = 0.0;
      for (std::size_t column = pivot + 1U; column < n; ++column) {
        matrix[row][column] -= factor * matrix[pivot][column];
      }
      rhs[row] -= factor * rhs[pivot];
    }
  }
  std::vector<double> result(n, 0.0);
  for (std::size_t offset = 0; offset < n; ++offset) {
    const std::size_t row = n - 1U - offset;
    double residual = rhs[row];
    for (std::size_t column = row + 1U; column < n; ++column) {
      residual -= matrix[row][column] * result[column];
    }
    result[row] = residual / matrix[row][row];
  }
  return result;
}

Matrix make_spd(std::mt19937_64& rng, std::size_t n) {
  Matrix basis(n, std::vector<double>(n, 0.0));
  for (std::size_t row = 0; row < n; ++row) {
    for (std::size_t column = 0; column < n; ++column) {
      const std::int64_t raw = static_cast<std::int64_t>(rng() % 11ULL) - 5;
      basis[row][column] = static_cast<double>(raw);
    }
  }
  Matrix matrix(n, std::vector<double>(n, 0.0));
  for (std::size_t row = 0; row < n; ++row) {
    for (std::size_t column = row; column < n; ++column) {
      double sum = 0.0;
      for (std::size_t k = 0; k < n; ++k) {
        sum += basis[k][row] * basis[k][column];
      }
      if (row == column) {
        sum += static_cast<double>(n + 1U);
      }
      matrix[row][column] = sum;
      matrix[column][row] = sum;
    }
  }
  return matrix;
}

void require_factor_replays(const Matrix& matrix,
                            const algorithms::numerical::CholeskyFactorization& factor) {
  const Matrix replay = reconstruct(factor);
  REQUIRE_EQ(replay.size(), matrix.size());
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    REQUIRE_EQ(factor.lower[row].size(), matrix.size());
    REQUIRE(factor.lower[row][row] > 0.0);
    for (std::size_t column = 0; column < matrix.size(); ++column) {
      if (column > row) {
        REQUIRE_EQ(factor.lower[row][column], 0.0);
      }
      REQUIRE(approx(replay[row][column], matrix[row][column], 2e-10));
    }
  }
}

TEST_CASE(cholesky_known_factor_and_solve) {
  const Matrix matrix{{4.0, 12.0, -16.0},
                      {12.0, 37.0, -43.0},
                      {-16.0, -43.0, 98.0}};
  const auto factor = algorithms::numerical::cholesky_factorize(matrix);
  REQUIRE(approx(factor.lower[0][0], 2.0));
  REQUIRE(approx(factor.lower[1][0], 6.0));
  REQUIRE(approx(factor.lower[1][1], 1.0));
  REQUIRE(approx(factor.lower[2][0], -8.0));
  REQUIRE(approx(factor.lower[2][1], 5.0));
  REQUIRE(approx(factor.lower[2][2], 3.0));
  require_factor_replays(matrix, factor);

  const std::vector<double> expected{1.25, -0.5, 2.0};
  std::vector<double> rhs(3, 0.0);
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column) {
      rhs[row] += matrix[row][column] * expected[column];
    }
  }
  const auto solved = algorithms::numerical::cholesky_solve(factor, rhs);
  for (std::size_t index = 0; index < expected.size(); ++index) {
    REQUIRE(approx(solved[index], expected[index], 1e-11));
  }
}

TEST_CASE(cholesky_validation_and_breakdown_contract) {
  const auto empty = algorithms::numerical::cholesky_factorize({});
  REQUIRE(empty.lower.empty());
  REQUIRE(algorithms::numerical::cholesky_solve(empty, {}).empty());

  REQUIRE_THROWS_AS(algorithms::numerical::cholesky_factorize({{1.0, 2.0}}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::numerical::cholesky_factorize({{2.0, 1.0}, {0.0, 2.0}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(algorithms::numerical::cholesky_factorize(
                        {{std::numeric_limits<double>::infinity()}}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::numerical::cholesky_factorize({{1.0, 1.0}, {1.0, 1.0}}),
      std::domain_error);
  REQUIRE_THROWS_AS(
      algorithms::numerical::cholesky_factorize({{1.0, 2.0}, {2.0, 1.0}}),
      std::domain_error);

  algorithms::numerical::CholeskyFactorization malformed{{{1.0, 1.0},
                                                           {0.0, 1.0}}};
  REQUIRE_THROWS_AS(algorithms::numerical::cholesky_solve(malformed, {1.0, 2.0}),
                    std::invalid_argument);
  const auto one = algorithms::numerical::cholesky_factorize({{4.0}});
  REQUIRE_THROWS_AS(algorithms::numerical::cholesky_solve(one, {}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(algorithms::numerical::cholesky_solve(
                        one, {std::numeric_limits<double>::quiet_NaN()}),
                    std::invalid_argument);
}

TEST_CASE(cholesky_random_spd_vs_gaussian) {
  std::mt19937_64 rng(0xC40135EULL);
  for (std::size_t trial = 0; trial < 700; ++trial) {
    const std::size_t n = 1U + static_cast<std::size_t>(rng() % 10ULL);
    const Matrix matrix = make_spd(rng, n);
    const auto factor = algorithms::numerical::cholesky_factorize(matrix);
    require_factor_replays(matrix, factor);

    std::vector<double> rhs(n, 0.0);
    for (double& value : rhs) {
      const std::int64_t raw = static_cast<std::int64_t>(rng() % 41ULL) - 20;
      value = static_cast<double>(raw);
    }
    const auto solved = algorithms::numerical::cholesky_solve(factor, rhs);
    const auto oracle = gaussian_solve(matrix, rhs);
    for (std::size_t index = 0; index < n; ++index) {
      REQUIRE(approx(solved[index], oracle[index], 2e-9));
    }
  }
}

TEST_CASE(cholesky_extreme_diagonal_and_determinism) {
  const double high = std::numeric_limits<double>::max();
  const Matrix matrix{{high, 0.0}, {0.0, std::numeric_limits<double>::min()}};
  const auto first = algorithms::numerical::cholesky_factorize(matrix);
  const auto second = algorithms::numerical::cholesky_factorize(matrix);
  REQUIRE_EQ(first.lower, second.lower);
  require_factor_replays(matrix, first);
  const auto solved = algorithms::numerical::cholesky_solve(first, {1.0, 1.0});
  REQUIRE(std::isfinite(solved[0]));
  REQUIRE(std::isfinite(solved[1]));
}

}  // namespace
