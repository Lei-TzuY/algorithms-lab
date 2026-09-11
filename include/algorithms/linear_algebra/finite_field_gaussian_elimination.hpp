#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace algorithms::linear_algebra {

struct FiniteFieldLinearSystemResult {
  bool consistent{false};
  std::size_t rank{0};
  std::vector<std::size_t> pivot_columns;
  std::vector<std::vector<std::uint64_t>> reduced_coefficients;
  std::vector<std::uint64_t> reduced_rhs;
  std::vector<std::uint64_t> particular_solution;
  std::vector<std::vector<std::uint64_t>> nullspace_basis;
};

// Solves A*x = b over the prime field F_modulus and returns a deterministic
// reduced-row-echelon witness. `variable_count` is explicit so zero-equation
// systems can still retain a non-zero variable dimension.
//
// Preconditions:
// - modulus is prime;
// - coefficients.size() == rhs.size();
// - every coefficient row has exactly variable_count entries.
//
// Input values are normalized modulo modulus. For consistent systems, the
// particular solution sets every free variable to zero, and nullspace_basis
// contains one canonical basis vector per free column in ascending order.
[[nodiscard]] FiniteFieldLinearSystemResult solve_linear_system_mod_prime(
    std::size_t variable_count,
    const std::vector<std::vector<std::uint64_t>>& coefficients,
    const std::vector<std::uint64_t>& rhs, std::uint64_t modulus);

}  // namespace algorithms::linear_algebra
