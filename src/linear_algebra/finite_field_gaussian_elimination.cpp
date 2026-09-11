#include "algorithms/linear_algebra/finite_field_gaussian_elimination.hpp"

#include "algorithms/number_theory/modular.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::linear_algebra {
namespace {

std::uint64_t subtract_mod(std::uint64_t a, std::uint64_t b,
                           std::uint64_t modulus) noexcept {
  return a >= b ? a - b : modulus - (b - a);
}

std::uint64_t negate_mod(std::uint64_t value,
                         std::uint64_t modulus) noexcept {
  return value == 0U ? 0U : modulus - value;
}

void validate_input(
    std::size_t variable_count,
    const std::vector<std::vector<std::uint64_t>>& coefficients,
    const std::vector<std::uint64_t>& rhs, std::uint64_t modulus) {
  if (!number_theory::is_prime(modulus)) {
    throw std::invalid_argument("modulus must be prime");
  }
  if (coefficients.size() != rhs.size()) {
    throw std::invalid_argument("coefficient row count must match rhs size");
  }
  for (const auto& row : coefficients) {
    if (row.size() != variable_count) {
      throw std::invalid_argument("coefficient row has wrong variable count");
    }
  }
}

}  // namespace

FiniteFieldLinearSystemResult solve_linear_system_mod_prime(
    std::size_t variable_count,
    const std::vector<std::vector<std::uint64_t>>& coefficients,
    const std::vector<std::uint64_t>& rhs, std::uint64_t modulus) {
  validate_input(variable_count, coefficients, rhs, modulus);

  const std::size_t row_count = coefficients.size();
  std::vector<std::vector<std::uint64_t>> reduced = coefficients;
  std::vector<std::uint64_t> reduced_rhs = rhs;
  for (std::size_t row = 0; row < row_count; ++row) {
    for (std::size_t column = 0; column < variable_count; ++column) {
      reduced[row][column] %= modulus;
    }
    reduced_rhs[row] %= modulus;
  }

  std::vector<std::size_t> pivot_columns;
  std::size_t pivot_row = 0;
  for (std::size_t column = 0;
       column < variable_count && pivot_row < row_count; ++column) {
    std::size_t selected = pivot_row;
    while (selected < row_count && reduced[selected][column] == 0U) {
      ++selected;
    }
    if (selected == row_count) {
      continue;
    }

    if (selected != pivot_row) {
      std::swap(reduced[selected], reduced[pivot_row]);
      std::swap(reduced_rhs[selected], reduced_rhs[pivot_row]);
    }

    const std::uint64_t inverse = number_theory::power_mod(
        reduced[pivot_row][column], modulus - 2U, modulus);
    for (std::size_t j = 0; j < variable_count; ++j) {
      reduced[pivot_row][j] = number_theory::multiply_mod(
          reduced[pivot_row][j], inverse, modulus);
    }
    reduced_rhs[pivot_row] = number_theory::multiply_mod(
        reduced_rhs[pivot_row], inverse, modulus);

    for (std::size_t row = 0; row < row_count; ++row) {
      if (row == pivot_row) {
        continue;
      }
      const std::uint64_t factor = reduced[row][column];
      if (factor == 0U) {
        continue;
      }
      for (std::size_t j = 0; j < variable_count; ++j) {
        const std::uint64_t product = number_theory::multiply_mod(
            factor, reduced[pivot_row][j], modulus);
        reduced[row][j] = subtract_mod(reduced[row][j], product, modulus);
      }
      const std::uint64_t rhs_product = number_theory::multiply_mod(
          factor, reduced_rhs[pivot_row], modulus);
      reduced_rhs[row] =
          subtract_mod(reduced_rhs[row], rhs_product, modulus);
    }

    pivot_columns.push_back(column);
    ++pivot_row;
  }

  bool consistent = true;
  for (std::size_t row = 0; row < row_count; ++row) {
    bool all_zero = true;
    for (std::size_t column = 0; column < variable_count; ++column) {
      if (reduced[row][column] != 0U) {
        all_zero = false;
        break;
      }
    }
    if (all_zero && reduced_rhs[row] != 0U) {
      consistent = false;
      break;
    }
  }

  FiniteFieldLinearSystemResult result;
  result.consistent = consistent;
  result.rank = pivot_columns.size();
  result.pivot_columns = pivot_columns;
  result.reduced_coefficients = std::move(reduced);
  result.reduced_rhs = std::move(reduced_rhs);

  if (!consistent) {
    return result;
  }

  result.particular_solution.assign(variable_count, 0U);
  for (std::size_t row = 0; row < pivot_columns.size(); ++row) {
    result.particular_solution[pivot_columns[row]] = result.reduced_rhs[row];
  }

  std::vector<bool> is_pivot(variable_count, false);
  for (const std::size_t column : pivot_columns) {
    is_pivot[column] = true;
  }
  for (std::size_t free_column = 0; free_column < variable_count;
       ++free_column) {
    if (is_pivot[free_column]) {
      continue;
    }
    std::vector<std::uint64_t> basis_vector(variable_count, 0U);
    basis_vector[free_column] = 1U;
    for (std::size_t row = 0; row < pivot_columns.size(); ++row) {
      basis_vector[pivot_columns[row]] = negate_mod(
          result.reduced_coefficients[row][free_column], modulus);
    }
    result.nullspace_basis.push_back(std::move(basis_vector));
  }

  return result;
}

}  // namespace algorithms::linear_algebra
