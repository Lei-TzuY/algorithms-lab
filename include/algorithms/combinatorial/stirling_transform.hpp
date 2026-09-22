#pragma once

#include "algorithms/number_theory/modular.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace algorithms::combinatorial {

struct StirlingTransformTables {
  // Rows/columns are indexed by n,k. Entries are canonical residues modulo
  // modulus. second_kind[n][k] = S(n,k); signed_first_kind[n][k] = s(n,k).
  std::vector<std::vector<std::uint64_t>> second_kind;
  std::vector<std::vector<std::uint64_t>> signed_first_kind;
};

namespace stirling_transform_detail {

inline void validate_modulus(const std::uint64_t modulus) {
  if (modulus < 2U) {
    throw std::invalid_argument(
        "Stirling transform modulus must be at least two");
  }
}

inline std::uint64_t add_mod(
    const std::uint64_t first, const std::uint64_t second,
    const std::uint64_t modulus) noexcept {
  return first >= modulus - second
             ? first - (modulus - second)
             : first + second;
}

inline std::uint64_t subtract_mod(
    const std::uint64_t first, const std::uint64_t second,
    const std::uint64_t modulus) noexcept {
  return first >= second
             ? first - second
             : modulus - (second - first);
}

inline void validate_residues(
    const std::vector<std::uint64_t>& values,
    const std::uint64_t modulus) {
  for (const std::uint64_t value : values) {
    if (value >= modulus) {
      throw std::invalid_argument(
          "Stirling transform input contains a non-canonical residue");
    }
  }
}

}  // namespace stirling_transform_detail

// Builds Stirling numbers through row max_n modulo an arbitrary modulus >= 2.
//
// The two triangular matrices are exact inverses over the integers and
// therefore remain inverses after reduction modulo any modulus; primality is
// not required.
[[nodiscard]] inline StirlingTransformTables stirling_transform_tables(
    const std::size_t max_n, const std::uint64_t modulus) {
  using algorithms::number_theory::multiply_mod;
  using stirling_transform_detail::add_mod;
  using stirling_transform_detail::subtract_mod;
  using stirling_transform_detail::validate_modulus;

  validate_modulus(modulus);
  if (max_n == std::numeric_limits<std::size_t>::max()) {
    throw std::length_error(
        "Stirling transform table dimension overflows size_t");
  }

  const std::size_t dimension = max_n + 1U;
  StirlingTransformTables tables;
  tables.second_kind.assign(
      dimension, std::vector<std::uint64_t>(dimension, 0U));
  tables.signed_first_kind.assign(
      dimension, std::vector<std::uint64_t>(dimension, 0U));

  tables.second_kind[0U][0U] = 1U % modulus;
  tables.signed_first_kind[0U][0U] = 1U % modulus;

  for (std::size_t n = 1U; n <= max_n; ++n) {
    const std::uint64_t n_minus_one_mod =
        static_cast<std::uint64_t>(n - 1U) % modulus;

    for (std::size_t k = 1U; k <= n; ++k) {
      const std::uint64_t second_scaled =
          multiply_mod(static_cast<std::uint64_t>(k) % modulus,
                       tables.second_kind[n - 1U][k], modulus);
      tables.second_kind[n][k] =
          add_mod(tables.second_kind[n - 1U][k - 1U],
                  second_scaled, modulus);

      const std::uint64_t first_scaled =
          multiply_mod(n_minus_one_mod,
                       tables.signed_first_kind[n - 1U][k],
                       modulus);
      tables.signed_first_kind[n][k] =
          subtract_mod(
              tables.signed_first_kind[n - 1U][k - 1U],
              first_scaled, modulus);
    }
  }

  return tables;
}

// Forward Stirling transform:
//   output[n] = sum_{k=0}^n S(n,k) * input[k] (mod modulus).
[[nodiscard]] inline std::vector<std::uint64_t> stirling_transform(
    const std::vector<std::uint64_t>& input,
    const std::uint64_t modulus) {
  using algorithms::number_theory::multiply_mod;
  using stirling_transform_detail::add_mod;
  using stirling_transform_detail::validate_modulus;
  using stirling_transform_detail::validate_residues;

  validate_modulus(modulus);
  validate_residues(input, modulus);
  if (input.empty()) {
    return {};
  }

  const auto tables =
      stirling_transform_tables(input.size() - 1U, modulus);
  std::vector<std::uint64_t> output(input.size(), 0U);

  for (std::size_t n = 0U; n < input.size(); ++n) {
    std::uint64_t total = 0U;
    for (std::size_t k = 0U; k <= n; ++k) {
      total = add_mod(
          total,
          multiply_mod(tables.second_kind[n][k], input[k], modulus),
          modulus);
    }
    output[n] = total;
  }

  return output;
}

// Inverse Stirling transform:
//   output[n] = sum_{k=0}^n s(n,k) * input[k] (mod modulus),
// where s(n,k) are signed Stirling numbers of the first kind.
[[nodiscard]] inline std::vector<std::uint64_t>
inverse_stirling_transform(
    const std::vector<std::uint64_t>& input,
    const std::uint64_t modulus) {
  using algorithms::number_theory::multiply_mod;
  using stirling_transform_detail::add_mod;
  using stirling_transform_detail::validate_modulus;
  using stirling_transform_detail::validate_residues;

  validate_modulus(modulus);
  validate_residues(input, modulus);
  if (input.empty()) {
    return {};
  }

  const auto tables =
      stirling_transform_tables(input.size() - 1U, modulus);
  std::vector<std::uint64_t> output(input.size(), 0U);

  for (std::size_t n = 0U; n < input.size(); ++n) {
    std::uint64_t total = 0U;
    for (std::size_t k = 0U; k <= n; ++k) {
      total = add_mod(
          total,
          multiply_mod(
              tables.signed_first_kind[n][k], input[k], modulus),
          modulus);
    }
    output[n] = total;
  }

  return output;
}

}  // namespace algorithms::combinatorial
