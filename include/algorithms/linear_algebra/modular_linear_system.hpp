#pragma once

#include "algorithms/linear_algebra/smith_normal_form.hpp"
#include "algorithms/number_theory/modular.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>

namespace algorithms::linear_algebra {

struct ModularLinearSystemResult {
  std::vector<std::uint64_t> solution;
  std::vector<std::uint64_t> smith_coordinates;
  std::vector<std::uint64_t> transformed_rhs;
  std::vector<std::int64_t> invariant_factors;
  std::vector<std::uint64_t> diagonal_gcds;
  std::size_t rank = 0;

  friend bool operator==(const ModularLinearSystemResult&,
                         const ModularLinearSystemResult&) = default;
};

namespace modular_linear_detail {

[[nodiscard]] inline std::uint64_t magnitude(std::int64_t value) noexcept {
  if (value >= 0) return static_cast<std::uint64_t>(value);
  return static_cast<std::uint64_t>(-(value + 1)) + 1U;
}

[[nodiscard]] inline std::uint64_t residue(std::int64_t value,
                                           std::uint64_t modulus) noexcept {
  const auto remainder = magnitude(value) % modulus;
  if (value >= 0 || remainder == 0) return remainder;
  return modulus - remainder;
}

[[nodiscard]] inline std::uint64_t add_mod(std::uint64_t first,
                                           std::uint64_t second,
                                           std::uint64_t modulus) noexcept {
  return first >= modulus - second ? first - (modulus - second)
                                   : first + second;
}

[[nodiscard]] inline std::uint64_t subtract_mod(std::uint64_t first,
                                                std::uint64_t second,
                                                std::uint64_t modulus) noexcept {
  return first >= second ? first - second : modulus - (second - first);
}

[[nodiscard]] inline std::uint64_t inverse_coprime(std::uint64_t value,
                                                   std::uint64_t modulus) {
  if (modulus <= 1 || value == 0 || value >= modulus) {
    throw std::logic_error("modular linear-system inverse precondition failure");
  }

  std::uint64_t old_remainder = modulus;
  std::uint64_t remainder = value;
  std::uint64_t old_coefficient = 0;
  std::uint64_t coefficient = 1;
  while (remainder != 0) {
    const auto quotient = old_remainder / remainder;
    const auto next_remainder = old_remainder % remainder;
    const auto product = algorithms::number_theory::multiply_mod(
        quotient % modulus, coefficient, modulus);
    const auto next_coefficient =
        subtract_mod(old_coefficient, product, modulus);
    old_remainder = remainder;
    remainder = next_remainder;
    old_coefficient = coefficient;
    coefficient = next_coefficient;
  }
  if (old_remainder != 1) {
    throw std::logic_error("modular linear-system inverse does not exist");
  }
  return old_coefficient;
}

[[nodiscard]] inline std::vector<std::uint64_t> multiply_signed_vector_mod(
    const SmithMatrix& matrix, const std::vector<std::int64_t>& vector,
    std::uint64_t modulus) {
  std::vector<std::uint64_t> result(matrix.size(), 0);
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    if (matrix[row].size() != vector.size()) {
      throw std::logic_error("modular linear-system transform shape mismatch");
    }
    std::uint64_t sum = 0;
    for (std::size_t column = 0; column < vector.size(); ++column) {
      const auto term = algorithms::number_theory::multiply_mod(
          residue(matrix[row][column], modulus),
          residue(vector[column], modulus), modulus);
      sum = add_mod(sum, term, modulus);
    }
    result[row] = sum;
  }
  return result;
}

[[nodiscard]] inline std::vector<std::uint64_t> multiply_unsigned_vector_mod(
    const SmithMatrix& matrix, const std::vector<std::uint64_t>& vector,
    std::uint64_t modulus) {
  std::vector<std::uint64_t> result(matrix.size(), 0);
  for (std::size_t row = 0; row < matrix.size(); ++row) {
    if (matrix[row].size() != vector.size()) {
      throw std::logic_error("modular linear-system transform shape mismatch");
    }
    std::uint64_t sum = 0;
    for (std::size_t column = 0; column < vector.size(); ++column) {
      const auto term = algorithms::number_theory::multiply_mod(
          residue(matrix[row][column], modulus), vector[column], modulus);
      sum = add_mod(sum, term, modulus);
    }
    result[row] = sum;
  }
  return result;
}

}  // namespace modular_linear_detail

// Solves A*x == b (mod modulus) through the bounded-exact Smith decomposition
// U*A*V = D. A successful result chooses the least non-negative solution for
// every constrained Smith coordinate and zero for every free Smith coordinate,
// then maps back with V. The public solution is canonical for this deterministic
// Smith decomposition, not claimed to be lexicographically minimum among all
// solutions.
//
// This surface deliberately inherits smith_normal_form's bounded-int64
// representability contract: it can throw std::overflow_error while constructing
// U/D/V even when the modular system itself has a mathematical solution.
[[nodiscard]] inline std::optional<ModularLinearSystemResult>
solve_modular_linear_system(const SmithMatrix& coefficients,
                            const std::vector<std::int64_t>& rhs,
                            std::uint64_t modulus) {
  if (modulus == 0) {
    throw std::invalid_argument("modular linear-system modulus must be positive");
  }
  if (rhs.size() != coefficients.size()) {
    throw std::invalid_argument("modular linear-system rhs size mismatch");
  }

  const auto smith = smith_normal_form(coefficients);
  const std::size_t columns = smith.right_transform.size();

  ModularLinearSystemResult result;
  result.transformed_rhs = modular_linear_detail::multiply_signed_vector_mod(
      smith.left_transform, rhs, modulus);
  result.invariant_factors = smith.invariant_factors;
  result.rank = smith.rank;
  result.diagonal_gcds.reserve(smith.rank);
  result.smith_coordinates.assign(columns, 0);

  for (std::size_t index = 0; index < smith.rank; ++index) {
    const auto diagonal = static_cast<std::uint64_t>(
        smith.invariant_factors[index]);
    const auto common = algorithms::number_theory::gcd(diagonal, modulus);
    result.diagonal_gcds.push_back(common);
    const auto transformed = result.transformed_rhs[index];
    if (transformed % common != 0) return std::nullopt;

    const auto reduced_modulus = modulus / common;
    if (reduced_modulus == 1) {
      result.smith_coordinates[index] = 0;
      continue;
    }
    const auto reduced_diagonal = (diagonal / common) % reduced_modulus;
    const auto reduced_rhs = (transformed / common) % reduced_modulus;
    const auto inverse = modular_linear_detail::inverse_coprime(
        reduced_diagonal, reduced_modulus);
    result.smith_coordinates[index] =
        algorithms::number_theory::multiply_mod(inverse, reduced_rhs,
                                                reduced_modulus);
  }

  for (std::size_t row = smith.rank; row < result.transformed_rhs.size(); ++row) {
    if (result.transformed_rhs[row] != 0) return std::nullopt;
  }

  result.solution = modular_linear_detail::multiply_unsigned_vector_mod(
      smith.right_transform, result.smith_coordinates, modulus);
  return result;
}

}  // namespace algorithms::linear_algebra
