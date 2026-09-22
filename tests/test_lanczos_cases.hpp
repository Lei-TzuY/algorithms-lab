#pragma once

#include "algorithms/numerical/lanczos.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {

using LanczosMatrix = std::vector<std::vector<double>>;
using algorithms::numerical::LanczosResult;
using algorithms::numerical::LanczosStatus;

double lanczos_test_dot(const std::vector<double>& left,
                        const std::vector<double>& right) {
  double sum = 0.0;
  for (std::size_t index = 0U; index < left.size(); ++index) {
    sum += left[index] * right[index];
  }
  return sum;
}

double lanczos_test_norm(const std::vector<double>& vector) {
  double result = 0.0;
  for (const double value : vector) {
    result = std::hypot(result, value);
  }
  return result;
}

std::vector<double> lanczos_test_matvec(
    const LanczosMatrix& matrix,
    const std::vector<double>& vector) {
  std::vector<double> result(matrix.size(), 0.0);
  for (std::size_t row = 0U; row < matrix.size(); ++row) {
    for (std::size_t column = 0U; column < vector.size(); ++column) {
      result[row] += matrix[row][column] * vector[column];
    }
  }
  return result;
}

void require_lanczos_close(const double actual,
                           const double expected,
                           const double tolerance) {
  const double scale =
      std::max({1.0, std::abs(actual), std::abs(expected)});
  REQUIRE(std::abs(actual - expected) <= tolerance * scale);
}

void verify_lanczos_recurrence(
    const LanczosMatrix& matrix,
    const LanczosResult& result,
    const double tolerance) {
  REQUIRE_EQ(result.basis.size(), result.diagonal.size());
  REQUIRE_EQ(result.steps, result.basis.size());
  REQUIRE_EQ(
      result.subdiagonal.size(),
      result.basis.empty() ? 0U : result.basis.size() - 1U);
  REQUIRE_EQ(result.residual.size(),
             result.basis.empty() ? 0U : matrix.size());

  for (std::size_t row = 0U; row < result.basis.size(); ++row) {
    require_lanczos_close(
        lanczos_test_norm(result.basis[row]), 1.0, tolerance * 10.0);
    for (std::size_t column = 0U;
         column < result.basis.size(); ++column) {
      const double expected = row == column ? 1.0 : 0.0;
      require_lanczos_close(
          lanczos_test_dot(result.basis[row], result.basis[column]),
          expected, tolerance * 100.0);
    }
  }

  for (std::size_t column = 0U;
       column < result.basis.size(); ++column) {
    std::vector<double> reconstructed(
        matrix.size(), 0.0);
    for (std::size_t index = 0U; index < matrix.size(); ++index) {
      reconstructed[index] +=
          result.diagonal[column] * result.basis[column][index];
      if (column != 0U) {
        reconstructed[index] +=
            result.subdiagonal[column - 1U] *
            result.basis[column - 1U][index];
      }
      if (column + 1U < result.basis.size()) {
        reconstructed[index] +=
            result.subdiagonal[column] *
            result.basis[column + 1U][index];
      } else {
        reconstructed[index] += result.residual[index];
      }
    }

    const std::vector<double> actual =
        lanczos_test_matvec(matrix, result.basis[column]);
    for (std::size_t index = 0U; index < matrix.size(); ++index) {
      require_lanczos_close(
          actual[index], reconstructed[index], tolerance * 100.0);
    }
  }

  require_lanczos_close(
      lanczos_test_norm(result.residual),
      result.residual_norm, tolerance * 10.0);
}

struct ArnoldiReference {
  std::vector<std::vector<double>> basis;
  std::vector<std::vector<double>> hessenberg;
};

