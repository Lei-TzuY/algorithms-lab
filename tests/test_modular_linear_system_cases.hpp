#pragma once

#include "algorithms/linear_algebra/modular_linear_system.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <random>
#include <stdexcept>
#include <vector>

namespace modular_linear_system_test_detail {

using algorithms::linear_algebra::ModularLinearSystemResult;
using algorithms::linear_algebra::SmithMatrix;

std::uint64_t magnitude(std::int64_t value) noexcept {
  if (value >= 0) return static_cast<std::uint64_t>(value);
  return static_cast<std::uint64_t>(-(value + 1)) + 1U;
}

std::uint64_t residue(std::int64_t value, std::uint64_t modulus) {
  const auto remainder = magnitude(value) % modulus;
  if (value >= 0 || remainder == 0) return remainder;
  return modulus - remainder;
}

std::uint64_t add_mod(std::uint64_t first, std::uint64_t second,
                      std::uint64_t modulus) {
  return first >= modulus - second ? first - (modulus - second)
                                   : first + second;
}

bool satisfies(const SmithMatrix& matrix, const std::vector<std::int64_t>& rhs,
               std::uint64_t modulus,
               const std::vector<std::uint64_t>& solution) {
  if (matrix.size() != rhs.size()) return false;
  const std::size_t columns = matrix.empty() ? 0 : matrix.front().size();
  if (solution.size() != columns) return false;
  for (const auto& row : matrix) {
    if (row.size() != columns) return false;
  }
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    std::uint64_t total = 0;
    for (std::size_t column = 0; column < columns; ++column) {
      const auto term = algorithms::number_theory::multiply_mod(
          residue(matrix[row][column], modulus), solution[column], modulus);
      total = add_mod(total, term, modulus);
    }
    if (total != residue(rhs[row], modulus)) return false;
  }
  return true;
}

bool satisfies_small_direct(const SmithMatrix& matrix,
                            const std::vector<std::int64_t>& rhs,
                            std::uint64_t modulus,
                            const std::vector<std::uint64_t>& solution) {
  if (matrix.size() != rhs.size()) return false;
  const std::size_t columns = matrix.empty() ? 0 : matrix.front().size();
  if (solution.size() != columns) return false;
  const auto signed_modulus = static_cast<std::int64_t>(modulus);
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    std::int64_t total = 0;
    for (std::size_t column = 0; column < columns; ++column) {
      total += matrix[row][column] *
               static_cast<std::int64_t>(solution[column]);
    }
    auto left = total % signed_modulus;
    if (left < 0) left += signed_modulus;
    auto right = rhs[row] % signed_modulus;
    if (right < 0) right += signed_modulus;
    if (left != right) return false;
  }
  return true;
}

bool exhaustive_has_solution(const SmithMatrix& matrix,
                             const std::vector<std::int64_t>& rhs,
                             std::uint64_t modulus) {
  const std::size_t columns = matrix.empty() ? 0 : matrix.front().size();
  std::vector<std::uint64_t> candidate(columns, 0);
  for (;;) {
    if (satisfies_small_direct(matrix, rhs, modulus, candidate)) return true;
    std::size_t position = 0;
    while (position < columns) {
      ++candidate[position];
      if (candidate[position] < modulus) break;
      candidate[position] = 0;
      ++position;
    }
    if (position == columns) return false;
  }
}

void verify_certificate(const SmithMatrix& matrix,
                        const std::vector<std::int64_t>& rhs,
                        std::uint64_t modulus,
                        const ModularLinearSystemResult& result) {
  REQUIRE(satisfies(matrix, rhs, modulus, result.solution));
  for (const auto value : result.solution) REQUIRE(value < modulus);
  for (const auto value : result.smith_coordinates) REQUIRE(value < modulus);
  for (const auto value : result.transformed_rhs) REQUIRE(value < modulus);
  REQUIRE_EQ(result.rank, result.invariant_factors.size());
  REQUIRE_EQ(result.rank, result.diagonal_gcds.size());
  REQUIRE_EQ(result.transformed_rhs.size(), matrix.size());

  const auto smith = algorithms::linear_algebra::smith_normal_form(matrix);
  REQUIRE_EQ(result.invariant_factors, smith.invariant_factors);
  REQUIRE_EQ(result.rank, smith.rank);
  REQUIRE_EQ(result.smith_coordinates.size(), smith.right_transform.size());
  for (std::size_t index = 0; index < result.rank; ++index) {
    const auto diagonal = static_cast<std::uint64_t>(result.invariant_factors[index]);
    REQUIRE_EQ(result.diagonal_gcds[index],
               algorithms::number_theory::gcd(diagonal, modulus));
    REQUIRE_EQ(algorithms::number_theory::multiply_mod(
                   diagonal % modulus, result.smith_coordinates[index], modulus),
               result.transformed_rhs[index]);
  }
  for (std::size_t row = result.rank; row < result.transformed_rhs.size(); ++row) {
    REQUIRE_EQ(result.transformed_rhs[row], std::uint64_t{0});
  }
}

}  // namespace modular_linear_system_test_detail

