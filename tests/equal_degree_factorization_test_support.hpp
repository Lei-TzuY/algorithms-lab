#pragma once

#include "algorithms/number_theory/modular.hpp"
#include "prime_field_factorization_test_support.hpp"

#include <cstddef>
#include <cstdint>

namespace equal_degree_test_detail {

using algorithms::polynomials::PrimeFieldPolynomial;

[[nodiscard]] inline std::uint64_t add_mod_reconstruction(
    std::uint64_t first, std::uint64_t second,
    std::uint64_t prime) noexcept {
  return first >= prime - second ? first - (prime - second) : first + second;
}

[[nodiscard]] inline PrimeFieldPolynomial multiply_reconstruction(
    const PrimeFieldPolynomial& first, const PrimeFieldPolynomial& second,
    std::uint64_t prime) {
  if (first.empty() || second.empty()) {
    return {};
  }
  PrimeFieldPolynomial result(first.size() + second.size() - 1U, 0U);
  for (std::size_t left = 0U; left < first.size(); ++left) {
    for (std::size_t right = 0U; right < second.size(); ++right) {
      const std::uint64_t term = algorithms::number_theory::multiply_mod(
          first[left], second[right], prime);
      result[left + right] =
          add_mod_reconstruction(result[left + right], term, prime);
    }
  }
  distinct_degree_test_detail::trim(result);
  return result;
}

[[nodiscard]] inline PrimeFieldPolynomial scale_reconstruction(
    PrimeFieldPolynomial polynomial, std::uint64_t unit,
    std::uint64_t prime) {
  for (std::uint64_t& coefficient : polynomial) {
    coefficient =
        algorithms::number_theory::multiply_mod(coefficient, unit, prime);
  }
  distinct_degree_test_detail::trim(polynomial);
  return polynomial;
}

}  // namespace equal_degree_test_detail
