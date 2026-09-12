#pragma once

#include "algorithms/polynomials/distinct_degree_factorization.hpp"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace algorithms::polynomials {

// Exact arithmetic in F_p[x] / (modulus), where modulus is monic irreducible.
// Elements use the unique low-to-high polynomial representative of degree less
// than degree(). Inputs may contain trailing zeros, but every returned element
// is canonical and trimmed. Coefficients must already be residues in [0,p).
class PrimeFieldExtension {
 public:
  using Element = PrimeFieldPolynomial;

  PrimeFieldExtension(std::uint64_t prime, PrimeFieldPolynomial modulus)
      : prime_(prime), modulus_(std::move(modulus)) {
    detail::validate_prime(prime_);
    detail::validate_coefficients(modulus_, prime_);
    detail::trim(modulus_);
    if (modulus_.size() < 2U) {
      throw std::invalid_argument(
          "extension-field modulus must have positive degree");
    }
    if (modulus_.back() != 1U) {
      throw std::invalid_argument("extension-field modulus must be monic");
    }
    degree_ = modulus_.size() - 1U;

    DistinctDegreeFactorization ddf;
    try {
      ddf = polynomial_distinct_degree_factorization_mod(modulus_, prime_);
    } catch (const std::invalid_argument&) {
      throw std::invalid_argument(
          "extension-field modulus must be irreducible over F_p");
    }
    if (ddf.unit != 1U || ddf.factors.size() != 1U ||
        ddf.factors.front().irreducible_degree != degree_ ||
        ddf.factors.front().factor != modulus_) {
      throw std::invalid_argument(
          "extension-field modulus must be irreducible over F_p");
    }
  }

  [[nodiscard]] std::uint64_t prime() const noexcept { return prime_; }
  [[nodiscard]] std::size_t degree() const noexcept { return degree_; }
  [[nodiscard]] const PrimeFieldPolynomial& modulus() const noexcept {
    return modulus_;
  }

  [[nodiscard]] Element zero() const { return {}; }
  [[nodiscard]] Element one() const { return {1U}; }

  [[nodiscard]] Element add(Element first, Element second) const {
    first = canonicalize(std::move(first));
    second = canonicalize(std::move(second));
    Element result(first.size() > second.size() ? first.size() : second.size(),
                   0U);
    for (std::size_t index = 0U; index < result.size(); ++index) {
      const std::uint64_t left = index < first.size() ? first[index] : 0U;
      const std::uint64_t right = index < second.size() ? second[index] : 0U;
      result[index] = detail::add_mod(left, right, prime_);
    }
    detail::trim(result);
    return result;
  }

  [[nodiscard]] Element subtract(Element first, Element second) const {
    first = canonicalize(std::move(first));
    second = canonicalize(std::move(second));
    return detail::subtract(first, second, prime_);
  }

  [[nodiscard]] Element multiply(Element first, Element second) const {
    first = canonicalize(std::move(first));
    second = canonicalize(std::move(second));
    return reduce(detail::multiply(first, second, prime_));
  }

  [[nodiscard]] Element inverse(Element value) const {
    value = canonicalize(std::move(value));
    if (value.empty()) {
      throw std::domain_error("zero has no multiplicative inverse");
    }
    PolynomialExtendedGcdResult extended =
        polynomial_extended_gcd_mod(value, modulus_, prime_);
    if (extended.gcd != PrimeFieldPolynomial{1U}) {
      throw std::logic_error(
          "nonzero extension-field element unexpectedly lacks an inverse");
    }
    return reduce(std::move(extended.bezout_first));
  }

  [[nodiscard]] Element divide(Element numerator, Element denominator) const {
    numerator = canonicalize(std::move(numerator));
    return multiply(std::move(numerator), inverse(std::move(denominator)));
  }

  [[nodiscard]] Element power(Element base, std::uint64_t exponent) const {
    base = canonicalize(std::move(base));
    Element result{1U};
    while (exponent != 0U) {
      if ((exponent & 1U) != 0U) {
        result = multiply(std::move(result), base);
      }
      exponent >>= 1U;
      if (exponent != 0U) {
        base = multiply(base, base);
      }
    }
    return result;
  }

 private:
  [[nodiscard]] Element canonicalize(Element value) const {
    detail::validate_coefficients(value, prime_);
    detail::trim(value);
    if (value.size() > degree_) {
      throw std::invalid_argument(
          "extension-field element is not the canonical degree-bounded representative");
    }
    return value;
  }

  [[nodiscard]] Element reduce(Element polynomial) const {
    detail::trim(polynomial);
    if (polynomial.size() <= degree_) {
      return polynomial;
    }
    return polynomial_divide_mod(std::move(polynomial), modulus_, prime_)
        .remainder;
  }

  std::uint64_t prime_;
  PrimeFieldPolynomial modulus_;
  std::size_t degree_ = 0U;
};

}  // namespace algorithms::polynomials
