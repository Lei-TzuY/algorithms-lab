#pragma once

#include "algorithms/linear_algebra/characteristic_polynomial.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

namespace characteristic_polynomial_test_detail {

using Matrix = std::vector<std::vector<std::int64_t>>;

[[nodiscard]] inline std::int64_t leibniz_determinant(
    const Matrix& matrix) {
  const std::size_t n = matrix.size();
  if (n == 0U) {
    return 1;
  }

  std::vector<std::size_t> permutation(n);
  std::iota(permutation.begin(), permutation.end(),
            std::size_t{0});

  std::int64_t total = 0;
  do {
    std::int64_t product = 1;
    for (std::size_t row = 0U; row < n; ++row) {
      product *= matrix[row][permutation[row]];
    }

    std::size_t inversions = 0U;
    for (std::size_t left = 0U; left < n; ++left) {
      for (std::size_t right = left + 1U; right < n; ++right) {
        if (permutation[left] > permutation[right]) {
          ++inversions;
        }
      }
    }

    total += (inversions % 2U == 0U) ? product : -product;
  } while (std::next_permutation(permutation.begin(),
                                  permutation.end()));

  return total;
}

[[nodiscard]] inline std::int64_t determinant_at_lambda(
    const Matrix& matrix, const std::int64_t lambda) {
  Matrix shifted = matrix;
  for (std::size_t row = 0U; row < shifted.size(); ++row) {
    for (std::size_t column = 0U;
         column < shifted.size(); ++column) {
      shifted[row][column] =
          row == column ? lambda - matrix[row][column]
                        : -matrix[row][column];
    }
  }
  return leibniz_determinant(shifted);
}

[[nodiscard]] inline std::int64_t evaluate_polynomial(
    const std::vector<std::int64_t>& coefficients,
    const std::int64_t lambda) {
  std::int64_t value = 0;
  for (const std::int64_t coefficient : coefficients) {
    value = value * lambda + coefficient;
  }
  return value;
}

inline void require_matches_determinant_oracle(
    const Matrix& matrix,
    const std::vector<std::int64_t>& coefficients) {
  REQUIRE_EQ(coefficients.size(), matrix.size() + 1U);
  REQUIRE_EQ(coefficients.front(), std::int64_t{1});

  for (std::int64_t lambda = -4; lambda <= 4; ++lambda) {
    REQUIRE_EQ(
        evaluate_polynomial(coefficients, lambda),
        determinant_at_lambda(matrix, lambda));
  }
}

}  // namespace characteristic_polynomial_test_detail

TEST_CASE(characteristic_polynomial_empty_singleton_and_known_matrices) {
  using algorithms::linear_algebra::integer_characteristic_polynomial;
  using characteristic_polynomial_test_detail::Matrix;

  REQUIRE(
      integer_characteristic_polynomial(Matrix{}) ==
      std::vector<std::int64_t>{1});

  REQUIRE(
      integer_characteristic_polynomial(Matrix{{7}}) ==
      std::vector<std::int64_t>({1, -7}));

  REQUIRE(
      integer_characteristic_polynomial(
          Matrix{{1, 2}, {3, 4}}) ==
      std::vector<std::int64_t>({1, -5, -2}));

  REQUIRE(
      integer_characteristic_polynomial(
          Matrix{{2, 0, 0}, {0, -3, 0}, {0, 0, 5}}) ==
      std::vector<std::int64_t>({1, -4, -11, 30}));

  REQUIRE(
      integer_characteristic_polynomial(
          Matrix{{0, 1, 0}, {0, 0, 1}, {0, 0, 0}}) ==
      std::vector<std::int64_t>({1, 0, 0, 0}));
}

TEST_CASE(characteristic_polynomial_validation_and_overflow_contract) {
  using algorithms::linear_algebra::integer_characteristic_polynomial;
  using characteristic_polynomial_test_detail::Matrix;

  REQUIRE_THROWS_AS(
      integer_characteristic_polynomial(
          Matrix{{1, 2}, {3}}),
      std::invalid_argument);

  REQUIRE_THROWS_AS(
      integer_characteristic_polynomial(
          Matrix{{std::numeric_limits<std::int64_t>::min()}}),
      std::overflow_error);

  const auto maximum =
      std::numeric_limits<std::int64_t>::max();
  REQUIRE_THROWS_AS(
      integer_characteristic_polynomial(
          Matrix{{maximum, maximum},
                 {maximum, maximum}}),
      std::overflow_error);
}

TEST_CASE(characteristic_polynomial_structural_examples_match_oracle) {
  using algorithms::linear_algebra::integer_characteristic_polynomial;
  using characteristic_polynomial_test_detail::Matrix;
  using characteristic_polynomial_test_detail::
      require_matches_determinant_oracle;

  const std::vector<Matrix> cases{
      Matrix{{0}},
      Matrix{{1, -2}, {4, 3}},
      Matrix{{0, 1, 0}, {0, 0, 1}, {-6, 11, -6}},
      Matrix{{2, 1, 0, 0},
             {1, 2, 1, 0},
             {0, 1, 2, 1},
             {0, 0, 1, 2}},
  };

  for (const Matrix& matrix : cases) {
    require_matches_determinant_oracle(
        matrix, integer_characteristic_polynomial(matrix));
  }
}

TEST_CASE(characteristic_polynomial_randomized_determinant_differential) {
  using algorithms::linear_algebra::integer_characteristic_polynomial;
  using characteristic_polynomial_test_detail::Matrix;
  using characteristic_polynomial_test_detail::
      require_matches_determinant_oracle;

  std::mt19937_64 random(0xC4A2F011ULL);
  for (std::size_t trial = 0U; trial < 900U; ++trial) {
    const std::size_t n =
        static_cast<std::size_t>(random() % 5U);
    Matrix matrix(n, std::vector<std::int64_t>(n, 0));
    for (auto& row : matrix) {
      for (std::int64_t& value : row) {
        value = static_cast<std::int64_t>(random() % 7U) - 3;
      }
    }

    const auto coefficients =
        integer_characteristic_polynomial(matrix);
    require_matches_determinant_oracle(matrix, coefficients);

    if (n != 0U) {
      std::int64_t trace = 0;
      for (std::size_t index = 0U; index < n; ++index) {
        trace += matrix[index][index];
      }
      REQUIRE_EQ(coefficients[1U], -trace);
    }
  }
}
