#pragma once

#include "algorithms/number_theory/modular.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::linear_algebra {
namespace pfaffian_detail {

[[nodiscard]] inline std::uint64_t add_mod(std::uint64_t first,
                                           std::uint64_t second,
                                           std::uint64_t modulus) noexcept {
  if (first >= modulus - second) {
    return first - (modulus - second);
  }
  return first + second;
}

[[nodiscard]] inline std::uint64_t subtract_mod(std::uint64_t first,
                                                std::uint64_t second,
                                                std::uint64_t modulus) noexcept {
  if (first >= second) {
    return first - second;
  }
  return modulus - (second - first);
}

[[nodiscard]] inline std::uint64_t negate_mod(std::uint64_t value,
                                              std::uint64_t modulus) noexcept {
  return value == 0U ? 0U : modulus - value;
}

}  // namespace pfaffian_detail

// Computes the Pfaffian of a skew-symmetric matrix over the prime field F_p.
// Matrix entries are interpreted modulo modulus. For odd dimension the
// conventional Pfaffian value is zero. Empty-matrix Pfaffian is one.
[[nodiscard]] inline std::uint64_t pfaffian_mod_prime(
    const std::vector<std::vector<std::uint64_t>>& matrix,
    std::uint64_t modulus) {
  using algorithms::number_theory::is_prime;
  using algorithms::number_theory::multiply_mod;
  using algorithms::number_theory::power_mod;
  using pfaffian_detail::add_mod;
  using pfaffian_detail::negate_mod;
  using pfaffian_detail::subtract_mod;

  if (!is_prime(modulus)) {
    throw std::invalid_argument("Pfaffian modulus must be prime");
  }

  const std::size_t dimension = matrix.size();
  std::vector<std::vector<std::uint64_t>> work(
      dimension, std::vector<std::uint64_t>(dimension, 0U));
  for (std::size_t row = 0; row < dimension; ++row) {
    if (matrix[row].size() != dimension) {
      throw std::invalid_argument("Pfaffian matrix must be square");
    }
    for (std::size_t column = 0; column < dimension; ++column) {
      work[row][column] = matrix[row][column] % modulus;
    }
  }

  for (std::size_t row = 0; row < dimension; ++row) {
    if (work[row][row] != 0U) {
      throw std::invalid_argument("Pfaffian matrix diagonal must be zero");
    }
    for (std::size_t column = row + 1U; column < dimension; ++column) {
      if (work[column][row] != negate_mod(work[row][column], modulus)) {
        throw std::invalid_argument("Pfaffian matrix must be skew-symmetric");
      }
    }
  }

  if ((dimension & 1U) != 0U) {
    return 0U;
  }

  std::uint64_t result = 1U;
  for (std::size_t pivot_index = 0; pivot_index < dimension;
       pivot_index += 2U) {
    std::size_t partner = pivot_index + 1U;
    while (partner < dimension && work[pivot_index][partner] == 0U) {
      ++partner;
    }
    if (partner == dimension) {
      return 0U;
    }

    if (partner != pivot_index + 1U) {
      std::swap(work[partner], work[pivot_index + 1U]);
      for (std::size_t row = 0; row < dimension; ++row) {
        std::swap(work[row][partner], work[row][pivot_index + 1U]);
      }
      result = negate_mod(result, modulus);
    }

    const std::uint64_t pivot = work[pivot_index][pivot_index + 1U];
    result = multiply_mod(result, pivot, modulus);
    const std::uint64_t inverse = power_mod(pivot, modulus - 2U, modulus);

    for (std::size_t row = pivot_index + 2U; row < dimension; ++row) {
      for (std::size_t column = row + 1U; column < dimension; ++column) {
        const std::uint64_t left = multiply_mod(
            work[pivot_index + 1U][row], work[pivot_index][column], modulus);
        const std::uint64_t right = multiply_mod(
            work[pivot_index][row], work[pivot_index + 1U][column], modulus);
        const std::uint64_t correction = multiply_mod(
            subtract_mod(left, right, modulus), inverse, modulus);
        const std::uint64_t value =
            add_mod(work[row][column], correction, modulus);
        work[row][column] = value;
        work[column][row] = negate_mod(value, modulus);
      }
    }
  }
  return result;
}

}  // namespace algorithms::linear_algebra
