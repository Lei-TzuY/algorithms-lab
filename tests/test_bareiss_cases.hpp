#pragma once

#include "algorithms/linear_algebra/bareiss_determinant.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numeric>
#include <random>
#include <stdexcept>
#include <vector>

namespace bareiss_recovery_tests {
using Matrix = std::vector<std::vector<std::int64_t>>;

[[nodiscard]] inline std::int64_t leibniz_determinant(const Matrix& matrix) {
  const std::size_t n = matrix.size();
  if (n == 0) return 1;
  std::vector<std::size_t> permutation(n);
  std::iota(permutation.begin(), permutation.end(), std::size_t{0});
  std::int64_t total = 0;
  do {
    std::int64_t product = 1;
    for (std::size_t row = 0; row < n; ++row) {
      product *= matrix[row][permutation[row]];
    }
    std::size_t inversions = 0;
    for (std::size_t left = 0; left < n; ++left) {
      for (std::size_t right = left + 1; right < n; ++right) {
        if (permutation[left] > permutation[right]) ++inversions;
      }
    }
    total += inversions % 2U == 0U ? product : -product;
  } while (std::next_permutation(permutation.begin(), permutation.end()));
  return total;
}

}  // namespace bareiss_recovery_tests

TEST_CASE(bareiss_empty_singleton_and_known_matrices) {
  using algorithms::linear_algebra::bareiss_determinant;
  using bareiss_recovery_tests::Matrix;
  REQUIRE_EQ(bareiss_determinant(Matrix{}), std::int64_t{1});
  REQUIRE_EQ(bareiss_determinant(Matrix{{-7}}), std::int64_t{-7});
  REQUIRE_EQ(bareiss_determinant(Matrix{{1, 2}, {3, 4}}), std::int64_t{-2});
  REQUIRE_EQ(bareiss_determinant(Matrix{{0, 2}, {3, 4}}), std::int64_t{-6});
  REQUIRE_EQ(
      bareiss_determinant(Matrix{{1, 1, 0}, {1, 1, 1}, {0, 1, 1}}),
      std::int64_t{-1});
  REQUIRE_EQ(bareiss_determinant(Matrix{{6, 1, 1}, {4, -2, 5}, {2, 8, 7}}),
             std::int64_t{-306});
}

TEST_CASE(bareiss_singular_and_validation) {
  using algorithms::linear_algebra::bareiss_determinant;
  using bareiss_recovery_tests::Matrix;
  REQUIRE_EQ(bareiss_determinant(Matrix{{1, 2, 3}, {2, 4, 6}, {7, 8, 9}}),
             std::int64_t{0});
  REQUIRE_EQ(bareiss_determinant(Matrix{{0, 1, 2}, {0, 3, 4}, {0, 5, 6}}),
             std::int64_t{0});
  REQUIRE_THROWS_AS(bareiss_determinant(Matrix{{1, 2}, {3}}),
                    std::invalid_argument);
}

TEST_CASE(bareiss_bounded_overflow_contract) {
  using algorithms::linear_algebra::bareiss_determinant;
  using bareiss_recovery_tests::Matrix;
  const auto maximum = std::numeric_limits<std::int64_t>::max();
  REQUIRE_THROWS_AS(bareiss_determinant(Matrix{{maximum, maximum},
                                                {maximum, maximum}}),
                    std::overflow_error);
  REQUIRE_THROWS_AS(bareiss_determinant(Matrix{{maximum, 0}, {0, 2}}),
                    std::overflow_error);
}

TEST_CASE(bareiss_randomized_differential_against_leibniz) {
  using algorithms::linear_algebra::bareiss_determinant;
  using bareiss_recovery_tests::Matrix;
  using bareiss_recovery_tests::leibniz_determinant;

  std::mt19937_64 rng(0xBA4E155ULL);
  for (int trial = 0; trial < 2000; ++trial) {
    const std::size_t n = static_cast<std::size_t>(rng() % 7U);
    Matrix matrix(n, std::vector<std::int64_t>(n, 0));
    for (auto& row : matrix) {
      for (auto& value : row) {
        value = static_cast<std::int64_t>(rng() % 11U) - 5;
      }
    }
    REQUIRE_EQ(bareiss_determinant(matrix), leibniz_determinant(matrix));
  }
}
