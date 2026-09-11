#pragma once

#include "algorithms/number_theory/modular.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>

namespace algorithms::number_theory {

// Returns the smaller square root r in [0, prime_modulus) such that
// r^2 == value (mod prime_modulus), or std::nullopt when no root exists.
// prime_modulus must be prime and is validated through is_prime().
//
// The implementation is deterministic Tonelli-Shanks for odd primes, with
// explicit p=2 and zero-residue boundaries. It reuses the repository's
// overflow-safe modular arithmetic; no wider integer type is required.
[[nodiscard]] inline std::optional<std::uint64_t> tonelli_shanks_square_root(
    std::uint64_t value, std::uint64_t prime_modulus) {
  if (!is_prime(prime_modulus)) {
    throw std::invalid_argument("modular square-root modulus must be prime");
  }

  value %= prime_modulus;
  if (value == 0U || prime_modulus == 2U) {
    return value;
  }

  const std::uint64_t legendre =
      power_mod(value, (prime_modulus - 1U) / 2U, prime_modulus);
  if (legendre != 1U) {
    return std::nullopt;
  }

  if ((prime_modulus & 3U) == 3U) {
    const std::uint64_t root =
        power_mod(value, prime_modulus / 4U + 1U, prime_modulus);
    return std::min(root, prime_modulus - root);
  }

  std::uint64_t odd_part = prime_modulus - 1U;
  unsigned int powers_of_two = 0U;
  while ((odd_part & 1U) == 0U) {
    odd_part >>= 1U;
    ++powers_of_two;
  }

  std::uint64_t non_residue = 2U;
  while (non_residue < prime_modulus &&
         power_mod(non_residue, (prime_modulus - 1U) / 2U, prime_modulus) !=
             prime_modulus - 1U) {
    ++non_residue;
  }
  if (non_residue == prime_modulus) {
    throw std::logic_error("prime field unexpectedly lacks a quadratic non-residue");
  }

  std::uint64_t c = power_mod(non_residue, odd_part, prime_modulus);
  std::uint64_t root =
      power_mod(value, odd_part / 2U + 1U, prime_modulus);
  std::uint64_t residue = power_mod(value, odd_part, prime_modulus);
  unsigned int active_power = powers_of_two;

  while (residue != 1U) {
    unsigned int exponent = 0U;
    std::uint64_t squared = residue;
    while (squared != 1U && exponent < active_power) {
      squared = multiply_mod(squared, squared, prime_modulus);
      ++exponent;
    }
    if (exponent == active_power) {
      throw std::logic_error("Tonelli-Shanks invariant failed for a quadratic residue");
    }

    const unsigned int shift = active_power - exponent - 1U;
    const std::uint64_t power = std::uint64_t{1} << shift;
    const std::uint64_t factor = power_mod(c, power, prime_modulus);
    root = multiply_mod(root, factor, prime_modulus);
    const std::uint64_t factor_squared =
        multiply_mod(factor, factor, prime_modulus);
    residue = multiply_mod(residue, factor_squared, prime_modulus);
    c = factor_squared;
    active_power = exponent;
  }

  return std::min(root, prime_modulus - root);
}

}  // namespace algorithms::number_theory
