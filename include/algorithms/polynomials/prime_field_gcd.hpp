#pragma once

#include "algorithms/number_theory/modular.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::polynomials {

using PrimeFieldPolynomial = std::vector<std::uint64_t>;

struct PolynomialDivisionResult {
  PrimeFieldPolynomial quotient;
  PrimeFieldPolynomial remainder;
};

struct PolynomialExtendedGcdResult {
  PrimeFieldPolynomial gcd;
  PrimeFieldPolynomial bezout_first;
  PrimeFieldPolynomial bezout_second;
};

namespace detail {

inline void validate_prime(std::uint64_t prime) {
  if (!algorithms::number_theory::is_prime(prime)) {
    throw std::invalid_argument("polynomial modulus must be prime");
  }
}

inline void validate_coefficients(const PrimeFieldPolynomial& polynomial,
                                  std::uint64_t prime) {
  for (const std::uint64_t coefficient : polynomial) {
    if (coefficient >= prime) {
      throw std::invalid_argument(
          "polynomial coefficient is not a canonical field residue");
    }
  }
}

inline void trim(PrimeFieldPolynomial& polynomial) {
  while (!polynomial.empty() && polynomial.back() == 0U) {
    polynomial.pop_back();
  }
}

[[nodiscard]] inline std::uint64_t add_mod(std::uint64_t first,
                                           std::uint64_t second,
                                           std::uint64_t prime) noexcept {
  if (first >= prime - second) {
    return first - (prime - second);
  }
  return first + second;
}

[[nodiscard]] inline std::uint64_t subtract_mod(std::uint64_t first,
                                                std::uint64_t second,
                                                std::uint64_t prime) noexcept {
  return first >= second ? first - second : prime - (second - first);
}

[[nodiscard]] inline PrimeFieldPolynomial subtract(
    const PrimeFieldPolynomial& first, const PrimeFieldPolynomial& second,
    std::uint64_t prime) {
  PrimeFieldPolynomial result(first.size() > second.size() ? first.size()
                                                            : second.size(),
                              0U);
  for (std::size_t index = 0; index < result.size(); ++index) {
    const std::uint64_t left = index < first.size() ? first[index] : 0U;
    const std::uint64_t right = index < second.size() ? second[index] : 0U;
    result[index] = subtract_mod(left, right, prime);
  }
  trim(result);
  return result;
}

[[nodiscard]] inline PrimeFieldPolynomial multiply(
    const PrimeFieldPolynomial& first, const PrimeFieldPolynomial& second,
    std::uint64_t prime) {
  if (first.empty() || second.empty()) {
    return {};
  }
  if (first.size() >
      std::numeric_limits<std::size_t>::max() - second.size() + 1U) {
    throw std::length_error("polynomial product size overflows size_t");
  }
  PrimeFieldPolynomial result(first.size() + second.size() - 1U, 0U);
  for (std::size_t left = 0; left < first.size(); ++left) {
    for (std::size_t right = 0; right < second.size(); ++right) {
      const std::uint64_t term = algorithms::number_theory::multiply_mod(
          first[left], second[right], prime);
      result[left + right] = add_mod(result[left + right], term, prime);
    }
  }
  trim(result);
  return result;
}

[[nodiscard]] inline PrimeFieldPolynomial scale(
    PrimeFieldPolynomial polynomial, std::uint64_t factor,
    std::uint64_t prime) {
  for (std::uint64_t& coefficient : polynomial) {
    coefficient = algorithms::number_theory::multiply_mod(coefficient, factor,
                                                           prime);
  }
  trim(polynomial);
  return polynomial;
}

}  // namespace detail

[[nodiscard]] inline PolynomialDivisionResult polynomial_divide_mod(
    PrimeFieldPolynomial dividend, PrimeFieldPolynomial divisor,
    std::uint64_t prime) {
  detail::validate_prime(prime);
  detail::validate_coefficients(dividend, prime);
  detail::validate_coefficients(divisor, prime);
  detail::trim(dividend);
  detail::trim(divisor);
  if (divisor.empty()) {
    throw std::invalid_argument("polynomial division by zero");
  }
  if (dividend.size() < divisor.size()) {
    return PolynomialDivisionResult{{}, std::move(dividend)};
  }

  PrimeFieldPolynomial quotient(dividend.size() - divisor.size() + 1U, 0U);
  const std::uint64_t divisor_leading_inverse =
      algorithms::number_theory::power_mod(divisor.back(), prime - 2U, prime);

  while (!dividend.empty() && dividend.size() >= divisor.size()) {
    const std::size_t shift = dividend.size() - divisor.size();
    const std::uint64_t factor = algorithms::number_theory::multiply_mod(
        dividend.back(), divisor_leading_inverse, prime);
    quotient[shift] = factor;
    for (std::size_t index = 0; index < divisor.size(); ++index) {
      const std::uint64_t term = algorithms::number_theory::multiply_mod(
          factor, divisor[index], prime);
      dividend[shift + index] =
          detail::subtract_mod(dividend[shift + index], term, prime);
    }
    detail::trim(dividend);
  }
  detail::trim(quotient);
  return PolynomialDivisionResult{std::move(quotient), std::move(dividend)};
}

[[nodiscard]] inline PolynomialExtendedGcdResult polynomial_extended_gcd_mod(
    PrimeFieldPolynomial first, PrimeFieldPolynomial second,
    std::uint64_t prime) {
  detail::validate_prime(prime);
  detail::validate_coefficients(first, prime);
  detail::validate_coefficients(second, prime);
  detail::trim(first);
  detail::trim(second);

  PrimeFieldPolynomial old_remainder = std::move(first);
  PrimeFieldPolynomial remainder = std::move(second);
  PrimeFieldPolynomial old_first{1U};
  PrimeFieldPolynomial current_first;
  PrimeFieldPolynomial old_second;
  PrimeFieldPolynomial current_second{1U};

  while (!remainder.empty()) {
    PolynomialDivisionResult division =
        polynomial_divide_mod(old_remainder, remainder, prime);
    PrimeFieldPolynomial next_first = detail::subtract(
        old_first, detail::multiply(division.quotient, current_first, prime),
        prime);
    PrimeFieldPolynomial next_second = detail::subtract(
        old_second, detail::multiply(division.quotient, current_second, prime),
        prime);

    old_remainder = std::move(remainder);
    remainder = std::move(division.remainder);
    old_first = std::move(current_first);
    current_first = std::move(next_first);
    old_second = std::move(current_second);
    current_second = std::move(next_second);
  }

  if (old_remainder.empty()) {
    return PolynomialExtendedGcdResult{{}, {1U}, {}};
  }

  const std::uint64_t inverse_leading = algorithms::number_theory::power_mod(
      old_remainder.back(), prime - 2U, prime);
  return PolynomialExtendedGcdResult{
      detail::scale(std::move(old_remainder), inverse_leading, prime),
      detail::scale(std::move(old_first), inverse_leading, prime),
      detail::scale(std::move(old_second), inverse_leading, prime)};
}

[[nodiscard]] inline PrimeFieldPolynomial polynomial_gcd_mod(
    PrimeFieldPolynomial first, PrimeFieldPolynomial second,
    std::uint64_t prime) {
  return polynomial_extended_gcd_mod(std::move(first), std::move(second), prime)
      .gcd;
}

}  // namespace algorithms::polynomials
