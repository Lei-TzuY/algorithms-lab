#include "algorithms/linear_algebra/finite_field_gaussian_elimination.hpp"
#include "algorithms/number_theory/modular.hpp"
#include "test_framework.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <set>
#include <stdexcept>
#include <vector>

namespace {
using algorithms::linear_algebra::FiniteFieldLinearSystemResult;
using algorithms::linear_algebra::solve_linear_system_mod_prime;

bool satisfies_full_width(const std::vector<std::vector<std::uint64_t>>& a,
                          const std::vector<std::uint64_t>& b,
                          const std::vector<std::uint64_t>& x,
                          std::uint64_t modulus) {
  for (std::size_t row = 0; row < a.size(); ++row) {
    std::uint64_t sum = 0U;
    for (std::size_t column = 0; column < x.size(); ++column) {
      const std::uint64_t term = algorithms::number_theory::multiply_mod(
          a[row][column] % modulus, x[column] % modulus, modulus);
      sum = sum >= modulus - term ? sum - (modulus - term) : sum + term;
    }
    if (sum != b[row] % modulus) return false;
  }
  return true;
}

bool satisfies_small(const std::vector<std::vector<std::uint64_t>>& a,
                     const std::vector<std::uint64_t>& b,
                     const std::vector<std::uint64_t>& x,
                     std::uint64_t modulus) {
  for (std::size_t row = 0; row < a.size(); ++row) {
    std::uint64_t sum = 0U;
    for (std::size_t column = 0; column < x.size(); ++column) {
      sum = (sum + (a[row][column] % modulus) * x[column]) % modulus;
    }
    if (sum != b[row] % modulus) return false;
  }
  return true;
}

std::vector<std::vector<std::uint64_t>> enumerate_assignments(
    std::size_t variable_count, std::uint64_t modulus) {
  std::size_t total = 1U;
  for (std::size_t i = 0; i < variable_count; ++i) {
    total *= static_cast<std::size_t>(modulus);
  }
  std::vector<std::vector<std::uint64_t>> assignments;
  assignments.reserve(total);
  for (std::size_t code = 0; code < total; ++code) {
    std::size_t state = code;
    std::vector<std::uint64_t> assignment(variable_count, 0U);
    for (std::size_t column = 0; column < variable_count; ++column) {
      assignment[column] =
          static_cast<std::uint64_t>(state % static_cast<std::size_t>(modulus));
      state /= static_cast<std::size_t>(modulus);
    }
    assignments.push_back(std::move(assignment));
  }
  return assignments;
}

std::vector<std::vector<std::uint64_t>> exhaustive_solutions(
    std::size_t variable_count,
    const std::vector<std::vector<std::uint64_t>>& a,
    const std::vector<std::uint64_t>& b, std::uint64_t modulus) {
  std::vector<std::vector<std::uint64_t>> solutions;
  for (const auto& assignment : enumerate_assignments(variable_count, modulus)) {
    if (satisfies_small(a, b, assignment, modulus)) {
      solutions.push_back(assignment);
    }
  }
  std::sort(solutions.begin(), solutions.end());
  return solutions;
}

std::vector<std::vector<std::uint64_t>> affine_solutions(
    const FiniteFieldLinearSystemResult& result, std::uint64_t modulus) {
  if (!result.consistent) return {};
  const std::size_t dimension = result.nullspace_basis.size();
  std::vector<std::vector<std::uint64_t>> solutions;
  for (const auto& coefficients : enumerate_assignments(dimension, modulus)) {
    std::vector<std::uint64_t> value = result.particular_solution;
    for (std::size_t basis = 0; basis < dimension; ++basis) {
      for (std::size_t column = 0; column < value.size(); ++column) {
        value[column] =
            (value[column] + coefficients[basis] *
                                 result.nullspace_basis[basis][column]) %
            modulus;
      }
    }
    solutions.push_back(std::move(value));
  }
  std::sort(solutions.begin(), solutions.end());
  solutions.erase(std::unique(solutions.begin(), solutions.end()), solutions.end());
  return solutions;
}

std::size_t row_span_rank(const std::vector<std::vector<std::uint64_t>>& a,
                          std::size_t variable_count, std::uint64_t modulus) {
  const std::size_t row_count = a.size();
  std::set<std::vector<std::uint64_t>> span;
  for (const auto& coefficients : enumerate_assignments(row_count, modulus)) {
    std::vector<std::uint64_t> value(variable_count, 0U);
    for (std::size_t row = 0; row < row_count; ++row) {
      for (std::size_t column = 0; column < variable_count; ++column) {
        value[column] =
            (value[column] + coefficients[row] * (a[row][column] % modulus)) %
            modulus;
      }
    }
    span.insert(std::move(value));
  }
  std::size_t rank = 0;
  std::size_t cardinality = 1U;
  while (cardinality < span.size()) {
    cardinality *= static_cast<std::size_t>(modulus);
    ++rank;
  }
  REQUIRE_EQ(cardinality, span.size());
  return rank;
}

void verify_rref(const FiniteFieldLinearSystemResult& result,
                 std::size_t variable_count, std::uint64_t modulus) {
  REQUIRE_EQ(result.pivot_columns.size(), result.rank);
  REQUIRE(std::is_sorted(result.pivot_columns.begin(), result.pivot_columns.end()));
  REQUIRE_EQ(result.reduced_coefficients.size(), result.reduced_rhs.size());
  for (const auto& row : result.reduced_coefficients) {
    REQUIRE_EQ(row.size(), variable_count);
    for (const std::uint64_t value : row) REQUIRE(value < modulus);
  }
  for (const std::uint64_t value : result.reduced_rhs) REQUIRE(value < modulus);
  for (std::size_t pivot_row = 0; pivot_row < result.rank; ++pivot_row) {
    const std::size_t pivot_column = result.pivot_columns[pivot_row];
    REQUIRE_EQ(result.reduced_coefficients[pivot_row][pivot_column], 1U);
    for (std::size_t row = 0; row < result.reduced_coefficients.size(); ++row) {
      if (row != pivot_row) {
        REQUIRE_EQ(result.reduced_coefficients[row][pivot_column], 0U);
      }
    }
  }
}
}  // namespace

