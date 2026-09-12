#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace algorithms::number_theory {

struct FieldSample {
  std::uint64_t x;
  std::uint64_t y;

  friend bool operator==(const FieldSample&, const FieldSample&) = default;
};

// Interpolate the unique polynomial of degree < samples.size() over F_p.
// Coefficients are returned low degree first and canonicalized to [0, p).
// prime_modulus must be prime; sample x coordinates must be distinct modulo p.
[[nodiscard]] std::vector<std::uint64_t> interpolate_prime_field(
    std::span<const FieldSample> samples, std::uint64_t prime_modulus);

// Horner evaluation over F_p using the repository's overflow-safe modular
// multiplication. An empty coefficient list represents the zero polynomial.
[[nodiscard]] std::uint64_t evaluate_polynomial_mod(
    std::span<const std::uint64_t> coefficients, std::uint64_t x,
    std::uint64_t prime_modulus);

}  // namespace algorithms::number_theory
