#pragma once

#include "algorithms/polynomials/prime_field_gcd.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace algorithms::polynomials {

struct SquareFreeFactor {
  PrimeFieldPolynomial factor;
  std::uint64_t multiplicity;
};

struct SquareFreeFactorization {
  std::uint64_t unit;
  std::vector<SquareFreeFactor> factors;
};

[[nodiscard]] inline PrimeFieldPolynomial polynomial_derivative_mod(
    PrimeFieldPolynomial polynomial, std::uint64_t prime) {
  detail::validate_prime(prime);
  detail::validate_coefficients(polynomial, prime);
  detail::trim(polynomial);
  if (polynomial.size() <= 1U) {
    return {};
  }

  static_assert(std::numeric_limits<std::size_t>::digits <=
                std::numeric_limits<std::uint64_t>::digits);
  PrimeFieldPolynomial derivative(polynomial.size() - 1U, 0U);
  for (std::size_t exponent = 1U; exponent < polynomial.size(); ++exponent) {
    const auto exponent_mod_prime =
        static_cast<std::uint64_t>(exponent) % prime;
    derivative[exponent - 1U] = algorithms::number_theory::multiply_mod(
        polynomial[exponent], exponent_mod_prime, prime);
  }
  detail::trim(derivative);
  return derivative;
}

namespace square_free_detail {

[[nodiscard]] inline bool is_one(const PrimeFieldPolynomial& polynomial) {
  return polynomial.size() == 1U && polynomial.front() == 1U;
}

[[nodiscard]] inline PrimeFieldPolynomial exact_quotient(
    const PrimeFieldPolynomial& dividend, const PrimeFieldPolynomial& divisor,
    std::uint64_t prime) {
  auto division = polynomial_divide_mod(dividend, divisor, prime);
  if (!division.remainder.empty()) {
    throw std::logic_error(
        "square-free decomposition encountered a non-exact quotient");
  }
  return std::move(division.quotient);
}

[[nodiscard]] inline PrimeFieldPolynomial pth_root(
    const PrimeFieldPolynomial& polynomial, std::uint64_t prime) {
  static_assert(std::numeric_limits<std::size_t>::digits <=
                std::numeric_limits<std::uint64_t>::digits);
  PrimeFieldPolynomial root;
  for (std::size_t exponent = 0U; exponent < polynomial.size(); ++exponent) {
    const std::uint64_t coefficient = polynomial[exponent];
    if (coefficient == 0U) {
      continue;
    }
    const auto wide_exponent = static_cast<std::uint64_t>(exponent);
    if (wide_exponent % prime != 0U) {
      throw std::logic_error(
          "derivative-zero polynomial is not a p-th power");
    }
    const auto root_exponent = wide_exponent / prime;
    const auto root_index = static_cast<std::size_t>(root_exponent);
    if (root.size() <= root_index) {
      root.resize(root_index + 1U, 0U);
    }
    // Frobenius is the identity on coefficients of F_p: a^p = a.
    root[root_index] = coefficient;
  }
  detail::trim(root);
  return root;
}

inline void decompose_monic(const PrimeFieldPolynomial& monic,
                            std::uint64_t prime,
                            std::vector<SquareFreeFactor>& output) {
  if (is_one(monic)) {
    return;
  }

  const auto derivative = polynomial_derivative_mod(monic, prime);
  PrimeFieldPolynomial repeated = polynomial_gcd_mod(monic, derivative, prime);
  PrimeFieldPolynomial remaining = exact_quotient(monic, repeated, prime);

  std::uint64_t multiplicity = 1U;
  while (!is_one(remaining)) {
    const PrimeFieldPolynomial overlap =
        polynomial_gcd_mod(remaining, repeated, prime);
    PrimeFieldPolynomial layer = exact_quotient(remaining, overlap, prime);
    if (!is_one(layer)) {
      output.push_back(SquareFreeFactor{std::move(layer), multiplicity});
    }

    remaining = overlap;
    repeated = exact_quotient(repeated, overlap, prime);
    if (multiplicity == std::numeric_limits<std::uint64_t>::max()) {
      throw std::length_error("square-free multiplicity overflows uint64_t");
    }
    ++multiplicity;
  }

  if (!is_one(repeated)) {
    const PrimeFieldPolynomial root = pth_root(repeated, prime);
    std::vector<SquareFreeFactor> nested;
    decompose_monic(root, prime, nested);
    for (SquareFreeFactor& factor : nested) {
      if (factor.multiplicity >
          std::numeric_limits<std::uint64_t>::max() / prime) {
        throw std::length_error("square-free multiplicity overflows uint64_t");
      }
      factor.multiplicity *= prime;
      output.push_back(std::move(factor));
    }
  }
}

}  // namespace square_free_detail

[[nodiscard]] inline SquareFreeFactorization
polynomial_square_free_factorization_mod(PrimeFieldPolynomial polynomial,
                                          std::uint64_t prime) {
  detail::validate_prime(prime);
  detail::validate_coefficients(polynomial, prime);
  detail::trim(polynomial);
  if (polynomial.empty()) {
    throw std::invalid_argument(
        "square-free factorization of the zero polynomial is undefined");
  }

  const std::uint64_t unit = polynomial.back();
  const std::uint64_t inverse_leading =
      algorithms::number_theory::power_mod(unit, prime - 2U, prime);
  polynomial = detail::scale(std::move(polynomial), inverse_leading, prime);

  std::vector<SquareFreeFactor> factors;
  square_free_detail::decompose_monic(polynomial, prime, factors);
  std::sort(factors.begin(), factors.end(),
            [](const SquareFreeFactor& first, const SquareFreeFactor& second) {
              return first.multiplicity < second.multiplicity;
            });

  std::vector<SquareFreeFactor> merged;
  for (SquareFreeFactor& factor : factors) {
    if (!merged.empty() &&
        merged.back().multiplicity == factor.multiplicity) {
      merged.back().factor =
          detail::multiply(merged.back().factor, factor.factor, prime);
    } else {
      merged.push_back(std::move(factor));
    }
  }

  return SquareFreeFactorization{unit, std::move(merged)};
}

}  // namespace algorithms::polynomials
