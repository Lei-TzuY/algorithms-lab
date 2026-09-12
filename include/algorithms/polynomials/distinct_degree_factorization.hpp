#pragma once

#include "algorithms/polynomials/square_free_factorization.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::polynomials {

struct DistinctDegreeFactor {
  PrimeFieldPolynomial factor;
  std::size_t irreducible_degree;
};

struct DistinctDegreeFactorization {
  std::uint64_t unit;
  std::vector<DistinctDegreeFactor> factors;
};

namespace distinct_degree_detail {

[[nodiscard]] inline bool is_one(const PrimeFieldPolynomial& polynomial) {
  return polynomial.size() == 1U && polynomial.front() == 1U;
}

[[nodiscard]] inline PrimeFieldPolynomial remainder(
    PrimeFieldPolynomial polynomial, const PrimeFieldPolynomial& modulus,
    std::uint64_t prime) {
  return polynomial_divide_mod(std::move(polynomial), modulus, prime).remainder;
}

[[nodiscard]] inline PrimeFieldPolynomial multiply_reduced(
    const PrimeFieldPolynomial& first, const PrimeFieldPolynomial& second,
    const PrimeFieldPolynomial& modulus, std::uint64_t prime) {
  return remainder(detail::multiply(first, second, prime), modulus, prime);
}

[[nodiscard]] inline PrimeFieldPolynomial power_reduced(
    PrimeFieldPolynomial base, std::uint64_t exponent,
    const PrimeFieldPolynomial& modulus, std::uint64_t prime) {
  PrimeFieldPolynomial result{1U};
  base = remainder(std::move(base), modulus, prime);
  while (exponent != 0U) {
    if ((exponent & 1U) != 0U) {
      result = multiply_reduced(result, base, modulus, prime);
    }
    exponent >>= 1U;
    if (exponent != 0U) {
      base = multiply_reduced(base, base, modulus, prime);
    }
  }
  return result;
}

[[nodiscard]] inline PrimeFieldPolynomial exact_quotient(
    const PrimeFieldPolynomial& dividend, const PrimeFieldPolynomial& divisor,
    std::uint64_t prime) {
  auto division = polynomial_divide_mod(dividend, divisor, prime);
  if (!division.remainder.empty()) {
    throw std::logic_error(
        "distinct-degree factorization encountered a non-exact quotient");
  }
  return std::move(division.quotient);
}

}  // namespace distinct_degree_detail

[[nodiscard]] inline DistinctDegreeFactorization
polynomial_distinct_degree_factorization_mod(PrimeFieldPolynomial polynomial,
                                              std::uint64_t prime) {
  detail::validate_prime(prime);
  detail::validate_coefficients(polynomial, prime);
  detail::trim(polynomial);
  if (polynomial.empty()) {
    throw std::invalid_argument(
        "distinct-degree factorization of the zero polynomial is undefined");
  }

  const std::uint64_t unit = polynomial.back();
  if (polynomial.size() == 1U) {
    return DistinctDegreeFactorization{unit, {}};
  }

  const std::uint64_t inverse_leading =
      algorithms::number_theory::power_mod(unit, prime - 2U, prime);
  PrimeFieldPolynomial remaining =
      detail::scale(std::move(polynomial), inverse_leading, prime);

  const PrimeFieldPolynomial derivative =
      polynomial_derivative_mod(remaining, prime);
  if (!distinct_degree_detail::is_one(
          polynomial_gcd_mod(remaining, derivative, prime))) {
    throw std::invalid_argument(
        "distinct-degree factorization requires a square-free polynomial");
  }

  PrimeFieldPolynomial frobenius{0U, 1U};
  const PrimeFieldPolynomial x{0U, 1U};
  std::vector<DistinctDegreeFactor> factors;
  std::size_t degree = 1U;

  while (!distinct_degree_detail::is_one(remaining)) {
    const std::size_t remaining_degree = remaining.size() - 1U;
    if (degree > remaining_degree / 2U) {
      factors.push_back(
          DistinctDegreeFactor{std::move(remaining), remaining_degree});
      break;
    }

    frobenius = distinct_degree_detail::power_reduced(
        std::move(frobenius), prime, remaining, prime);
    const PrimeFieldPolynomial difference =
        detail::subtract(frobenius, x, prime);
    PrimeFieldPolynomial group =
        polynomial_gcd_mod(remaining, difference, prime);

    if (!distinct_degree_detail::is_one(group)) {
      factors.push_back(DistinctDegreeFactor{group, degree});
      remaining = distinct_degree_detail::exact_quotient(
          remaining, group, prime);
      if (distinct_degree_detail::is_one(remaining)) {
        break;
      }
      frobenius = distinct_degree_detail::remainder(
          std::move(frobenius), remaining, prime);
    }

    ++degree;
  }

  return DistinctDegreeFactorization{unit, std::move(factors)};
}

}  // namespace algorithms::polynomials