TEST_CASE(modular_linear_system_deterministic_composite_cases) {
  using algorithms::linear_algebra::SmithMatrix;
  using algorithms::linear_algebra::solve_modular_linear_system;
  using modular_linear_system_test_detail::verify_certificate;

  const auto solvable = solve_modular_linear_system(SmithMatrix{{2}}, {2}, 4);
  REQUIRE(solvable.has_value());
  verify_certificate(SmithMatrix{{2}}, {2}, 4, *solvable);
  REQUIRE_EQ(solvable->solution, (std::vector<std::uint64_t>{1}));

  REQUIRE(!solve_modular_linear_system(SmithMatrix{{2}}, {1}, 4).has_value());

  const SmithMatrix coupled{{2, 4}, {6, 8}};
  const std::vector<std::int64_t> coupled_rhs{2, 6};
  const auto coupled_result = solve_modular_linear_system(coupled, coupled_rhs, 12);
  REQUIRE(coupled_result.has_value());
  verify_certificate(coupled, coupled_rhs, 12, *coupled_result);

  const auto underdetermined =
      solve_modular_linear_system(SmithMatrix{{2, 0}}, {2}, 4);
  REQUIRE(underdetermined.has_value());
  verify_certificate(SmithMatrix{{2, 0}}, {2}, 4, *underdetermined);
  REQUIRE_EQ(underdetermined->smith_coordinates[1], std::uint64_t{0});

  REQUIRE(!solve_modular_linear_system(SmithMatrix{{0}}, {2}, 4).has_value());
}

TEST_CASE(modular_linear_system_validation_full_width_and_bounded_smith_contract) {
  using algorithms::linear_algebra::SmithMatrix;
  using algorithms::linear_algebra::solve_modular_linear_system;
  using modular_linear_system_test_detail::verify_certificate;

  REQUIRE_THROWS_AS(solve_modular_linear_system(SmithMatrix{{1}}, {1}, 0),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(solve_modular_linear_system(SmithMatrix{{1}}, {}, 7),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(solve_modular_linear_system(SmithMatrix{{1, 2}, {3}}, {1, 2}, 7),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(
      solve_modular_linear_system(
          SmithMatrix{{std::numeric_limits<std::int64_t>::min()}}, {0}, 7),
      std::overflow_error);

  const auto empty = solve_modular_linear_system({}, {}, 13);
  REQUIRE(empty.has_value());
  REQUIRE(empty->solution.empty());

  const auto zero_variables =
      solve_modular_linear_system(SmithMatrix{{}, {}}, {0, 0}, 9);
  REQUIRE(zero_variables.has_value());
  REQUIRE(zero_variables->solution.empty());
  REQUIRE(!solve_modular_linear_system(SmithMatrix{{}, {}}, {0, 1}, 9).has_value());

  const auto modulus_one =
      solve_modular_linear_system(SmithMatrix{{7, -3}}, {5}, 1);
  REQUIRE(modulus_one.has_value());
  REQUIRE_EQ(modulus_one->solution,
             (std::vector<std::uint64_t>{0, 0}));

  constexpr std::uint64_t kFull = std::numeric_limits<std::uint64_t>::max();
  const auto full_width = solve_modular_linear_system(SmithMatrix{{2}}, {1}, kFull);
  REQUIRE(full_width.has_value());
  REQUIRE_EQ(full_width->solution[0], std::uint64_t{1} << 63U);
  verify_certificate(SmithMatrix{{2}}, {1}, kFull, *full_width);

  const auto negative_rhs =
      solve_modular_linear_system(SmithMatrix{{1}}, {-1}, kFull);
  REQUIRE(negative_rhs.has_value());
  REQUIRE_EQ(negative_rhs->solution[0], kFull - 1U);
  verify_certificate(SmithMatrix{{1}}, {-1}, kFull, *negative_rhs);
}

TEST_CASE(modular_linear_system_replay_is_deterministic) {
  using algorithms::linear_algebra::SmithMatrix;
  using algorithms::linear_algebra::solve_modular_linear_system;
  const SmithMatrix matrix{{4, -2, 6}, {2, 8, 4}};
  const std::vector<std::int64_t> rhs{2, 10};
  const auto first = solve_modular_linear_system(matrix, rhs, 18);
  const auto second = solve_modular_linear_system(matrix, rhs, 18);
  REQUIRE_EQ(first, second);
  if (first.has_value()) {
    modular_linear_system_test_detail::verify_certificate(matrix, rhs, 18, *first);
  }
}

TEST_CASE(modular_linear_system_randomized_exhaustive_composite_differential) {
  using algorithms::linear_algebra::SmithMatrix;
  using algorithms::linear_algebra::solve_modular_linear_system;
  using modular_linear_system_test_detail::exhaustive_has_solution;
  using modular_linear_system_test_detail::verify_certificate;

  std::mt19937_64 rng(0x434F4E475255454EULL);
  std::uniform_int_distribution<int> dimension(1, 3);
  std::uniform_int_distribution<int> entry(-5, 5);
  std::uniform_int_distribution<int> modulus_distribution(1, 7);
  for (std::size_t trial = 0; trial < 700; ++trial) {
    const auto rows = static_cast<std::size_t>(dimension(rng));
    const auto columns = static_cast<std::size_t>(dimension(rng));
    SmithMatrix matrix(rows, std::vector<std::int64_t>(columns));
    for (auto& row : matrix) {
      for (auto& value : row) value = entry(rng);
    }
    std::vector<std::int64_t> rhs(rows);
    for (auto& value : rhs) value = entry(rng);
    const auto modulus = static_cast<std::uint64_t>(modulus_distribution(rng));

    const bool expected = exhaustive_has_solution(matrix, rhs, modulus);
    const auto actual = solve_modular_linear_system(matrix, rhs, modulus);
    REQUIRE_EQ(actual.has_value(), expected);
    if (actual.has_value()) {
      verify_certificate(matrix, rhs, modulus, *actual);
      REQUIRE_EQ(actual, solve_modular_linear_system(matrix, rhs, modulus));
    }
  }
}