TEST_CASE(finite_field_gaussian_validates_shape_modulus_and_zero_equations) {
  REQUIRE_THROWS_AS(solve_linear_system_mod_prime(1U, {{1U}}, {1U}, 1U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(solve_linear_system_mod_prime(1U, {{1U}}, {1U}, 4U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(solve_linear_system_mod_prime(2U, {{1U}}, {1U}, 5U),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(solve_linear_system_mod_prime(1U, {{1U}}, {}, 5U),
                    std::invalid_argument);

  const auto empty = solve_linear_system_mod_prime(3U, {}, {}, 5U);
  REQUIRE(empty.consistent);
  REQUIRE_EQ(empty.rank, 0U);
  REQUIRE_EQ(empty.particular_solution,
             (std::vector<std::uint64_t>{0U, 0U, 0U}));
  REQUIRE_EQ(empty.nullspace_basis.size(), 3U);
  REQUIRE_EQ(empty.nullspace_basis[0],
             (std::vector<std::uint64_t>{1U, 0U, 0U}));
  REQUIRE_EQ(empty.nullspace_basis[1],
             (std::vector<std::uint64_t>{0U, 1U, 0U}));
  REQUIRE_EQ(empty.nullspace_basis[2],
             (std::vector<std::uint64_t>{0U, 0U, 1U}));

  const auto zero_variable_consistent =
      solve_linear_system_mod_prime(0U, {{}}, {0U}, 3U);
  REQUIRE(zero_variable_consistent.consistent);
  const auto zero_variable_inconsistent =
      solve_linear_system_mod_prime(0U, {{}}, {1U}, 3U);
  REQUIRE(!zero_variable_inconsistent.consistent);
}

TEST_CASE(finite_field_gaussian_unique_solution_normalizes_input) {
  const std::vector<std::vector<std::uint64_t>> a{{9U, 8U}, {8U, 10U}};
  const std::vector<std::uint64_t> b{8U, 9U};
  const auto result = solve_linear_system_mod_prime(2U, a, b, 7U);
  REQUIRE(result.consistent);
  REQUIRE_EQ(result.rank, 2U);
  REQUIRE_EQ(result.pivot_columns, (std::vector<std::size_t>{0U, 1U}));
  REQUIRE_EQ(result.particular_solution,
             (std::vector<std::uint64_t>{3U, 2U}));
  REQUIRE(result.nullspace_basis.empty());
  verify_rref(result, 2U, 7U);
  REQUIRE(satisfies_full_width(a, b, result.particular_solution, 7U));
}

TEST_CASE(finite_field_gaussian_reconstructs_affine_solution_space) {
  const std::vector<std::vector<std::uint64_t>> a{{1U, 1U, 1U},
                                                  {2U, 2U, 2U}};
  const std::vector<std::uint64_t> b{1U, 2U};
  const auto result = solve_linear_system_mod_prime(3U, a, b, 5U);
  REQUIRE(result.consistent);
  REQUIRE_EQ(result.rank, 1U);
  REQUIRE_EQ(result.pivot_columns, (std::vector<std::size_t>{0U}));
  REQUIRE_EQ(result.particular_solution,
             (std::vector<std::uint64_t>{1U, 0U, 0U}));
  REQUIRE_EQ(result.nullspace_basis,
             (std::vector<std::vector<std::uint64_t>>{{4U, 1U, 0U},
                                                       {4U, 0U, 1U}}));
  REQUIRE_EQ(affine_solutions(result, 5U), exhaustive_solutions(3U, a, b, 5U));
  verify_rref(result, 3U, 5U);
}

TEST_CASE(finite_field_gaussian_detects_inconsistency_without_fake_solution) {
  const std::vector<std::vector<std::uint64_t>> a{{1U, 1U}, {2U, 2U}};
  const std::vector<std::uint64_t> b{0U, 1U};
  const auto result = solve_linear_system_mod_prime(2U, a, b, 3U);
  REQUIRE(!result.consistent);
  REQUIRE_EQ(result.rank, 1U);
  REQUIRE(result.particular_solution.empty());
  REQUIRE(result.nullspace_basis.empty());
  verify_rref(result, 2U, 3U);
}

TEST_CASE(finite_field_gaussian_supports_full_uint64_prime_field) {
  constexpr std::uint64_t prime = 18446744073709551557ULL;
  REQUIRE(algorithms::number_theory::is_prime(prime));

  const std::vector<std::vector<std::uint64_t>> a{{1U, 1U}, {1U, 2U}};
  const std::vector<std::uint64_t> b{prime - 3U, prime - 5U};
  const auto result = solve_linear_system_mod_prime(2U, a, b, prime);
  REQUIRE(result.consistent);
  REQUIRE_EQ(result.particular_solution,
             (std::vector<std::uint64_t>{prime - 1U, prime - 2U}));
  REQUIRE(satisfies_full_width(a, b, result.particular_solution, prime));
  verify_rref(result, 2U, prime);

  const auto inverse_two = solve_linear_system_mod_prime(
      1U, {{2U}}, {1U}, prime);
  REQUIRE_EQ(inverse_two.particular_solution[0], (prime + 1U) / 2U);
}

TEST_CASE(finite_field_gaussian_randomized_exhaustive_differential) {
  std::mt19937_64 rng(0x6A757373ULL);
  constexpr std::array<std::uint64_t, 3> primes{2U, 3U, 5U};
  std::uniform_int_distribution<std::size_t> dimension_dist(0U, 4U);
  std::uniform_int_distribution<std::size_t> prime_dist(0U, primes.size() - 1U);
  std::uniform_int_distribution<std::uint64_t> raw_value_dist(0U, 30U);

  for (std::size_t trial = 0; trial < 300U; ++trial) {
    const std::size_t variable_count = dimension_dist(rng);
    const std::size_t row_count = dimension_dist(rng);
    const std::uint64_t modulus = primes[prime_dist(rng)];
    std::vector<std::vector<std::uint64_t>> a(
        row_count, std::vector<std::uint64_t>(variable_count, 0U));
    std::vector<std::uint64_t> b(row_count, 0U);
    for (std::size_t row = 0; row < row_count; ++row) {
      for (std::size_t column = 0; column < variable_count; ++column) {
        a[row][column] = raw_value_dist(rng);
      }
      b[row] = raw_value_dist(rng);
    }

    const auto result =
        solve_linear_system_mod_prime(variable_count, a, b, modulus);
    const auto expected = exhaustive_solutions(variable_count, a, b, modulus);
    REQUIRE_EQ(result.consistent, !expected.empty());
    REQUIRE_EQ(result.rank, row_span_rank(a, variable_count, modulus));
    verify_rref(result, variable_count, modulus);

    if (!result.consistent) {
      REQUIRE(result.particular_solution.empty());
      REQUIRE(result.nullspace_basis.empty());
      continue;
    }

    REQUIRE_EQ(result.particular_solution.size(), variable_count);
    REQUIRE_EQ(result.nullspace_basis.size(), variable_count - result.rank);
    REQUIRE(satisfies_small(a, b, result.particular_solution, modulus));
    const std::vector<std::uint64_t> zero_rhs(row_count, 0U);
    for (const auto& basis : result.nullspace_basis) {
      REQUIRE(satisfies_small(a, zero_rhs, basis, modulus));
    }
    REQUIRE_EQ(affine_solutions(result, modulus), expected);
  }
}
