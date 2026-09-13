#pragma once

#include "test_svd_helpers.hpp"

TEST_CASE(svd_validation_known_shapes_and_rank_deficiency) {
  using algorithms::numerical::one_sided_jacobi_svd;
  using svd_test_detail::Matrix;

  svd_test_detail::require_factorization_invariants({});
  svd_test_detail::require_factorization_invariants({{}, {}, {}});
  svd_test_detail::require_factorization_invariants(
      {{3.0, 0.0}, {0.0, 2.0}, {0.0, 0.0}});
  svd_test_detail::require_factorization_invariants(
      {{1.0, 2.0}, {2.0, 4.0}, {3.0, 6.0}});
  svd_test_detail::require_factorization_invariants(
      {{1.0, 2.0, 3.0}, {4.0, 5.0, 6.0}});
  svd_test_detail::require_factorization_invariants(
      {{1.0, 0.0}, {0.0, 1.0}});

  REQUIRE_THROWS_AS(one_sided_jacobi_svd({{1.0}, {2.0, 3.0}}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(
      one_sided_jacobi_svd(
          {{1.0, std::numeric_limits<double>::infinity()}}),
      std::invalid_argument);
  REQUIRE_THROWS_AS(one_sided_jacobi_svd({{1.0}}, 0.0),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(one_sided_jacobi_svd({{1.0}}, 1.0e-12, 0U),
                    std::invalid_argument);
  const double max = std::numeric_limits<double>::max();
  REQUIRE_THROWS_AS(one_sided_jacobi_svd({{max, max}, {max, -max}}),
                    std::overflow_error);
}

TEST_CASE(svd_randomized_reconstruction_and_singular_equations) {
  using svd_test_detail::Matrix;
  std::mt19937_64 random(0x5356445245434f56ULL);
  std::uniform_int_distribution<int> dimension(1, 6);
  std::uniform_real_distribution<double> value(-4.0, 4.0);
  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    const std::size_t rows = static_cast<std::size_t>(dimension(random));
    const std::size_t columns = static_cast<std::size_t>(dimension(random));
    Matrix matrix(rows, std::vector<double>(columns, 0.0));
    for (auto& row : matrix) {
      for (double& element : row) {
        element = value(random);
      }
    }
    svd_test_detail::require_factorization_invariants(matrix);
  }
}

TEST_CASE(svd_randomized_two_by_two_closed_form_oracle) {
  using svd_test_detail::Matrix;
  std::mt19937_64 random(0x5356443242593255ULL);
  std::uniform_real_distribution<double> value(-6.0, 6.0);
  for (std::size_t trial = 0U; trial < 500U; ++trial) {
    Matrix matrix(2U, std::vector<double>(2U, 0.0));
    for (auto& row : matrix) {
      for (double& element : row) {
        element = value(random);
      }
    }
    svd_test_detail::require_2x2_closed_form(matrix);
  }
}

TEST_CASE(svd_repeatability_and_ordering) {
  const std::vector<std::vector<double>> matrix{
      {4.0, -1.0, 2.0}, {0.5, 3.0, -2.0}, {1.0, 1.0, 1.0}, {2.0, -3.0, 5.0}};
  const auto first = algorithms::numerical::one_sided_jacobi_svd(matrix);
  const auto second = algorithms::numerical::one_sided_jacobi_svd(matrix);
  REQUIRE(first.u == second.u);
  REQUIRE(first.v == second.v);
  REQUIRE(first.singular_values == second.singular_values);
  REQUIRE_EQ(first.sweeps, second.sweeps);
}