ArnoldiReference lanczos_reference_arnoldi(
    const LanczosMatrix& matrix,
    const std::vector<double>& starting_vector,
    const std::size_t max_steps,
    const double breakdown_tolerance) {
  const std::size_t size = matrix.size();
  ArnoldiReference result;
  result.hessenberg.assign(
      max_steps + 1U, std::vector<double>(max_steps, 0.0));

  std::vector<double> first = starting_vector;
  const double first_norm = lanczos_test_norm(first);
  for (double& value : first) {
    value /= first_norm;
  }
  result.basis.push_back(first);

  for (std::size_t column = 0U; column < max_steps; ++column) {
    std::vector<double> candidate =
        lanczos_test_matvec(matrix, result.basis[column]);

    // Two MGS passes deliberately differ from the production three-term
    // recurrence and serve as an independent Krylov projection reference.
    for (std::size_t pass = 0U; pass < 2U; ++pass) {
      for (std::size_t row = 0U; row <= column; ++row) {
        const double projection =
            lanczos_test_dot(result.basis[row], candidate);
        result.hessenberg[row][column] += projection;
        for (std::size_t index = 0U; index < size; ++index) {
          candidate[index] -=
              projection * result.basis[row][index];
        }
      }
    }

    const double next_norm = lanczos_test_norm(candidate);
    result.hessenberg[column + 1U][column] = next_norm;
    if (next_norm <= breakdown_tolerance ||
        column + 1U == max_steps) {
      break;
    }

    for (double& value : candidate) {
      value /= next_norm;
    }
    result.basis.push_back(std::move(candidate));
  }

  return result;
}

TEST_CASE(lanczos_validation_and_empty_contract) {
  const LanczosMatrix empty;
  const auto empty_result =
      algorithms::numerical::lanczos_tridiagonalize(
          empty, {}, 0U, 0.0);
  REQUIRE(empty_result.status == LanczosStatus::empty);
  REQUIRE(empty_result.basis.empty());
  REQUIRE(empty_result.diagonal.empty());
  REQUIRE(empty_result.subdiagonal.empty());
  REQUIRE(empty_result.residual.empty());
  REQUIRE_EQ(empty_result.steps, 0U);

  REQUIRE_THROWS_AS(
      algorithms::numerical::lanczos_tridiagonalize(
          LanczosMatrix{{1.0, 2.0}, {3.0, 4.0}},
          {1.0, 1.0}, 2U, 0.0),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::numerical::lanczos_tridiagonalize(
          LanczosMatrix{{1.0, 0.0}, {0.0}},
          {1.0, 1.0}, 2U, 0.0),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::numerical::lanczos_tridiagonalize(
          LanczosMatrix{{1.0}}, {}, 1U, 0.0),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::numerical::lanczos_tridiagonalize(
          LanczosMatrix{{1.0}}, {0.0}, 1U, 0.0),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::numerical::lanczos_tridiagonalize(
          LanczosMatrix{{1.0}}, {1.0}, 0U, 0.0),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::numerical::lanczos_tridiagonalize(
          LanczosMatrix{{1.0}}, {1.0}, 2U, 0.0),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::numerical::lanczos_tridiagonalize(
          LanczosMatrix{{1.0}}, {1.0}, 1U, -1.0),
      std::invalid_argument);
  REQUIRE_THROWS_AS(
      algorithms::numerical::lanczos_tridiagonalize(
          LanczosMatrix{{std::numeric_limits<double>::infinity()}},
          {1.0}, 1U, 0.0),
      std::invalid_argument);
}

TEST_CASE(lanczos_identity_breaks_down_after_one_basis_vector) {
  const LanczosMatrix identity{
      {1.0, 0.0, 0.0, 0.0},
      {0.0, 1.0, 0.0, 0.0},
      {0.0, 0.0, 1.0, 0.0},
      {0.0, 0.0, 0.0, 1.0}};

  const auto result =
      algorithms::numerical::lanczos_tridiagonalize(
          identity, {1.0, 2.0, 3.0, 4.0}, 4U, 1.0e-14);

  REQUIRE(result.status == LanczosStatus::invariant_subspace);
  REQUIRE_EQ(result.basis.size(), 1U);
  REQUIRE_EQ(result.diagonal.size(), 1U);
  REQUIRE(result.subdiagonal.empty());
  require_lanczos_close(result.diagonal[0], 1.0, 1.0e-13);
  REQUIRE(result.residual_norm <= 1.0e-14);
  verify_lanczos_recurrence(identity, result, 1.0e-12);
}

