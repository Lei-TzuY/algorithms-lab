#pragma once

#include "algorithms/number_theory/modular.hpp"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace algorithms::randomized {

using ModularMatrix = std::vector<std::vector<std::uint64_t>>;

struct FreivaldsVerificationResult {
  bool accepted{false};
  std::size_t trials_requested{0U};
  std::size_t trials_executed{0U};
  std::optional<std::size_t> rejecting_trial;
  std::uint64_t seed{0U};
  friend bool operator==(const FreivaldsVerificationResult&,
                         const FreivaldsVerificationResult&) = default;
};

namespace detail {

inline void validate_rectangular_nonempty(const ModularMatrix& matrix,
                                          const char* name) {
  if (matrix.empty() || matrix.front().empty()) {
    throw std::invalid_argument(std::string(name) + " must be non-empty");
  }
  const std::size_t columns = matrix.front().size();
  for (const auto& row : matrix) {
    if (row.size() != columns) {
      throw std::invalid_argument(std::string(name) + " must be rectangular");
    }
  }
}

inline void validate_residues(const ModularMatrix& matrix,
                              const std::uint64_t modulus,
                              const char* name) {
  for (const auto& row : matrix) {
    for (const std::uint64_t value : row) {
      if (value >= modulus) {
        throw std::invalid_argument(std::string(name) +
                                    " contains a non-canonical residue");
      }
    }
  }
}

[[nodiscard]] inline std::uint64_t add_mod(const std::uint64_t left,
                                           const std::uint64_t right,
                                           const std::uint64_t modulus) noexcept {
  if (left >= modulus - right) {
    return left - (modulus - right);
  }
  return left + right;
}

[[nodiscard]] inline std::uint64_t sample_field_element(
    std::mt19937_64& random, const std::uint64_t modulus) {
  const std::uint64_t threshold = (std::uint64_t{0} - modulus) % modulus;
  for (;;) {
    const std::uint64_t sample = random();
    if (sample >= threshold) {
      return sample % modulus;
    }
  }
}

[[nodiscard]] inline std::vector<std::uint64_t> matrix_vector_product(
    const ModularMatrix& matrix, const std::vector<std::uint64_t>& vector,
    const std::uint64_t modulus) {
  std::vector<std::uint64_t> result(matrix.size(), 0U);
  for (std::size_t row = 0U; row < matrix.size(); ++row) {
    std::uint64_t sum = 0U;
    for (std::size_t column = 0U; column < vector.size(); ++column) {
      const std::uint64_t term = algorithms::number_theory::multiply_mod(
          matrix[row][column], vector[column], modulus);
      sum = add_mod(sum, term, modulus);
    }
    result[row] = sum;
  }
  return result;
}

}  // namespace detail

// Replayable Freivalds verification over the prime field F_modulus.
//
// `accepted == false` is a deterministic certificate that A*B != C. An
// accepted result only means that every requested random-vector check passed;
// finite randomized verification is deliberately not an exact equality proof.
[[nodiscard]] inline FreivaldsVerificationResult
freivalds_randomized_verify_matrix_product(
    const ModularMatrix& first, const ModularMatrix& second,
    const ModularMatrix& claimed_product, const std::uint64_t modulus,
    const std::uint64_t seed, const std::size_t trials) {
  if (trials == 0U) {
    throw std::invalid_argument("Freivalds requires at least one trial");
  }
  if (!algorithms::number_theory::is_prime(modulus)) {
    throw std::invalid_argument("Freivalds modulus must be prime");
  }
  detail::validate_rectangular_nonempty(first, "first matrix");
  detail::validate_rectangular_nonempty(second, "second matrix");
  detail::validate_rectangular_nonempty(claimed_product, "claimed product");

  const std::size_t rows = first.size();
  const std::size_t shared = first.front().size();
  const std::size_t columns = second.front().size();
  if (second.size() != shared || claimed_product.size() != rows ||
      claimed_product.front().size() != columns) {
    throw std::invalid_argument("Freivalds matrix shapes are incompatible");
  }
  detail::validate_residues(first, modulus, "first matrix");
  detail::validate_residues(second, modulus, "second matrix");
  detail::validate_residues(claimed_product, modulus, "claimed product");

  std::mt19937_64 random(seed);
  for (std::size_t trial = 0U; trial < trials; ++trial) {
    std::vector<std::uint64_t> vector(columns);
    for (std::uint64_t& value : vector) {
      value = detail::sample_field_element(random, modulus);
    }
    const auto second_times_vector =
        detail::matrix_vector_product(second, vector, modulus);
    const auto left =
        detail::matrix_vector_product(first, second_times_vector, modulus);
    const auto right = detail::matrix_vector_product(claimed_product, vector,
                                                     modulus);
    if (left != right) {
      return FreivaldsVerificationResult{false, trials, trial + 1U, trial, seed};
    }
  }
  return FreivaldsVerificationResult{true, trials, trials, std::nullopt, seed};
}

}  // namespace algorithms::randomized