TEST_CASE(lanczos_step_limit_retains_terminal_residual) {
  const LanczosMatrix matrix{
      {4.0, 1.0, 0.0, 0.0},
      {1.0, 3.0, 1.0, 0.0},
      {0.0, 1.0, 2.0, 1.0},
      {0.0, 0.0, 1.0, 1.0}};

  const auto result =
      algorithms::numerical::lanczos_tridiagonalize(
          matrix, {1.0, 0.5, -0.25, 2.0}, 2U, 1.0e-14);

  REQUIRE(result.status == LanczosStatus::iteration_limit);
  REQUIRE_EQ(result.basis.size(), 2U);
  REQUIRE_EQ(result.diagonal.size(), 2U);
  REQUIRE_EQ(result.subdiagonal.size(), 1U);
  REQUIRE(result.residual_norm > 1.0e-14);
  verify_lanczos_recurrence(matrix, result, 1.0e-11);
}

TEST_CASE(lanczos_random_symmetric_matches_full_arnoldi_projection) {
  std::mt19937_64 random(0x1A4C2055ULL);
  std::uniform_int_distribution<std::size_t> size_distribution(2U, 8U);
  std::uniform_real_distribution<double> value_distribution(-2.0, 2.0);

  for (std::size_t trial = 0U; trial < 120U; ++trial) {
    const std::size_t size = size_distribution(random);
    LanczosMatrix matrix(
        size, std::vector<double>(size, 0.0));
    for (std::size_t row = 0U; row < size; ++row) {
      for (std::size_t column = row; column < size; ++column) {
        const double value = value_distribution(random);
        matrix[row][column] = value;
        matrix[column][row] = value;
      }
    }

    std::vector<double> start(size, 0.0);
    for (double& value : start) {
      value = value_distribution(random);
    }

    const double tolerance = 1.0e-12;
    const auto actual =
        algorithms::numerical::lanczos_tridiagonalize(
            matrix, start, size, tolerance);
    const auto reference =
        lanczos_reference_arnoldi(
            matrix, start, size, tolerance);

    REQUIRE_EQ(actual.basis.size(), reference.basis.size());
    REQUIRE_EQ(actual.diagonal.size(), actual.basis.size());
    REQUIRE_EQ(
        actual.subdiagonal.size(),
        actual.basis.empty() ? 0U : actual.basis.size() - 1U);

    verify_lanczos_recurrence(matrix, actual, 2.0e-9);

    for (std::size_t column = 0U;
         column < actual.basis.size(); ++column) {
      require_lanczos_close(
          actual.diagonal[column],
          reference.hessenberg[column][column],
          2.0e-9);
      if (column + 1U < actual.basis.size()) {
        require_lanczos_close(
            actual.subdiagonal[column],
            reference.hessenberg[column + 1U][column],
            2.0e-9);
      }

      for (std::size_t row = 0U; row + 1U < column; ++row) {
        require_lanczos_close(
            reference.hessenberg[row][column], 0.0, 2.0e-9);
      }
    }
  }
}

TEST_CASE(lanczos_nonfinite_intermediate_fails_closed) {
  const double huge = std::numeric_limits<double>::max();
  REQUIRE_THROWS_AS(
      algorithms::numerical::lanczos_tridiagonalize(
          LanczosMatrix{{huge, huge}, {huge, huge}},
          {1.0, 1.0}, 2U, 0.0),
      std::overflow_error);
}

}  // namespace
